using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace core_Receiver.Packets
{
    class ChangeIdResponse
    {
        public byte newID;
        public static object ParseChangeIdResponse(Span<byte> data)
        {
            if (data.ToArray().ToAscii().StartsWith("NewID"))
            {
                return new ChangeIdResponse { newID = data[5] };
            }
            else
            {
                return null;
            }
        }

        public override string ToString()
        {
            return $"New ID: {newID.ToChar()} ({newID})";
        }
    }
}
