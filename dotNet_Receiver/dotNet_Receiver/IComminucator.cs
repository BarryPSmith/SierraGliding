using System;
using System.Collections.Generic;
using System.IO;

namespace core_Receiver
{
    interface ICommunicator
    {
        TextWriter OutputWriter { get; }
        Action<string> StreamError { get; set; }

        event EventHandler<(IList<Byte> data, bool corrupt)> PacketReceived;

        byte GetNextUniqueID();
        void Write(byte[] data, byte writeType = 0);

        void Disconnect();
    }
}