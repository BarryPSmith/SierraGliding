using System;
using System.Collections.Generic;
using System.Text;

namespace core_Receiver.Packets
{
    class ChangeIdResponse
    {
        public byte newID;
        public static object ParseChangeIdResponse(Span<byte> data, out bool handled)
        {
            if (data.Length >= 6 &&
                Encoding.ASCII.GetString(data.Slice(0, 5)) == "NewId")
            {
                handled = true;
                return new ChangeIdResponse { newID = data[5] };
            }
            else
            {
                handled = false;
                return null;
            }
        }

        public override string ToString()
        {
            return $"New ID: {(char)newID} ({newID})";
        }
    }
}
