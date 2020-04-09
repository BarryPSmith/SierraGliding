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

namespace core_Receiver
{
    static class Program
    {
        const string CallSign = "KN6DUC";

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
"
);
        }

        static ICommunicator _dataReceiver;

        static public volatile TextWriter ErrorWriter = Console.Error;
        static public volatile TextWriter OutputWriter = Console.Out;

        static List<DataPosting> _serverPosters;

        static string _dbFn;
        static List<string> _destUrls = new List<string>();
        static bool _connectUDP = false;
        static int? _incomingPort, _outgoingPort;
        static string _srcNetworkAddress = null;
        static string _serialAddress = null;
        static string _npsFn = null;

        static CancellationTokenSource _exitingSource = new CancellationTokenSource();

        static void ParseArgs(string[] args)
        {
            var dbIndex = Array.FindIndex(args, arg => arg.Equals("--db", StringComparison.OrdinalIgnoreCase));
            if (dbIndex >= 0)
                _dbFn = args[dbIndex + 1];

            for (int idx = Array.FindIndex(args, arg => arg.Equals("--dest", StringComparison.OrdinalIgnoreCase));
                     idx >= 0;
                     idx = Array.FindIndex(args, idx + 1, arg => arg.Equals("--dest", StringComparison.OrdinalIgnoreCase)))
                _destUrls.Add(args[idx + 1]);

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

            FileStream npsFs = null;
            bool npsIsStandardOut = true;

            if (!string.IsNullOrEmpty(_dbFn))
                _dataStore = new LocalDatastore(_dbFn);

            List<Task> runTasks;

            _serverPosters = _destUrls.Select(url =>
            {
                var ret = new DataPosting(url);
                ret.OnException += (sender, ex) => 
                    ErrorWriter.WriteLine($"Post error ({url}): {ex.GetType()}: {ex.Message}");
                return ret;
            }).ToList();

            if (_connectUDP)
            {
                var udpListener = new SocketListener(_incomingPort.Value, _outgoingPort.Value);
                _dataReceiver = udpListener;
                runTasks = new List<Task> { udpListener.StartAsync(CancellationToken.None) };
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
                    var udpListener = new SocketListener(_incomingPort.Value, _outgoingPort.Value);
                    runTasks.Add(udpListener.StartAsync(_exitingSource.Token));
                    communicator.PacketReceived += (sender, e) => udpListener.Write(e.ToArray());
                    udpListener.PacketReceived += (sender, e) => communicator.Write(e.ToArray());
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

        private static void OnPacketReceived(object sender, IList<byte> data)
        {
            DateTime receivedTime = DateTime.Now;
            try
            {
                var packet = PacketDecoder.DecodeBytes(data as byte[] ?? data.ToArray());
                if (packet == null)
                {
                    var localBytes = data.ToArray();
                    Task.Run(() => _dataStore?.RecordUndecipherable(localBytes));
                    Task.Run(() =>
                    {
                        OutputWriter.Write(receivedTime.ToString());
                        OutputWriter.Write(" Unhandled Packet: ");
                        OutputWriter.WriteLine(OutputWriter.Encoding.GetString(localBytes));
                    });
                    return;
                }
                //Do this in a Task to avoid waiting if we've scrolled up.
                var consoleTask = Task.Run(() => OutputWriter.WriteLine($"Packet {receivedTime}: {packet.ToString()}"));
                switch (packet.type)
                {
                    case PacketTypes.Weather:
                        foreach (var serverPoster in _serverPosters)
                            serverPoster?.SendWeatherDataAsync(packet, receivedTime);
                        break;
                    case PacketTypes.Response:
                    case PacketTypes.Command:
                    case PacketTypes.Ping:
                    case PacketTypes.Modem:
                        break;
                    default:
                        //nps is often stdOut in debugging, don't write to it from multiple threads:
                        var localBytes = data.ToArray();
                        consoleTask.ContinueWith(notUsed =>
                        {
                            OutputWriter?.Write($"{DateTime.Now} Unhandled Packet: ");
                            OutputWriter?.WriteLine(OutputWriter.Encoding.GetString(localBytes));
                        });
                        break;
                }
                Task.Run(() => _dataStore?.RecordPacket(packet));
                
            }
            catch (Exception ex)
            {
                ErrorWriter.WriteLine($"Error {DateTime.Now}: {ex}");
            }
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
                tasks.Add(Task.Run(() =>
                {
                    kisser.ConnectSerial(serialAddress);
                }));
            }
            StartPingLoop();
            return tasks;
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
                    var key = Console.ReadKey();
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
                }
            }
        }

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
                    byte[] ping = Encoding.ASCII.GetBytes("P0#" + CallSign);
                    //ping[0] |= 0x80; // Demand relay
                    ping[1] = 0x00; //Addressed to all stations (any station which is set to relay commands will also relay the ping).
                    ping[2] = i; //This is used for relay tracking
                    _dataReceiver.Write(ping);
                    OutputWriter.WriteLine($"Ping sent: {i}");
                    unchecked
                    {
                        i++;
                    }
                    Thread.Sleep(TimeSpan.FromMinutes(5));
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"Ping Error ({i}): {ex}");
                    Thread.Sleep(TimeSpan.FromMinutes(1));
                }
            }
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
    }
}
