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
            bool wasRelayDemanded = (bytes[cur] & 0x80) != 0;
            var type = (byte)(bytes[cur++] & 0x7F);

            var sendingStationID = bytes[cur];
            var uniqueID = bytes[cur + 1];
            int dataStart = cur + 2;

            Packet ret = new Packet
            {
                CallSign = "X",
                type = (PacketTypes)type,
                sendingStation = sendingStationID,
                uniqueID = uniqueID
            };

            switch (ret.type)
            {
                case PacketTypes.Modem:
                    ret.packetData = new ModemResponse(bytes.AsSpan(dataStart));
                    break;
                case PacketTypes.Weather:
                    ret.packetData = DecodeWeatherPackets(bytes.AsSpan(cur));
                    ret.GetDataString = 
                        data => (data as IList<SingleWeatherData>)?.ToCsv();
                    break;
                case PacketTypes.Response:
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
                            case "U":
                                ret.packetData = ChangeIdResponse.ParseChangeIdResponse(bytes.AsSpan(dataStart), out knownCommand);
                                break;
                            case "PFR":
                                ret.packetData = bytes.AsSpan(dataStart).ToArray();
                                ret.GetDataString = Data => ((byte[])Data).ToCsv(b => b.ToString("X2"), " ");
                                break;
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
                            ret.packetData = bytes.Skip(dataStart).ToCsv(b => $"{Packet.GetChar(b)}:{b:X}");
                    }
                    break;
                default:
                    ret.packetData = bytes.Skip(dataStart).ToArray();
                    break;
            }
            return ret;
        }

        public static string ToCsv<T>(this IEnumerable<T> source, Func<T, string> transformer = null, string separator = ", ")
        {
            if (transformer == null)
                transformer = A => A.ToString();
            return source?.Aggregate("", (run, cur) => run + (string.IsNullOrEmpty(run) ? "" : separator) + transformer(cur));
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
            if (len > 5)
                ret.extras = data.Slice(cur).ToArray();
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
