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

        public static Packet DecodeBytes(byte[] inBytes, bool checkX = true)
        {
            if (inBytes.Length < 4)
                return null;
            var bytes = inBytes;

            int cur = 0;
#if false
            var callSign = Encoding.ASCII.GetString(bytes, cur, 6);
            cur += 6;
#else
            if (checkX)
            {
                if (inBytes[cur++] != (byte)'X')
                    return null;
            }
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
                uniqueID = uniqueID,
                originalData = bytes
            };

            switch (ret.type)
            {
                case PacketTypes.Modem:
                    ret.packetData = new ModemResponse(bytes.AsSpan(dataStart));
                    break;
                case PacketTypes.Stats:
                    ret.packetData = new StatsResponse(bytes.AsSpan(dataStart));
                    break;
                case PacketTypes.Weather:
                case PacketTypes.Overflow2:
                    (ret.packetData, ret.exception) = DecodeWeatherPackets(bytes.AsSpan(cur));
                    ret.GetDataString = 
                        data => (data as IList<SingleWeatherData>)?.ToCsv();
                    break;
                case PacketTypes.Overflow:
                    (ret.packetData, ret.exception) = DecodeWeatherPackets(bytes.AsSpan(dataStart));
                    ret.GetDataString =
                        data => (data as IList<SingleWeatherData>)?.ToCsv();
                    break;
                case PacketTypes.Response:
                    var responseType = bytes[dataStart++];
                    byte subType;
                    try
                    {
                        subType = bytes[dataStart];
                        switch (responseType.ToChar())
                        {
                            case 'Q':
                                if (subType == 'C')
                                    ret.packetData = new QueryConfigResponse(bytes.AsSpan(dataStart + 1));
                                else if (subType == 'V')
                                    ret.packetData = new QueryVolatileResponse(bytes.AsSpan(dataStart + 1));
                                break;
                            case 'P':
                                if (subType == 'C')
                                    ret.packetData = bytes.ToAscii(dataStart + 1);
                                else if (subType == 'Q')
                                    ret.packetData = new ProgrammingResponse(bytes.AsSpan(dataStart + 1));
                                else if (subType == 'A')
                                    ret.packetData = bytes.ToAscii(dataStart);
                                break;
                            case 'U':
                                ret.packetData = ChangeIdResponse.ParseChangeIdResponse(bytes.AsSpan(dataStart));
                                break;
                            case 'F':
                                if (subType == 'R')
                                {
                                    ret.packetData = bytes.AsSpan(dataStart + 1).ToArray();
                                    ret.GetDataString = Data => ((byte[])Data).ToCsv(b => b.ToString("X2"), " ");
                                }
                                break;
                            case 'D':
                                if (subType == 'L')
                                    ret.packetData = new MessageListResponse(bytes.AsSpan(dataStart + 1));
                                else if (subType == 'R')
                                    ret.packetData = DecodeBytes(bytes.Skip(dataStart + 1).ToArray(), false);
                                break;
                            case 'X':
                                ret.packetData = new CrystalInfo(bytes.AsSpan(dataStart));
                                break;
                            default:
                                break;
                        }
                    }
                    catch
                    {
                        Console.Error.WriteLine("Unable to parse 'K' packet");
                    }
                    if (ret.packetData == null)
                    {
                        if (bytes.ToAscii(dataStart) == "OK")
                            ret.packetData = BasicResponse.OK;
                        else if (bytes.ToAscii(dataStart) == "IGNORED")
                            ret.packetData = BasicResponse.Ignored;
                        else
                            ret.packetData = bytes.Skip(dataStart).ToCsv(b => $"{b.ToChar()}:{b:X}");
                    }
                    break;
                default:
                    ret.packetData = bytes.Skip(dataStart).ToArray();
                    break;
            }
            return ret;
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
            {
                var easyReading = data.ToArray().ToCsv(b => $"{b:X2} {GetChar(b)}");
                throw new InvalidDataException("invalid packet length.");
            }
            if (packetLen < 5)
                throw new InvalidDataException("No weather packets");

            SingleWeatherData ret = new SingleWeatherData()
            {
                sendingStation = data[0],
                uniqueID = data[1],
                //data[2] is length - already recorded.
                windDirection = GetWindDirection(data[cur++]), //3
                windSpeed = GetWindSpeed(data[cur++]), //4
                gust = GetWindSpeed(data[cur++]) //5
            };

            //Temporary fix for stations reporting very high gusts after a restart:
            if (ret.gust > ret.windSpeed * 2.5 && ret.gust > 15)
                ret.gust = null;

            if (packetLen > cur)
                ret.batteryLevelH = data[cur++] / 255.0 * 7.5; //5 (^6)
            if (packetLen > cur)
                ret.externalTemp = GetTemp(data[cur++]); //6 (^7)
            if (packetLen > cur)
                ret.internalTemp = GetTemp(data[cur++]); //7 (^8)
            if (packetLen > cur)
                ret.pwmValue = data[cur++];
            if (packetLen > cur)
                ret.current = data[cur++];
            if (packetLen > cur)
                ret.extras = data[cur..packetLen].ToArray(); //8 (^9)
            return ret;
        }

        private static double GetTemp(byte tempByte)
            => (tempByte - 64.0) / 2;
        static char GetChar(byte b) => 48 <= b && b <= 90 ? b.ToChar() : '#';

        private static (IList<SingleWeatherData>, Exception) DecodeWeatherPackets(Span<byte> bytes)
        {
            Exception exception = null;
            int cur = 0;
            var ret = new List<SingleWeatherData>();
            var easyReading = bytes.ToArray().ToCsv(b => $"{b:X2} {GetChar(b)}");
            while (cur < bytes.Length)
            {
                try
                {
                    var packet = DecodeWeatherPacket(bytes.Slice(cur), out var len);
                    if (packet == null)
                        throw new NullReferenceException("Decode Weather Packet returned null");
                    // Something wrong with R packets... temporary fix
                    /*if (packet.sendingStation < 49 || packet.sendingStation > 80 || packet.extras?.Length > 5)
                        break;*/
                    ret.Add(packet);
                    cur += len;
                }
                catch (Exception ex)
                {
                    exception = ex;
                    Console.Error.WriteLine($"Exception decoding weather packet: {ex}");
                    break;
                }
            }
            var outOfOrderPackets = ret.Where((p, i) =>
                ret
                .Take(i)
                .Any(p2 => p2.sendingStation == p.sendingStation &&
                            p2.uniqueID > p.uniqueID))
                .ToList();
            if (outOfOrderPackets.Any())
            { }

            return (ret, exception);
        }
    }
}
