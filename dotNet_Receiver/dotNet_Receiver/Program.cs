using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace dotNet_Receiver
{
    class Program
    {
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
"
);
        }

        static KissCommunication _dataReceiver = new KissCommunication();

        static void Main(string[] args)
        {
#if DEBUG
            for (int i = 0; i < args.Length; i++)
                Console.WriteLine($"{i}: {args[i]}");
#endif

            if (!(args?.Length > 0))
            {
                Help();
                return;
            }

            TaskScheduler.UnobservedTaskException += TaskScheduler_UnobservedTaskException;
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;

            AppDomain.CurrentDomain.ProcessExit += CurrentDomain_ProcessExit;
#if local_storage
            string fn = null;// @"C:\temp\data3.sqlite";
#endif
            List<string> destUrls = new List<string>(); // "http://localhost:4000";
            string srcAddress = null; 
            string serialAddress = null;
            string npsFn = null;

            var srcArgIndex = Array.FindIndex(args, arg => arg.Equals("--src", StringComparison.OrdinalIgnoreCase));
            if (srcArgIndex >= 0)
                srcAddress = args[srcArgIndex + 1];
            
            var serialArgIndex = Array.FindIndex(args, arg => arg.Equals("--serial", StringComparison.OrdinalIgnoreCase));
            if (serialArgIndex >= 0 && args.Length > serialArgIndex + 1)
                serialAddress = args[serialArgIndex + 1];
            if (serialArgIndex < 0)
                serialArgIndex = Array.FindIndex(args, arg => arg.Equals("--serialFirst", StringComparison.OrdinalIgnoreCase));
            
            if (srcArgIndex < 0 && serialArgIndex < 0)
                srcAddress = "127.0.0.1:8001";

            for (int idx = Array.FindIndex(args, arg => arg.Equals("--dest", StringComparison.OrdinalIgnoreCase));
                     idx >= 0;
                     idx = Array.FindIndex(args, idx + 1, arg => arg.Equals("--dest", StringComparison.OrdinalIgnoreCase)))
                destUrls.Add(args[idx + 1]);
            if (!destUrls.Any())
                destUrls.Add("http://localhost:4000");

            var npsFnIndex = Array.FindIndex(args, arg => arg.Equals("--nonPacket", StringComparison.OrdinalIgnoreCase));
            if (npsFnIndex >= 0)
                npsFn = args[npsFnIndex + 1];

            Console.WriteLine("srcAddress: " + srcAddress);

#if local_storage
            var dbArgIdx = Array.FindIndex(args, arg => arg.Equals("--db", StringComparison.OrdinalIgnoreCase));
            if (dbArgIdx >= 0)
                fn = args[dbArgIdx + 1];
            DataStorage storage = fn != null ? new DataStorage(fn) : null;
#endif
            var serverPosters = destUrls.Select(url =>
            {
                var ret = new DataPosting(url);
                ret.OnException += (sender, ex) => Console.Error.WriteLine($"Post error ({url}): {ex.GetType()}: {ex.Message}");
                return ret;
            }).ToList();

            Stream nps = null;
            FileStream npsFs = null;
            if (string.IsNullOrEmpty(npsFn))
            {
                nps = Console.OpenStandardOutput();
            }
            else if (!npsFn.Equals("DISCARD", StringComparison.OrdinalIgnoreCase))
            {
                nps = npsFs = new FileStream(npsFn, FileMode.Append, FileAccess.Write, FileShare.ReadWrite | FileShare.Delete, 4096);
            }
            _dataReceiver.NonPacketStream = nps;

            _dataReceiver.PacketReceived =
                data =>
                {
                    DateTime receivedTime = DateTime.Now;
                    try
                    {
                        var packet = PacketDecoder.DecodeBytes(data as byte[] ?? data.ToArray());
                        //Do this in a Task to avoid waiting if we've scrolled up.
                        var consoleTask = Task.Run(() => Console.WriteLine($"Packet {receivedTime}: {packet.ToString()}"));
                        switch (packet.type)
                        {
                            case 'W': //Weather data
#if local_storage
                                try
                                {
                                    storage?.StoreWeatherData(packet, receivedTime);
                                }
                                catch (Exception ex)
                                {
                                    Console.Error.WriteLine($"Store Error {DateTime.Now}: {ex}");
                                }
#endif
                                foreach (var serverPoster in serverPosters)
                                    serverPoster?.SendWeatherDataAsync(packet, receivedTime);
                                break;
#if false
                            case 'D': //Wind direciton debug data
                                File.AppendAllLines(log, new[] { packet.GetDataString(packet.packetData) });
                                break;
#endif
                            default:
                                //nps is often stdOut in debugging, don't write to it from multiple threads:
                                var localBytes = data.ToArray();
                                consoleTask.ContinueWith(notUsed =>
                                {
                                    nps?.Write(Encoding.UTF8.GetBytes("Unhandled Packet: "));
                                    nps?.Write(localBytes);
                                    nps?.Write(Encoding.UTF8.GetBytes("\n"));
                                });
                                break;
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.Error.WriteLine($"Error {DateTime.Now}: {ex}");
                    }
                };

            try
            {
                if (!Console.IsInputRedirected)
                {
                    List<Task> tasks = new List<Task>();
                    if (!string.IsNullOrEmpty(srcAddress))
                        tasks.Add(Task.Run(() => _dataReceiver.Connect(srcAddress)));
                    if (serialArgIndex >= 0)
                        tasks.Add(Task.Run(() => _dataReceiver.ConnectSerial(serialAddress)));

                    Console.WriteLine("Reading data. Press q key to exit.");
                    while (true)
                    {
                        var key = Console.ReadKey();
                        if (key.KeyChar == 'q')
                        {
                            Console.WriteLine("Shutting Down...");
                            _dataReceiver.Disconnect();
                            Task.WaitAll(tasks.ToArray());
                            return;
                        }
                    }
                }
                else
                {
                    List<Task> tasks = new List<Task>();
                    if (!string.IsNullOrEmpty(srcAddress))
                        tasks.Add(TryForever(() => _dataReceiver.Connect(srcAddress)));
                    if (serialArgIndex >= 0)
                        tasks.Add(TryForever(() => _dataReceiver.ConnectSerial(serialAddress)));
                    Task.WaitAll(tasks.ToArray());
                }
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
