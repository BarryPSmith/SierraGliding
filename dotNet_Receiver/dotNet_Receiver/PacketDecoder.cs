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
            var sendingStationID = bytes[cur++];
            var uniqueID = bytes[cur++];

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
                        ret.packetData = DecodeWeatherPacket(bytes, sendingStationID, uniqueID, cur);
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
            // we got confused reading the Davis datasheet. This is a workaround until we can update the station data.
            // in all fairness, who specifies revs / hour?
            const double misreadDataSheet = 3.6 / 1.6;
            double ret;
            if (wsByte <= 100)
                ret = wsByte * 0.5;
            else if (wsByte <= 175)
                ret = wsByte - 50;
            else
                ret = (wsByte - 113) * 2;
            ret *= misreadDataSheet;
            return ret;
        }

        private static double GetWindDirection(byte wdByte, bool isLegacy)
        {
            if (isLegacy)
                return byte.TryParse(Encoding.ASCII.GetString(new[] { wdByte }, 0, 1), NumberStyles.HexNumber, CultureInfo.InvariantCulture.NumberFormat, out var wd) ?
                    wd * 22.5
                    :
                    -1;
            else
                return wdByte * 360 / 255.0;
        }

        private static IList<SingleWeatherData> DecodeWeatherPacket(byte[] bytes, byte stationID, byte uniqueID,
            int cur)
        {
            var length = bytes[cur++];
            if (length < 3)
                return new SingleWeatherData[0];
            var ret = new SingleWeatherData[1 + (length - 3) / 5];
            ret[0] = new SingleWeatherData
            {
                sendingStation = stationID,
                uniqueID = uniqueID,
                windDirection = GetWindDirection(bytes[cur++], stationID == 49),
                windSpeed = GetWindSpeed(bytes[cur++]), 
                batteryLevelH = /*7.5 + */bytes[cur++] / 255.0 * 7.5,
            };

            for (int i = 1; i < ret.Length; i++)
            {
                int offset = 8 + 5 * i;
                byte sendingStationID = bytes[offset];
                ret[i] = new SingleWeatherData
                {
                    sendingStation = sendingStationID,
                    uniqueID = bytes[offset + 1],
                    windDirection = GetWindDirection(bytes[offset + 2], sendingStationID == 49),
                    windSpeed = bytes[offset + 3] * 0.5,
                    batteryLevelH = 7.5 + bytes[offset + 4] / 255.0 * 7.5
                };
            }

            return ret;
        }

    }
}
