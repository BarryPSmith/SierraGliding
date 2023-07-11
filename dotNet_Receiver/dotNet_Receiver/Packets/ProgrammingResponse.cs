using System;
using System.Collections.Generic;
using System.Diagnostics;
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
            var recievedPacketsBytes = reader.ReadBytes((int)Math.Ceiling(totalExpectedPackets / 8.0));
            receivedPackets = new bool[totalExpectedPackets];
            for (int i = 0; i < totalExpectedPackets; i++)
            {
                int idxByte = i / 8;
                int idxBit = i % 8;
                receivedPackets[i] = (recievedPacketsBytes[idxByte] & (1 << idxBit)) != 0;
            }
        }

        public override string ToString()
        {
            double percent = (double)receivedPackets.Count(p => p) / receivedPackets.Count();
            return $"CRC: {expectedCRC:X}, Packet Count: {totalExpectedPackets}, All there: {allThere}, CRC match {crcMatch}, Complete: {percent:P0}, Missing: " +
                $"({receivedPackets.Select((b, i) => (b, i)).Where(tpl => !tpl.b).Select(tpl => tpl.i).ToCsv()})";
        }

        public static object DecodeProgrammingResponse(Span<Byte> bytes)
        {
            var byteArray = bytes.ToArray();
            var subType = (char)bytes[0];
            switch ((char)subType)
            {
                case 'C':
                    return byteArray.ToAscii(1);
                case 'Q':
                    return new ProgrammingResponse(bytes.Slice(1));
                case 'A':
                    return byteArray.ToAscii();
                case 'I':
                    return DecodePacketFailure(bytes);
                default:
                    return null;
            }
        }

        public static string DecodePacketFailure(Span<byte> data)
        {
            Debug.Assert(data[0] == 'I');
            if (data.Length < 3)
                return "Remote Programming: Response too short";
            if (data[1] != 0)
                return "Remote Programming: Unexpected non-failure.";
            switch (data[2])
            {
                case 1:
                    return "Remote Programming: Packet out of range";
                case 2:
                    return "Remote Programming: Duplicate";
                case 3:
                    return "Remote Programming: CRC Mismatch";
                case 4:
                    return "Remote Programming: Length Mismatch";
                case 5:
                    return "Remote Programming: Unable to read bytes-per-packet.";
                case 6:
                    return "Remote Programming: Unrecognised message subtype.";
                case 7:
                    return "Remote Programming: Unable to get message subtype.";
                default:
                    return $"Remote Programming: Unknown Failure ({data[2]})";
            }
        }
    }
}
