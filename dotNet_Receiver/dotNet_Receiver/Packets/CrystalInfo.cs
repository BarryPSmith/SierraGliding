using System;
using System.Collections.Generic;
using System.Text;

namespace core_Receiver.Packets
{
    class CrystalInfo
    {
        public bool Failed { get; set; }
        public byte Test1 { get; set; }
        public byte Test2 { get; set; }
        public CrystalInfo(Span<byte> data)
        {
            Failed = data[0] != 0;
            Test1 = data[1];
            Test2 = data[2];
        }

        public override string ToString()
        {
            return $"{(Failed ? "XTal Failed" : "XTal OK")}: {Test1}, {Test2}";
        }
    }
}
