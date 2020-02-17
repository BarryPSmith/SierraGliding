using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace core_Receiver.Packets
{
    class ProgrammingResponse
    {
        public UInt16 expectedCRC;
        public byte totalExpectedPackets;
        public bool allThere;
        public bool crcMatch;
        public bool[] receivedPackets;

        public ProgrammingResponse(Span<byte> data)
        {
            using MemoryStream ms = new MemoryStream(data.ToArray());
            using BinaryReader reader = new BinaryReader(ms);
            expectedCRC = reader.ReadUInt16();
            totalExpectedPackets = reader.ReadByte();
            allThere = reader.ReadByte() != 0;
            crcMatch = reader.ReadByte() != 0;
            var recievedPacketsBytes = reader.ReadBytes(totalExpectedPackets / 8);
            receivedPackets = new bool[totalExpectedPackets];
            for (int i = 0; i < totalExpectedPackets; i++)
            {
                int idxByte = i / 8;
                int idxBit = i % 8;
                receivedPackets[i] = receivedPackets[idxByte] & (1 << idxBit) != 0;
            }
        }

        public override string ToString()
        {
            double percent = (double)receivedPackets.Count(p => p) / receivedPackets.Count();
            return $"CRC: {expectedCRC}, Packet Count: {totalExpectedPackets}, All there: {allThere}, CRC match {crcMatch}, Complete: {percent:P0}, Missing: " +
                $"({receivedPackets.Select((b, i) => (b, i)).Where(tpl => !tpl.b).ToCsv()})";
        }
    }
}
