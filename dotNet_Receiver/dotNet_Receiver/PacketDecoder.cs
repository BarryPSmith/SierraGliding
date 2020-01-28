using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace dotNet_Receiver
{
    class Packet
    {
        public string CallSign;
        public byte sendingStation;
        public byte uniqueID;
        public char type;

        public object packetData;

        protected char GetChar(byte b) => Encoding.ASCII.GetChars(new[] { b })[0];
        public override string ToString()
            => $"{CallSign} {GetChar(sendingStation)} {uniqueID:X2} {type} {GetDataString?.Invoke(packetData)}";

        internal Func<object, string> GetDataString;
    }

    class SingleWeatherData
    {
        public byte sendingStation;
        public byte uniqueID;
        public double windDirection;
        public double windSpeed;
        public double batteryLevelH;
        public double internalTemp;
        public double externalTemp;

        public override string ToString()
        {
            return $"{windDirection:F1} {windSpeed:F2} {batteryLevelH:F2}";
        }
    }

    /// <summary>
    /// We were getting electrical noise on the wind speed counter.
    /// This data format was used to confirm what was going on. There was no proper protocol. It will likely change in the future
    /// </summary>
    class WindSpeedDebugPacket : Packet
    {
        public byte packetNumber;
        public byte bufferLocation;
        public byte[] data;

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder();
            sb.Append($"_{packetNumber}:{bufferLocation:X2}_,");
            for (int i = 0; i + 1 < data.Length; i += 2)
            {
                ushort value = BitConverter.ToUInt16(data, i);
                sb.Append($"{value},");
            }
            return sb.ToString();
        }
    }

    static class PacketDecoder
    {
        public static Packet DecodeBytes(byte[] inBytes)
        {
            if (inBytes.Length < 4)
                return null;
            var bytes = inBytes as byte[] ?? inBytes.ToArray();

            int cur = 0;
#if false
            var callSign = Encoding.ASCII.GetString(bytes, cur, 6);
            cur += 6;
#else
            var x = Encoding.ASCII.GetChars(bytes, cur++, 1)[0];
            if (x != 'X')
                return null;
#endif
            var type = Encoding.ASCII.GetChars(bytes, cur++, 1)[0];

            var sendingStationID = bytes[cur];
            var uniqueID = bytes[cur + 1];

            Packet ret = new Packet
            {
                CallSign = "X",
                type = type,
                sendingStation = sendingStationID,
                uniqueID = uniqueID
            };

            //Special case, which we should probably remove and make the windspeed debug follow the standard protocol:
            if (bytes[7] == 1 || bytes[7] == 2)
            {
                return new WindSpeedDebugPacket
                {
                    packetNumber = bytes[7],
                    bufferLocation = bytes[8],
                    data = bytes.Skip(9).ToArray()
                };
            }

            else
                switch (type)
                {
                    case 'W':
                        ret.packetData = DecodeWeatherPackets(bytes.AsSpan(cur), sendingStationID, uniqueID);
                        ret.GetDataString = 
                            data => (data as IList<SingleWeatherData>)?.ToCsv();
                        break;
                        /*
                    case 'D':
                        ushort[] dirData = new ushort[(bytes.Length - 9) / 2];
                        Buffer.BlockCopy(bytes, 10, dirData, 0, dirData.Length * 2);
                        ret.packetData = dirData;
                        ret.GetDataString =
                            data => (data as ushort[])?.ToCsv();
                        break;
                    case 'C':
                    case 'K':
                    case 'R':
                        // TODO: Other packet types
                        break;
                        */
                }
            return ret;
        }

        private static string ToCsv<T>(this IEnumerable<T> source)
            => source?.Aggregate("", (run, cur) => run + (string.IsNullOrEmpty(run) ? "" : ", ") + cur);

        private static double GetWindSpeed(byte wsByte)
        {
            double ret;
            if (wsByte <= 100)
                ret = wsByte * 0.5;
            else if (wsByte <= 175)
                ret = wsByte - 50;
            else
                ret = (wsByte - 113) * 2;
            return ret;
        }

        private static double GetWindDirection(byte wdByte)
        {
            return wdByte * 360 / 255.0;
        }

        private static SingleWeatherData DecodeWeatherPacket(Span<byte> data, out int packetLen)
        {
            if (data.Length < 3)
            {
                packetLen = 0;
                return null;
            }

            int cur = 0;
            var len = data[2];
            packetLen = len + 3; // +3: Station ID, message ID, length
            if (packetLen > data.Length)
                return null;

            SingleWeatherData ret = new SingleWeatherData()
            {
                sendingStation = data[cur++],
                uniqueID = data[cur++],
                windDirection = GetWindDirection(data[cur++]),
                windSpeed = GetWindSpeed(data[cur++])
            };

            if (len > 2)
                ret.batteryLevelH = data[cur++] / 255.0 * 7.5;
            if (len > 3)
                ret.externalTemp = GetTemp(data[cur++]);
            if (len > 4)
                ret.internalTemp = GetTemp(data[cur++]);
            return ret;
        }

        private static double GetTemp(byte tempByte)
            => (tempByte - 64.0) / 2;

        private static IList<SingleWeatherData> DecodeWeatherPackets(Span<byte> bytes, byte stationID, byte uniqueID)
        {
            int cur = 0;
            var ret = new List<SingleWeatherData>();
            while (cur < bytes.Length)
            {
                var packet = DecodeWeatherPacket(bytes.Slice(cur), out var len);
                if (packet == null)
                    throw new NullReferenceException("Decode Weather Packet returned null");
                ret.Add(packet);
                cur += len;
            }
            return ret;
        }
    }
}
