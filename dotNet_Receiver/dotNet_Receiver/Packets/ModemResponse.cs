using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace core_Receiver.Packets
{
    class ModemResponse
    {
        public ModemResponse(Span<byte> data)
        {
            using MemoryStream ms = new MemoryStream();
            ms.Write(data);
            ms.Seek(0, SeekOrigin.Begin);
            BinaryReader br = new BinaryReader(ms, Encoding.ASCII);

            CRCErrorRate = br.ReadUInt16() / (double)0xFFFF;
            DroppedPacketRate = br.ReadUInt16() / (double)0xFFFF;
            AverageDelayTime = br.ReadUInt32();
        }

        double CRCErrorRate { get; set; }
        double DroppedPacketRate { get; set; }
        double AverageDelayTime { get; set; }

        public override string ToString()
        {
            return $"Modem: CRC Error Rate: {CRCErrorRate:P1}, " +
                $"Dropped Packet Rate: {DroppedPacketRate:P1}, " +
                $"Average Delay Time: {AverageDelayTime / 1000} ms";
                    
        }
    }
}
