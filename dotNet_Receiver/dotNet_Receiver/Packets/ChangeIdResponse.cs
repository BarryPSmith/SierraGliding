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
            if (data.Length >= 6 &&
                Encoding.ASCII.GetString(data.Slice(0, 5)) == "NewId")
            {
                return new ChangeIdResponse { newID = data[5] };
            }
            else
            {
                throw new InvalidDataException();
            }
        }

        public override string ToString()
        {
            return $"New ID: {(char)newID} ({newID})";
        }
    }
}
