using System;
using System.Collections.Generic;
using System.Text;

namespace core_Receiver.Packets
{
    enum PacketTypes : byte
    {
        Unknown = 0,
        Modem = 6,
        Weather = (byte)'W',
        Overflow = (byte)'R',
        Command = (byte)'C',
        Response = (byte)'K',
        Ping = (byte)'P',
        StackDump = (byte)'S'
    }
}
