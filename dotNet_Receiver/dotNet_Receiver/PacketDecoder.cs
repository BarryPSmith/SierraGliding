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
        public double batteryLevelL => batteryLevelH - 7.5;

        public override string ToString()
        {
            return $"{windDirection:F1} {windSpeed:F2} {batteryLevelH:F2} / {batteryLevelL:F2}";
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
            byte[] thisData = new byte[2];
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
            if (inBytes.Length < 8)
                return null;
            var bytes = inBytes as byte[] ?? inBytes.ToArray();

            var callSign = Encoding.ASCII.GetString(bytes, 1, 6);
            var type = Encoding.ASCII.GetChars(bytes, 7, 1)[0];
            var sendingStationID = bytes[8];
            var uniqueID = bytes[9];

            Packet ret = new Packet
            {
                CallSign = callSign,
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
                        ret.packetData = DecodeWeatherPacket(bytes, sendingStationID, uniqueID);
                        ret.GetDataString = 
                            data => (data as IList<SingleWeatherData>)?.ToCsv();
                        break;
                    case 'C':
                    case 'K':
                    case 'R':
                        // TODO: Other packet types
                        break;
                }
            return ret;
        }

        private static string ToCsv<T>(this IEnumerable<T> source)
            => source?.Aggregate("", (run, cur) => (string.IsNullOrEmpty(run) ? "" : ", ") + cur);

        private static double GetWindDirection(byte wdByte)
            => byte.TryParse(Encoding.ASCII.GetString(new[] { wdByte }, 0, 1), NumberStyles.HexNumber, CultureInfo.InvariantCulture.NumberFormat, out var wd) ? 
                wd * 22.5
                : 
                -1;

        private static IList<SingleWeatherData> DecodeWeatherPacket(byte[] bytes, byte stationID, byte uniqueID)
        {
            var length = bytes[10];
            if (length < 3)
                return new SingleWeatherData[0];
            var ret = new SingleWeatherData[1 + (length - 3) / 5];
            ret[0] = new SingleWeatherData
            {
                sendingStation = stationID,
                uniqueID = uniqueID,
                windDirection = GetWindDirection(bytes[11]),
                windSpeed = bytes[12] * 0.5,
                batteryLevelH = 7.5 + bytes[13] / 255.0 * 7.5,
            };
            for (int i = 1; i < ret.Length; i++)
            {
                int offset = -2 + 5 * i;
                ret[i] = new SingleWeatherData
                {
                    sendingStation = bytes[offset],
                    uniqueID = bytes[offset + 1],
                    windDirection = GetWindDirection(bytes[offset + 2]),
                    windSpeed = bytes[offset + 3] * 0.5,
                    batteryLevelH = 7.5 + bytes[offset + 4] / 255.0 * 7.5
                };
            }

            return ret;
        }

    }
}
