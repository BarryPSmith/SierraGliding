using core_Receiver;
using core_Receiver.Packets;
using System.Data.SQLite;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;

namespace core_Receiver
{
    static class Program
    {
        static string _callSign = "KN6DUC";

        static LocalDatastore _dataStore;
        static void Help()
        {
            Console.WriteLine(
@"Sierra Gliding data receiver.

Connect to a KISS data source with station data.
Arguments:
 --src <address:port>   Source address to use. Default localhost:8001
 --serial <serial port> Serial port to use.
 --db <DB.sqlite>       Database to store data locally. Doesn't work until I get the SQLite stuff sorted.
 --dest <address:port>  Destionation web server. Default http://localhost:4000. Can be specified multiple times to send data to multiple servers.
 --nonPacket <file>     Record the non-packet stream in a particular file. Set to 'DISCARD' to discard the non packet stream.
 --exposeUDP <incoming> <outgoing> Expose the serial and net streams to UDP on ports incoming and outgoing
 --connectUDP <outgoing> <incoming> Send and receive via UDP rather than serial or TCP socket
 --offset <offset>      All station data reported to the server will have their IDs offset by this amount
 --callsign <Callsign>  The callsign to use. Must match station callsign for ping to work.
 --pingInterval <seconds> Specify a custom ping interval. Default is 5 minutes (300)
 --noping               Specify that the software should not send pings
 --logWeather           Specify that the local db will store weather packets
 --nts <list>           [Temporary] List of stations that do not send timestamped messages (pre-2.5)
"
);
        }

        static ICommunicator _dataReceiver;

        static public volatile TextWriter ErrorWriter = Console.Error;
        static public volatile TextWriter OutputWriter = Console.Out;

        static List<DataPosting> _serverPosters;

        static string _dbFn;
        readonly static List<string> _destUrls = new List<string>();
        static bool _connectUDP = false;
        static int? _incomingPort, _outgoingPort;
        static string _srcNetworkAddress = null;
        static string _serialAddress = null;
        static string _npsFn = null;
        static int _offset = 0;
        static bool _logWeather = false;

        static readonly CancellationTokenSource _exitingSource = new CancellationTokenSource();

