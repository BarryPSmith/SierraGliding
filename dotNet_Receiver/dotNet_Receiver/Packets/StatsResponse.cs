using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace core_Receiver.Packets
{
    class StatsResponse
    {
        public StatsResponse(Span<byte> data)
        {
            using MemoryStream ms = new MemoryStream();
            ms.Write(data);
            ms.Seek(0, SeekOrigin.Begin);
            BinaryReader br = new BinaryReader(ms, Encoding.ASCII);

            RSSI = br.ReadSingle();
            SNR = br.ReadSingle();
            if (SNR >= 128 / 4)
                SNR -= 256/4;
        }

        public float RSSI { get; set; }
        public float SNR { get; set; }

        public override string ToString()
        {
            return $"Modem: RSSI {RSSI:F1}, SNR {SNR:F1}";
        }
    }
}
