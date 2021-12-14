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
        const int TypeArraySize = 7;
        public QueryConfigResponse(Span<byte> inputData)
            : base(inputData, out var consumed)
        {
            inputData = inputData.Slice(consumed);
            using MemoryStream ms = new MemoryStream();
            ms.Write(inputData);
            ms.Seek(0, SeekOrigin.Begin);
            BinaryReader br = new BinaryReader(ms);
            Initialised = br.ReadBoolean();
            if (VersionNumber >= new Version(2, 1))
                StationID = br.ReadByte().ToChar();
            ShortInterval = br.ReadUInt32();
            LongInterval = br.ReadUInt32();
            BatteryThreshold_mV = br.ReadUInt16();
            BatteryEmergencyThres_mV = br.ReadUInt16();
            DemandRelay = br.ReadBoolean();
            StationsToRelayCommands = new List<byte>();
            for (int i = 0; i < ArraySize; i++)
            {
                var stationID = br.ReadByte();
                if (stationID != 0)
                    StationsToRelayCommands.Add(stationID);
            }
            StationsToRelayWeather = new List<byte>();
            for (int i = 0; i < ArraySize; i++)
            {
                var stationID = br.ReadByte();
                if (stationID != 0)
                    StationsToRelayWeather.Add(stationID);
            }
            Frequency_Hz = br.ReadUInt32();
            Bandwidth_Hz = br.ReadUInt16() * 100;
            TxPower = br.ReadInt16();
            SpreadingFactor = br.ReadByte();
            CSMA_P = br.ReadByte();
            CSMA_Timeslot = br.ReadUInt32();
            OutboundPreambleLength = br.ReadUInt16();
            TsOffset = br.ReadSByte();
            TsGain = br.ReadByte();
            WdCalib1 = br.ReadInt16();
            WdCalib2 = br.ReadInt16();
            if (VersionNumber >= new Version(2, 2))
            {
                ChargeVoltage_mV = br.ReadUInt16();
                ChargeResponseRate = br.ReadUInt16();
                SafeFreezingChargeLevel_mV = br.ReadUInt16();
                SafeFreezingPwm = br.ReadByte();
            }
            if (VersionNumber >= new Version(2, 3))
            {
                for (int i = 0; i < TypeArraySize; i++)
                {
                    var recordType = br.ReadByte();
                    if (recordType != 0)
                        MessageRecordTypes.Add(recordType.ToChar());
                }
                NonRelayRecording = br.ReadBoolean();
            }
            if (VersionNumber >= new Version(2, 5))
            {
                InboundPreambleLength = br.ReadUInt16();
                BoostedRx = br.ReadBoolean();
                br.ReadByte(); //Stasis requested;
                try
                {
                    CodingRate = br.ReadByte();
                }
                catch 
                {
                    // Bad versioning: Multiple versions of 2.5 out there, some of them don't have adjustable CR.
                }
            }
        }

        public bool Initialised { get; set; }
        public char StationID { get; set; }
        public UInt32 ShortInterval { get; set; }
        public UInt32 LongInterval { get; set; }
        public UInt16 BatteryThreshold_mV { get; set; }
        public UInt16 BatteryEmergencyThres_mV { get; set; }
        public bool DemandRelay { get; set; }
        public IList<byte> StationsToRelayCommands { get; set; }
        public IList<byte> StationsToRelayWeather { get; set; }
        public UInt32 Frequency_Hz { get; set; }
        public int Bandwidth_Hz { get; set; }
        public Int16 TxPower { get; set; }
        public byte SpreadingFactor { get; set; }
        public byte CodingRate { get; set; } = 5;
        public byte CSMA_P { get; set; }
        public UInt32 CSMA_Timeslot { get; set; }
        public UInt16 OutboundPreambleLength { get; set; }
        public sbyte TsOffset { get; set; }
        public byte TsGain { get; set; }

        public sbyte WdCalib1x => (sbyte)(WdCalib1 >> 8);
        public sbyte WdCalib1y => (sbyte)(WdCalib1 & 0xFF);
        public Int16 WdCalib1 { get; set; }
        public Int16 WdCalib2 { get; set; }

        public UInt16 ChargeVoltage_mV { get; set; }
        public UInt16 ChargeResponseRate { get; set; }
        public UInt16 SafeFreezingChargeLevel_mV { get; set; }
        public byte SafeFreezingPwm { get; set; }
        public List<char> MessageRecordTypes { get; set; } = new List<char>();
        public bool NonRelayRecording { get; set; }

        public UInt16 InboundPreambleLength { get; set; } = 8;

        public bool BoostedRx { get; set; } = false;
             
        public override string ToString()
        {
            return $"CONFIG Version:{Version}" + Environment.NewLine +
                $" S Interval:{ShortInterval}, L Interval:{LongInterval}, Batt Thresh:{BatteryThreshold_mV} " +
                $"Batt E Thresh:{BatteryEmergencyThres_mV}, Demand Relay:{DemandRelay}" + Environment.NewLine +
                $" Relay Commands:({StationsToRelayCommands.ToCsv(b => (b.ToChar()).ToString())}) " + Environment.NewLine +
                $" Relay Weather:({StationsToRelayWeather.ToCsv(b => (b.ToChar()).ToString())})" + Environment.NewLine +
                $" Freq:{Frequency_Hz / 1.0E6:F3} Hz, BW:{Bandwidth_Hz/1.0E3:F3} kHz, TxPower:{TxPower}, SF:{SpreadingFactor}, CSMA_P:{CSMA_P}, CSMA_Slot:{CSMA_Timeslot} uS, Coding Rate: {CodingRate}" + Environment.NewLine +
                $" TsOffset:{TsOffset}, TSGain:{TsGain}, WdCalibMin:({WdCalib1x}, {WdCalib1y}), WdCalibMax:{WdCalib2}" + Environment.NewLine +
                $" ChargeV: {ChargeVoltage_mV} mV, ChargeResponsitivity: {ChargeResponseRate}, FreezingChargeV: {SafeFreezingChargeLevel_mV} mV, FreezingPwm: {SafeFreezingPwm}" + Environment.NewLine +
                $" Record Types: ({MessageRecordTypes.ToCsv()}) Non Relay Records: {NonRelayRecording}" + Environment.NewLine +
                $" Outbound Preamble:{OutboundPreambleLength}, Inbound Preamble {InboundPreambleLength}" + Environment.NewLine +
                $" Boosted RX: {BoostedRx}";
        }
    }
}