        static void ParseArgs(string[] args)
        {
            var dbIndex = Array.FindIndex(args, arg => arg.Equals("--db", StringComparison.OrdinalIgnoreCase));
            if (dbIndex >= 0)
                _dbFn = args[dbIndex + 1];

            for (int idx = Array.FindIndex(args, arg => arg.Equals("--dest", StringComparison.OrdinalIgnoreCase));
                     idx >= 0;
                     idx = Array.FindIndex(args, idx + 1, arg => arg.Equals("--dest", StringComparison.OrdinalIgnoreCase)))
                _destUrls.Add(args[idx + 1]);

            int offsetIdx = Array.FindIndex(args, arg => arg.Equals("--offset", StringComparison.OrdinalIgnoreCase));
            if (offsetIdx >= 0)
            {
                _offset = int.Parse(args[offsetIdx + 1]);
            }

            int connectUdpIndex = Array.FindIndex(args, arg => arg.Equals("--connectUDP", StringComparison.OrdinalIgnoreCase));
            if (connectUdpIndex >= 0)
            {
                _connectUDP = true;
                _incomingPort = int.Parse(args[connectUdpIndex + 1]);
                _outgoingPort = int.Parse(args[connectUdpIndex + 2]);
            }

            var srcArgIndex = Array.FindIndex(args, arg => arg.Equals("--src", StringComparison.OrdinalIgnoreCase));
            if (srcArgIndex >= 0)
                _srcNetworkAddress = args[srcArgIndex + 1];

            var serialArgIndex = Array.FindIndex(args, arg => arg.Equals("--serial", StringComparison.OrdinalIgnoreCase));
            if (serialArgIndex >= 0 && args.Length > serialArgIndex + 1)
                _serialAddress = args[serialArgIndex + 1];

            var npsFnIndex = Array.FindIndex(args, arg => arg.Equals("--nonPacket", StringComparison.OrdinalIgnoreCase));
            if (npsFnIndex >= 0)
                _npsFn = args[npsFnIndex + 1];

            int exposeUdpIndex = Array.FindIndex(args, arg => arg.Equals("--exposeUDP", StringComparison.OrdinalIgnoreCase));
            if (exposeUdpIndex >= 0)
            {
                _outgoingPort = int.Parse(args[exposeUdpIndex + 1]);
                _incomingPort = int.Parse(args[exposeUdpIndex + 2]);
            }

            int callsignIndex = Array.FindIndex(args, arg => arg.Equals("--callsign", StringComparison.OrdinalIgnoreCase));
            if (callsignIndex >= 0)
            {
                _callSign = args[callsignIndex + 1];
            }

            int pingIntervalIndex = Array.FindIndex(args, arg => arg.Equals("--pingInterval", StringComparison.OrdinalIgnoreCase));
            if (pingIntervalIndex >= 0)
            {
                _pingInterval = TimeSpan.FromSeconds(int.Parse(args[pingIntervalIndex + 1]));
            }

            int ntsIndex = Array.FindIndex(args, arg => arg.Equals("--nts", StringComparison.OrdinalIgnoreCase));
            if (ntsIndex >= 0)
            {
                _ntsStations = args[ntsIndex + 1].Split(',').Select(str => 
                    str.Length > 1 ? int.Parse(str) : (int)str[0]).ToHashSet();
            }

            if (args.Contains("--logWeather", StringComparer.OrdinalIgnoreCase))
                _logWeather = true;

            if (args.Contains("--noPing", StringComparer.OrdinalIgnoreCase))
                _sendPing = false;
        }

        static void Main(string[] args)
        {
#if DEBUG
            for (int i = 0; i < args.Length; i++)
                OutputWriter.WriteLine($"{i}: {args[i]}");
#endif
            Stream nps = null;

            if (!(args?.Length > 0))
            {
                Help();
                return;
            }

            TaskScheduler.UnobservedTaskException += TaskScheduler_UnobservedTaskException;
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;
            AppDomain.CurrentDomain.ProcessExit += CurrentDomain_ProcessExit;

            ParseArgs(args);

            PacketDecoder.NtsStations = _ntsStations;

            FileStream npsFs = null;
            bool npsIsStandardOut = true;

            if (!string.IsNullOrEmpty(_dbFn))
                _dataStore = new LocalDatastore(_dbFn)
                {
                    StoreWeather = _logWeather
                };

            List<Task> runTasks;

            _serverPosters = _destUrls.Select(url =>
            {
                var ret = new DataPosting(url);
                ret.Offset = _offset;
                ret.OnException += (sender, ex) => 
                    ErrorWriter.WriteLine($"Post error ({url}): {ex.GetType()}: {ex.Message}");
                return ret;
            }).ToList();

            if (_connectUDP)
            {
                var udpListener = new SocketListener(_incomingPort.Value, _outgoingPort.Value, _outgoingPort.Value + 2, false);
                _dataReceiver = udpListener;
                runTasks = udpListener.StartAsync(CancellationToken.None);
            }
            else
            {
                var communicator = new KissCommunication();
                _dataReceiver = communicator;

                npsIsStandardOut = string.IsNullOrEmpty(_npsFn);
                if (npsIsStandardOut)
                {
                    nps = Console.OpenStandardOutput();
                }
                else if (!_npsFn.Equals("DISCARD", StringComparison.OrdinalIgnoreCase))
                {
                    npsFs = new FileStream(_npsFn, FileMode.Append, FileAccess.Write, FileShare.ReadWrite | FileShare.Delete, 4096);
                    nps = Stream.Synchronized(npsFs);
                    FlushForever(nps);
                }
                communicator.NonPacketStream = nps;

                if (!Console.IsInputRedirected)
                    runTasks = StartInteractive(communicator, _srcNetworkAddress, _serialAddress);
                else
                    runTasks = StartNonInteractive(communicator, _srcNetworkAddress, _serialAddress);

                if (_outgoingPort.HasValue && _incomingPort.HasValue)
                {
                    var udpListener = new SocketListener(_incomingPort.Value, _outgoingPort.Value, _incomingPort.Value + 2, true);
                    runTasks.AddRange(udpListener.StartAsync(_exitingSource.Token));
                    communicator.PacketReceived += (sender, e) => udpListener.Write(e.Item1.ToArray(), (byte)(e.corrupt ? 1 : 0));
                    udpListener.PacketReceived += (sender, e) => communicator.Write(e.data.ToArray());
                    udpListener.PacketReceived6 += (sender, e) => communicator.Write(e.ToArray(), 6);
                }
            }

            _dataReceiver.PacketReceived += OnPacketReceived;

            try
            {
                if (!Console.IsInputRedirected)
                {
                    Console.WriteLine("Running Interactively...");
                    RunInteractive(npsIsStandardOut);
                }
                else
                {
                    Console.WriteLine("Running non-interactively");
                }
                Task.WaitAll(runTasks.ToArray());
            }
            finally
            {
                if (npsFs != null)
                {
                    npsFs.Flush();
                    npsFs.Close();
                }
            }
        }

