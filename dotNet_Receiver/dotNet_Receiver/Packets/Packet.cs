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

        public override string ToString()
        {
            string dataString;
            if (GetDataString != null)
                dataString = $" {GetDataString?.Invoke(packetData)}";
            else if (packetData != null)
                dataString = $" {packetData}";
            else
                dataString = "";
            return $"{CallSign} {(char)sendingStation} {uniqueID:X2} {(char)type}{dataString}";
        }

        internal Func<object, string> GetDataString;
    }
}
