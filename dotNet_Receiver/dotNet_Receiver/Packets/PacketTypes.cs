using System;
using System.Collections.Generic;
using System.Text;

namespace core_Receiver.Packets
{
    public enum PacketTypes : byte
    {
        Unknown = 0,
        Modem = 6,
        Stats = 7,
        Weather = (byte)'W',
        Overflow = (byte)'R',
        Command = (byte)'C',
        Response = (byte)'K',
        Ping = (byte)'P',
        StackDump = (byte)'S',
    }
}