        private static void FlushForever(Stream str)
        {
            Task.Delay(500)
                .ContinueWith(notUsed => str.Flush())
                .ContinueWith(notUsed => FlushForever(str));
        }

        static object _consoleLock = new object();
        public static void WriteColoured(TextWriter writer, string text, ConsoleColor? foreColour, ConsoleColor? backColour = null,
            Action<string> writeFunction = null)
        {
            lock (_consoleLock)
            {
                if (writeFunction == null)
                    writeFunction = writer.Write;
                ConsoleColor? oldForeground = null,
                    oldBackground = null;
                if (writer == Console.Out)
                {
                    if (backColour != null)
                    {
                        oldBackground = Console.BackgroundColor;
                        Console.BackgroundColor = backColour.Value;
                    }
                    if (foreColour != null)
                    {
                        oldForeground = Console.ForegroundColor;
                        Console.ForegroundColor = foreColour.Value;
                    }
                }
                writeFunction(text);
                if (oldBackground != null)
                    Console.BackgroundColor = oldBackground.Value;
                if (oldForeground != null)
                    Console.ForegroundColor = oldForeground.Value;
            }
        }

        static void WriteUndecipherablePacket(DateTime receivedTime, byte[] localBytes,
            bool corrupt)
        {
            var startStr = corrupt ? "Corrupt" : "Unhandled";
            WriteColoured(OutputWriter, $"{receivedTime} {startStr} Packet: {localBytes.ToAscii()}",
                ConsoleColor.White, ConsoleColor.Red, OutputWriter.WriteLine);
        }

        static Dictionary<PacketTypes, ConsoleColor> PacketColours = new Dictionary<PacketTypes, ConsoleColor>
        {
            { PacketTypes.Response, ConsoleColor.Yellow },
            { PacketTypes.Overflow, ConsoleColor.Gray },
            { PacketTypes.Command, ConsoleColor.Blue }
        };

