using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace core_Receiver.Packets
{    
    /// <summary>
     /// This should be kept in sync with handleQueryVolatileCommand in Commands.cpp
     /// </summary>
    class QueryVolatileResponse : QueryResponse
    {
        public QueryVolatileResponse(Span<byte> data)
            : base(data, out int consumed)
        {

            int i = 0;
            bool constantArraySize = VersionNumber >= new Version(2, 2);
            bool hasMarkers = VersionNumber < new Version(2, 5);
            data = data.Slice(consumed);
            using MemoryStream ms = new MemoryStream();
            ms.Write(data);
            ms.Seek(0, SeekOrigin.Begin);
            BinaryReader br = new BinaryReader(ms, Encoding.ASCII);
            void CheckMarker(char marker)
            {
                if (hasMarkers && br.ReadChar() != marker)
                    throw new InvalidDataException($"Did not find expected '{marker}' in volatile query");
            }
            void CheckZeroMarker(bool afterArray = false)
            {
                int marker = 0;
                if ((!afterArray || (constantArraySize || i == ArraySize))
                    && hasMarkers && br.ReadByte() != marker)
                    throw new InvalidDataException($"Did not find expected '{marker}' in volatile query");
            }
            CheckMarker('M');
            Millis = br.ReadUInt32();
            if (VersionNumber >= new Version(2, 1))
                LastPingAge = Millis - br.ReadUInt32();
            if (VersionNumber < new System.Version(2, 5))
            {
                OverrideDuration = br.ReadUInt32();
                if (OverrideDuration > 0)
                {
                    OverrideStart = br.ReadUInt32();
                    OverrideShort = br.ReadBoolean();
                }
            }
            CheckZeroMarker();
            CheckMarker('S');
            for (i = 0; i < ArraySize; i++)
            {
                byte stationID = br.ReadByte();
                if (!constantArraySize && stationID == 0)
                    break;
                UInt32 seenMillis = br.ReadUInt32();
                var age = Millis - seenMillis;
                if (stationID != 0)
                    RecentlySeenStations.Add((stationID, age, DateTimeOffset.Now - TimeSpan.FromMilliseconds(age)));
            }
            CheckZeroMarker(true);
            CheckMarker('C');
            for (i = 0; i < ArraySize; i++)
            {
                byte commandID = br.ReadByte();
                if (!constantArraySize && commandID == 0)
                    break;
                if (commandID != 0)
                    RecentlyHandledCommands.Add(commandID);
            }
            CheckZeroMarker(true);
            CheckMarker('R');
            for (i = 0; i < ArraySize; i++)
            {
                byte type = br.ReadByte();
                if (!constantArraySize && type == 0)
                    break;
                var packet = new Packet
                {
                    type = (PacketTypes)type,
                    uniqueID = br.ReadByte(),
                    sendingStation = br.ReadByte()
                };
                if (type != 0)
                    RecentlyRelayedPackets.Add(packet);
            }
            CheckZeroMarker(true);
            CheckMarker('M');

            CRCErrorRate = br.ReadUInt16() / (double)0xFFFF;
            DroppedPacketRate = br.ReadUInt16() / (double)0xFFFF;
            AverageDelayTime = br.ReadUInt32();

            if (VersionNumber >= new Version(2, 3))
            {
                CheckMarker('S');
                uint timestamp = br.ReadUInt32();
                Time = DateTimeOffset.FromUnixTimeSeconds(timestamp).ToLocalTime();
                MemoryLowWater = br.ReadUInt16();
                FreeMemory = br.ReadUInt16();
            }

            if (VersionNumber >= new Version(2, 4))
            {
                CheckMarker('D');
                DatabaseHeaderAdd = br.ReadUInt32();
                DatabaseDataAdd = br.ReadUInt32();
                DatabaseCycle = br.ReadByte();
            }
        }

        public UInt32 Millis { get; set; }

        public UInt32? LastPingAge { get; set; }
        public UInt32 OverrideDuration { get; set; }
        public UInt32? OverrideStart { get; set; }
        public bool OverrideShort { get; set; }
        public List<(byte ID, UInt32 age, DateTimeOffset date)> RecentlySeenStations { get; set; } = new List<(byte ID, uint age, DateTimeOffset date)>();

        public List<byte> RecentlyHandledCommands { get; set; } = new List<byte>();
        public List<Packet> RecentlyRelayedPackets { get; set; } = new List<Packet>();

        public double CRCErrorRate { get; set; }
        public double DroppedPacketRate { get; set; }
        public UInt32 AverageDelayTime { get; set; }

        public DateTimeOffset? Time { get; set; }
        public UInt16? MemoryLowWater { get; set; }
        public UInt16? FreeMemory { get; set; }

        public UInt32 DatabaseHeaderAdd { get; set; }
        public UInt32 DatabaseDataAdd { get; set; }
        public byte DatabaseCycle { get; set; }
        public override string ToString()
        {
            var overrideString = OverrideDuration > 0 ? $"Override (Remaining:{OverrideDuration - (Millis - OverrideStart)}, short:{OverrideShort}), " : "";
            var pingString = LastPingAge.HasValue ? $"Ping Age:{LastPingAge}, " : "";
            return $"VOLATILE Version:{Version} Millis:{Millis}, {pingString}{overrideString}" + Environment.NewLine +
                $" Recently Seen:({RecentlySeenStations.ToCsv(rss => $"{(char)rss.ID}:{rss.age}")})" + Environment.NewLine +
                $" Recent Commands:({RecentlyHandledCommands.ToCsv(b => $"{b}:{(char)b}")})" + Environment.NewLine +
                $" Recently Relayed:({RecentlyRelayedPackets.ToCsv(p => p.IdentityString)})" + Environment.NewLine +
                $" CRC Error Rate:{CRCErrorRate:P1}, Dropped Packet Rate:{DroppedPacketRate:P1}, Average Delay:{AverageDelayTime} us" + Environment.NewLine +
                $" Station Time: {Time?.LocalDateTime}, Memory Low Water: {MemoryLowWater}, Free Memory: {FreeMemory}" + Environment.NewLine +
                $" DB Header: {DatabaseHeaderAdd}, DB Data: {DatabaseDataAdd}, DB Cycle: {DatabaseCycle}";
        }
    }
}
