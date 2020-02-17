using core_Receiver.Packets;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace core_Receiver
{

    enum BasicResponse { OK, Ignored };




#if false
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
#endif

    static class PacketDecoder
    {
        public static Dictionary<byte, string> RecentCommands { get; } = new Dictionary<byte, string>();

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
            int dataStart = cur + 2;

            Packet ret = new Packet
            {
                CallSign = "X",
                type = type,
                sendingStation = sendingStationID,
                uniqueID = uniqueID
            };

            switch (type)
            {
                case 'W':
                    ret.packetData = DecodeWeatherPackets(bytes.AsSpan(cur));
                    ret.GetDataString = 
                        data => (data as IList<SingleWeatherData>)?.ToCsv();
                    break;
                case 'K':
                    bool knownCommand = RecentCommands.TryGetValue(uniqueID, out var cmdType);
                    if (knownCommand)
                    {
                        switch (cmdType)
                        {
                            case "QC":
                                ret.packetData = new QueryConfigResponse(bytes.AsSpan(dataStart));
                                break;
                            case "QV":
                                ret.packetData = new QueryVolatileResponse(bytes.AsSpan(dataStart));
                                break;
                            case "PQ":
                                ret.packetData = new ProgrammingResponse(bytes.AsSpan(dataStart));
                                break;
                            case "PFR":
                            default:
                                knownCommand = false;
                                break;
                        }
                    }
                    if (!knownCommand)
                    {
                        if (bytes.Length >= dataStart + "OK".Length &&
                                    Encoding.ASCII.GetString(bytes, dataStart, "OK".Length) == "OK")
                            ret.packetData = BasicResponse.OK;
                        else if (bytes.Length >= dataStart + "IGNORED".Length &&
                            Encoding.ASCII.GetString(bytes, dataStart, "IGNORED".Length) == "IGNORED")
                            ret.packetData = BasicResponse.Ignored;
                        else
                            ret.packetData = bytes.Skip(dataStart).ToCsv(b => $"{Packet.GetChar(b)}:{b}");
                    }
                    break;
                    /*
                case 'D':
                    ushort[] dirData = new ushort[(bytes.Length - 9) / 2];
                    Buffer.BlockCopy(bytes, 10, dirData, 0, dirData.Length * 2);
                    ret.packetData = dirData;
                    ret.GetDataString =
                        data => (data as ushort[])?.ToCsv();
                    break;
                case 'K':
                case 'R':
                    // TODO: Other packet types
                    break;
                    */
            }
            return ret;
        }

        public static string ToCsv<T>(this IEnumerable<T> source, Func<T, string> transformer = null)
        {
            if (transformer == null)
                transformer = A => A.ToString();
            return source?.Aggregate("", (run, cur) => run + (string.IsNullOrEmpty(run) ? "" : ", ") + transformer(cur));
        }

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
            return wdByte * 360 / 256.0;
        }

        private static SingleWeatherData DecodeWeatherPacket(Span<byte> data, out int packetLen)
        {
            if (data.Length < 3)
            {
                packetLen = 0;
                throw new InvalidDataException("No weather packets");
            }

            int cur = 3;
            var len = data[2];
            packetLen = len + 3; // +3: Station ID, message ID, length
            if (packetLen > data.Length)
                throw new InvalidDataException("invalid packet length.");

            SingleWeatherData ret = new SingleWeatherData()
            {
                sendingStation = data[0],
                uniqueID = data[1],
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

        private static IList<SingleWeatherData> DecodeWeatherPackets(Span<byte> bytes)
        {
            int cur = 0;
            var ret = new List<SingleWeatherData>();
            while (cur < bytes.Length)
            {
                try
                {
                    var packet = DecodeWeatherPacket(bytes.Slice(cur), out var len);
                    if (packet == null)
                        throw new NullReferenceException("Decode Weather Packet returned null");
                    ret.Add(packet);
                    cur += len;
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"Exception decoding weather packet: {ex}");
                    break;
                }
            }
            return ret;
        }
    }
}