        static void WritePacket(DateTime receivedTime, Packet packet, bool corrupt)
        {
            var errorRegex = new Regex(@"ERR:-?\d+ \(-?\d+s\)");
            lock (_consoleLock)
            {
                if (!PacketColours.TryGetValue(packet.type, out var backColour))
                    backColour = ConsoleColor.White;
                if (corrupt)
                {
                    backColour = ConsoleColor.Red;
                    OutputWriter.Write("Corrupt ");
                }
                OutputWriter.Write($"Packet {receivedTime}: ");
                WriteColoured(OutputWriter, packet.IdentityString,
                    ConsoleColor.Black, backColour);
                OutputWriter.Write(" ");
                WriteColoured(OutputWriter, $"{packet.RSSI,6:F1} {packet.SNR,4:F1}",
                    ConsoleColor.Black, ConsoleColor.Cyan);
                var safeDataString = packet.SafeDataString;
                var prevIdx = 0;
                for (Match m = errorRegex.Match(safeDataString); m.Success; m = m.NextMatch())
                {
                    OutputWriter.Write(safeDataString.Substring(prevIdx, m.Index - prevIdx));
                    prevIdx = m.Index + m.Length;
                    WriteColoured(OutputWriter, m.Value, ConsoleColor.Black, ConsoleColor.Yellow);
                }
                if (prevIdx < safeDataString.Length)
                    OutputWriter.Write(safeDataString.Substring(prevIdx, safeDataString.Length - prevIdx));
                OutputWriter.WriteLine();
            }
        }

        static Packet _lastPacket;
        private static void OnPacketReceived(object sender, (IList<byte> data, bool corrupt) e)
        {
            var data = e.data;
            bool corrupt = e.corrupt;
            DateTimeOffset receivedTime = DateTimeOffset.Now;
            try
            {
                var packet = PacketDecoder.DecodeBytes(data as byte[] ?? data.ToArray(), receivedTime);
                if (packet == null)
                {
                    var localBytes = data.ToArray();
                    if (!corrupt)
                        Task.Run(() => _dataStore?.RecordUndecipherable(localBytes));
                    Task.Run(() => WriteUndecipherablePacket(receivedTime.LocalDateTime, localBytes, corrupt));
                    return;
                }
                //Do this in a Task to avoid waiting if we've scrolled up.
                Task consoleTask = null;
                if (packet.type != PacketTypes.Stats)
                    consoleTask = Task.Run(async () =>
                    {
                        // Delay for 100ms to allow the modem to send through RSSI & SNR
                        await Task.Delay(100);
                        WritePacket(receivedTime.LocalDateTime, packet, corrupt);
                    });
                switch (packet.type)
                {
                    case PacketTypes.Weather:
                    case PacketTypes.Overflow:
                    case PacketTypes.Overflow2:
                        if (!corrupt)
                            foreach (var serverPoster in _serverPosters)
                                serverPoster?.SendWeatherDataAsync(packet, receivedTime);
                        break;
                    case PacketTypes.Response:
                        AcknowledgeResponse(packet.uniqueID, packet.sendingStation);
                        if (packet.packetData.Equals(BasicResponse.Timeout)
                            && DateTimeOffset.Now - _lastPing > TimeSpan.FromSeconds(5))
                            _pingEvent.Set();
                        break;
                    case PacketTypes.Command:
                    case PacketTypes.Ping:
                    case PacketTypes.Modem:
                        break;
                    case PacketTypes.Stats:
                        if (_lastPacket != null)
                        {
                            var statsResponse = packet.packetData as StatsResponse;
                            _lastPacket.RSSI = statsResponse.RSSI;
                            _lastPacket.SNR = statsResponse.SNR;
                            var localLast = _lastPacket;
                            Task.Run(() => _dataStore?.RecordStats(localLast, statsResponse));
                        }
                        break;
                    default:
                        //nps is often stdOut in debugging, don't write to it from multiple threads:
                        var localBytes = data.ToArray();
                        consoleTask.ContinueWith(notUsed => WriteUndecipherablePacket(receivedTime.LocalDateTime, localBytes, corrupt));
                        break;
                }
                _lastPacket = packet.type == PacketTypes.Stats || packet.type == PacketTypes.Modem ? null : packet;
                if (!corrupt)
                    Task.Run(() => _dataStore?.RecordPacket(packet));
                
            }
            catch (Exception ex)
            {
                ErrorWriter.WriteLine($"Error {DateTime.Now}: {ex}");
            }
        }

