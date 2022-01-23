using System;
using System.Collections.Generic;
using System.Text;

namespace core_Receiver.Packets
{
    class Packet
    {
        public string CallSign;
        public byte sendingStation;
        public byte uniqueID;
        public PacketTypes type;

        public object packetData;
        public byte[] originalData;
        public Exception exception;

        public override string ToString()
        {
            string dataString = SafeDataString;
            return $"{IdentityString} {RSSI,6:F1} {SNR,4:F1}{dataString}";
        }

        public string IdentityString
        {
            get
            {
                return $"{CallSign} {sendingStation.ToChar()} {uniqueID:X2} {type.ToChar()}";
            }
        }

        public string SafeDataString
        {
            get
            {
                string ret;
                if (GetDataString != null)
                    ret = $" {GetDataString?.Invoke(packetData)}";
                else if (packetData != null)
                    ret = $" {packetData}";
                else
                    ret = "";
                if (exception != null)
                    ret += $"!!{exception}!!";
                return ret;
            }
        }

        internal Func<object, string> GetDataString;
        
        public double SNR { get; set; }
        public double RSSI { get; set; }
    }
}
