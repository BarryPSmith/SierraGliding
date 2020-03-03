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

        public static char GetChar(byte b) => Encoding.ASCII.GetChars(new[] { b })[0];
        public override string ToString()
        {
            string dataString;
            if (GetDataString != null)
                dataString = $" {GetDataString?.Invoke(packetData)}";
            else if (packetData != null)
                dataString = $" {packetData}";
            else
                dataString = "";
            return $"{CallSign} {GetChar(sendingStation)} {uniqueID:X2} {GetChar((byte)type)}{dataString}";
        }

        internal Func<object, string> GetDataString;
    }
}