        private static void AcknowledgeResponse(byte uniqueID, byte stationID)
        {
            // We acknowledge the response to stop stations from sending the K packet a second time.
            // We probably waste more airtime by replying with our long preamble,
            // but this way we control it and (hopefully) avoid a repeat K colliding with our next outgoing C.
            // We only acknowledge if we're the primary receiver
            if (_connectUDP)
                return;
            byte[] data = { (byte)'L', stationID, uniqueID };
            _dataReceiver.Write(data);
        }

        private static void StartPingLoop()
        {
            Thread t = new Thread(PingLoop)
            {
                IsBackground = true,
                Name = "PingThread"
            };
            t.Start();
        }

        private static List<Task> StartNonInteractive(KissCommunication kisser, string srcAddress, string serialAddress)
        {
            List<Task> tasks = new List<Task>();
            if (!string.IsNullOrEmpty(srcAddress))
                tasks.Add(TryForever(() => kisser.Connect(srcAddress)));
            if (!string.IsNullOrEmpty(serialAddress))
            {
                tasks.Add(TryForever(() =>
                {
                    kisser.ConnectSerial(serialAddress);
                }));
            }
            StartPingLoop();
            return tasks;
        }

        private static List<Task> StartInteractive(KissCommunication kisser, string srcAddress, string serialAddress)
        {
            List<Task> tasks = new List<Task>();
            if (!string.IsNullOrEmpty(srcAddress))
                tasks.Add(Task.Run(() => kisser.Connect(srcAddress)));
            if (!string.IsNullOrEmpty(serialAddress))
            {
                tasks.Add(TryForever(() =>
                {
                    kisser.ConnectSerial(serialAddress);
                }));
            }
            StartPingLoop();
            return tasks;
        }

