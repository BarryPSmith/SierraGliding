using System;
using System.Collections.Generic;
using System.Text;

namespace core_Receiver.Packets
{
    class MessageListResponse
    {
        public class PacketInfo
        {
            public PacketTypes recordType;
            public byte stationID;
            public DateTimeOffset timestamp;
            public UInt16 address;
        }

        public MessageListResponse(Span<byte> data)
        {
            const int recordSize = 8; 
            for (int i = 0; i <= data.Length - recordSize; i += recordSize)
            {
                Packets.Add(new PacketInfo
                {
                    recordType = (PacketTypes)data[i],
                    stationID = data[i + 1],
                    timestamp = DateTimeOffset.FromUnixTimeSeconds(BitConverter.ToUInt32(data.Slice(i + 2))),
                    address = BitConverter.ToUInt16(data.Slice(i + 6))
                });
            }
        }

        public List<PacketInfo> Packets { get; } = new List<PacketInfo>();

        public override string ToString()
        {
            return "Search Result: " + Environment.NewLine +
                Packets.ToCsv(
                    p => $" {p.address} : {p.recordType.ToChar()} {p.stationID.ToChar()} {p.timestamp.ToLocalTime()}",
                    Environment.NewLine);
        }
    }
}
