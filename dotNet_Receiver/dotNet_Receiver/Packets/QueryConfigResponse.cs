using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace core_Receiver.Packets
{
    /// <summary>
    /// This should keep in sync with the PermanentVariables of PermanentVariables.h
    /// </summary>
    class QueryConfigResponse : QueryResponse
    {
        public QueryConfigResponse(Span<byte> inputData)
            : base(inputData, out var consumed)
        {
            inputData = inputData.Slice(consumed);
            using MemoryStream ms = new MemoryStream();
            ms.Write(inputData);
            ms.Seek(0, SeekOrigin.Begin);
            BinaryReader br = new BinaryReader(ms);
            Initialised = br.ReadBoolean();
            ShortInterval = br.ReadUInt32();
            LongInterval = br.ReadUInt32();
            BatteryThreshold_mV = br.ReadUInt16();
            BatteryUpperThres_mV = br.ReadUInt16();
            DemandRelay = br.ReadBoolean();
            StationsToRelayCommands = new List<byte>();
            for (byte b = br.ReadByte(); b != 0; b++)
                StationsToRelayCommands.Add(b);
            StationsToRelayWeather = new List<byte>();
            for (byte b = br.ReadByte(); b != 0; b = br.ReadByte())
                StationsToRelayWeather.Add(b);
        }

        public bool Initialised { get; set; }
        public UInt32 ShortInterval { get; set; }
        public UInt32 LongInterval { get; set; }
        public UInt16 BatteryThreshold_mV { get; set; }
        public UInt16 BatteryUpperThres_mV { get; set; }
        public bool DemandRelay { get; set; }
        IList<byte> StationsToRelayCommands { get; set; }
        IList<byte> StationsToRelayWeather { get; set; }

        public override string ToString()
        {
            return $"CONFIG Version:{Version}, S Interval:{ShortInterval}, L Interval:{LongInterval}, Batt Thresh:{BatteryThreshold_mV} " +
                $"Batt U Thresh:{BatteryUpperThres_mV}, Demand Relay:{DemandRelay}, " +
                $"Relay Commands:({StationsToRelayCommands.ToCsv(b => Packet.GetChar(b).ToString())}) " +
                $"Relay Weather:({StationsToRelayWeather.ToCsv(b => Packet.GetChar(b).ToString())})";
        }
    }
}
