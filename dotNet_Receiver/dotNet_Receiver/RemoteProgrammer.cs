using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace core_Receiver
{
    class RemoteProgrammer
    {
        private byte _destinationStationID;
        private IList<byte> image;
        private UInt16 crc;

        public RemoteProgrammer(byte destinationStationID, string hexSourceFile)
        {

        }

        List<byte> ReadHex(string hexSourceFile)
        {
            var lines = File.ReadAllLines(hexSourceFile);
            int curAddress = 0;
            int lineIdx = 0;
            List<byte> ret = new List<byte>();
            bool ended = false;
            foreach (var line in lines)
            {
                lineIdx++;
                if (ended)
                    throw new InvalidDataException($"Line {lineIdx} occurs after EOF marker.");
                if (line[0] != ':')
                    throw new InvalidDataException($"Line {lineIdx}: Each line in a hex file must start with a colon");
                if (!int.TryParse(line.AsSpan(1, 2), NumberStyles.HexNumber, null, out int byteCount))
                    throw new InvalidDataException($"Line {lineIdx}: Cannot read byte count.");
                if (!int.TryParse(line.AsSpan(3, 4), NumberStyles.HexNumber, null, out int address))
                    throw new InvalidDataException($"Line {lineIdx}: Cannot read address");
                if (!int.TryParse(line.AsSpan(7, 2), NumberStyles.HexNumber, null, out int recordType))
                    throw new InvalidDataException($"Line {lineIdx}: Cannot read recordType");
                string newDataStr = byteCount > 0 ? line.Substring(9, byteCount * 2) : null;
                if (!int.TryParse(line.AsSpan(9 + byteCount * 2, 2), NumberStyles.HexNumber, null, out int checksum))
                    throw new InvalidDataException($"Line {lineIdx}: Cannot read checksum.");

                byte[] newData = new byte[byteCount];
                for (int i = 0; i < byteCount; i += 2)
                    newData[i / 2] = byte.Parse(newDataStr.Substring(i, 2), NumberStyles.HexNumber);
                byte calculatedChecksum = (byte)~(newData.Sum(b => (int)b) & 0xFF);
                if (calculatedChecksum != checksum)
                    throw new InvalidDataException($"Line {lineIdx}: Checksum Mismatch. Calculated: {calculatedChecksum:X}, expected: {checksum:X}");

                switch (recordType)
                {
                    case 0: // Data
                        if (address != curAddress)
                            throw new InvalidDataException($"Line {lineIdx}: Unexpected address {address:X}. Expect: {curAddress:X}");
                        ret.AddRange(newData);
                        curAddress += byteCount;
                        break;
                    case 1: //EOF
                        if (address != 0)
                            throw new InvalidDataException($"Line {lineIdx}: EOF With non-zero address.");
                        ended = true;
                        break;
                    default:
                        throw new NotImplementedException($"Line {lineIdx}: Record Type {recordType:X} not implemented");
                }
            }
            if (!ended)
                throw new InvalidDataException("No EOF marker in file.");
            return ret;
        }
    }
}