        private static void ShowSuspended(char curChar)
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                var oldLeft = Console.CursorLeft;
                Console.WriteLine();
                Console.MoveBufferArea(0, Console.CursorTop - 1, Console.BufferWidth, 1, 0, Console.CursorTop);
                Console.CursorTop--;
                WriteColoured(Console.Out, "Output Suspended. Press enter to finish current line and resume", ConsoleColor.Yellow);
                Console.CursorTop++;
                Console.CursorLeft = oldLeft;
            }
            else
            {
                WriteColoured(Console.Out, "Output Suspended. Press enter to finish current line and resume", ConsoleColor.Yellow,
                    null, Console.WriteLine);
                Console.Write(curChar);
            }
        }

        private static void RunInteractive(bool npsIsStandardOut)
        {
            OutputWriter.WriteLine("Reading data. Type '/' followed by a command to send. Press q key to exit.");
            CommandInterpreter interpreter = new CommandInterpreter(_dataReceiver);
            while (true)
            {
                bool exit = false;
                using var ms = new MemoryStream();
                using var safeStream = Stream.Synchronized(ms);
                using StreamWriter sw = new StreamWriter(safeStream);
                Stream oldNps = null;
                try
                {
                    var key = Console.ReadKey(false);
                    ShowSuspended(key.KeyChar);
                    OutputWriter = sw;
                    //While we're doing this, 
                    ErrorWriter = sw;
                    if (npsIsStandardOut && _dataReceiver is KissCommunication kisser)
                    {
                        oldNps = kisser.NonPacketStream;
                        kisser.NonPacketStream = safeStream;
                    }
                    interpreter.ProgrammerOutput = sw;
                    var line2 = Console.ReadLine();
                    var line = key.KeyChar + line2;
                    if (!interpreter.HandleLine(line))
                    {
                        exit = true;
                        Console.WriteLine("Shutting Down...");
                        _dataReceiver.Disconnect();
                        _exitingSource.Cancel();
                        return;
                    }
                }
                catch (Exception ex)
                {
                    if (exit)
                        throw;
                    else
                        Console.Error.WriteLine(ex.Message);
                }
                finally
                {
                    OutputWriter = Console.Out;
                    ErrorWriter = Console.Error;
                    if (_dataReceiver is KissCommunication kisser)
                        kisser.NonPacketStream = oldNps;
                    interpreter.ProgrammerOutput = Console.Out;
                    sw.Flush();
                    Console.Write(sw.Encoding.GetString(ms.ToArray()));
                    WriteColoured(Console.Out, "Output Resumed", ConsoleColor.Green);
                    Console.WriteLine();
                }
            }
        }

        private static AutoResetEvent _pingEvent = new AutoResetEvent(false);
        private static TimeSpan _pingInterval = TimeSpan.FromMinutes(5);
        static HashSet<int> _ntsStations;
        private static DateTimeOffset _lastPing;
        private static bool _sendPing = true;
        private static void PingLoop()
        {
            // Give the serial port time to connect:
            Thread.Sleep(1000);
            //         0  1  2  345678
            // Ping is P{00}{00}KN6DUC
            // X is prepended by modem
            byte i = 0;
            while (true)
            {
                try
                {
                    if (_sendPing)
                    {
                        SendPing(i);
                        unchecked
                        {
                            i++;
                        }
                    }
                    _lastPing = DateTimeOffset.Now;
                    _pingEvent.WaitOne(_pingInterval);
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"Ping Error ({i}): {ex}");
                    Thread.Sleep(TimeSpan.FromMinutes(1));
                }
            }
        }

        public static void SendPing(byte packetUID)
        {
            uint timestamp = (uint)DateTimeOffset.Now.ToUnixTimeSeconds();
            byte[] ping = Encoding.ASCII.GetBytes("P0#" + _callSign)
                .Concat(BitConverter.GetBytes(timestamp))
                .ToArray();
            //ping[0] |= 0x80; // Demand relay
            ping[1] = 0x00; //Addressed to all stations (any station which is set to relay commands will also relay the ping).
            ping[2] = packetUID; //This is used for relay tracking
            _dataReceiver.Write(ping);
            OutputWriter.WriteLine($"Ping sent: {packetUID}");
        }

        private static Task TryForever(Action A)
        {
            return Task.Run(() =>
            {
                while (true)
                    try
                    {
                        A();
                        return;
                    }
                    catch (Exception ex)
                    {
                        //Just retry forever...
                        Thread.Sleep(1000);
                        Console.Error.WriteLine($"{DateTime.Now} Connect error: {ex}");
                    }
            });
        }

        private static void CurrentDomain_ProcessExit(object sender, EventArgs e)
        {
            _dataReceiver.Disconnect();
            _exitingSource.Cancel();
        }

        private static void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
        }

        private static void TaskScheduler_UnobservedTaskException(object sender, UnobservedTaskExceptionEventArgs e)
        {
            Console.WriteLine(e.Exception);
            e.SetObserved();
        }

        public static void DoTest()
        {
            byte[][] data = {
                new byte []{ (byte)'W', (byte)'C', 1, 3, 0, 1, 2 },
                new byte []{ (byte)'W', (byte)'C', 2, 3, 0, 2, 2 },
                new byte []{ (byte)'W', (byte)'C', 3, 3, 0, 5, 2 },
                new byte []{ (byte)'W', (byte)'C', 4, 3, 0, 100, 2 },
                new byte []{ (byte)'W', (byte)'C', 5, 3, 0, 5, 2 },
                new byte []{ (byte)'W', (byte)'C', 6, 3, 0, 3, 2 },
            };
            foreach (var packet in data)
            {
                _dataReceiver.Write(packet);
                Console.WriteLine($"Sent {packet.ToCsv()}");
                Thread.Sleep(2000);
            }
        }
    }
}
