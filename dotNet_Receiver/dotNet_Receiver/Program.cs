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
 --db <DB.sqlite>       Database to store data locally
 --dest <address:port>  Destionation web server. Default http://localhost:4000. Can be specified multiple times to send data to multiple servers.
"
);
        }

        static KissCommunication _dataReceiver = new KissCommunication();

        static void Main(string[] args)
        {
            TaskScheduler.UnobservedTaskException += TaskScheduler_UnobservedTaskException;
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;

            AppDomain.CurrentDomain.ProcessExit += CurrentDomain_ProcessExit;

#if local_storage
            string fn = null;// @"C:\temp\data3.sqlite";
#endif
            //string log = @"c:\temp\stationLog.csv";
            List<string> destUrls = new List<string>(); // "http://localhost:4000";
            string srcAddress = "127.0.0.1:8001";

            var srcArgIndex = Array.FindIndex(args, arg => arg.Equals("--src", StringComparison.OrdinalIgnoreCase));
            if (srcArgIndex >= 0)
                srcAddress = args[srcArgIndex + 1];

            for (int idx = Array.FindIndex(args, arg => arg.Equals("--dest", StringComparison.OrdinalIgnoreCase));
                     idx >= 0;
                     idx = Array.FindIndex(args, idx + 1, arg => arg.Equals("--dest", StringComparison.OrdinalIgnoreCase)))
                destUrls.Add(args[idx + 1]);
            if (!destUrls.Any())
                destUrls.Add("http://localhost:4000");

#if local_storage
            var dbArgIdx = Array.FindIndex(args, arg => arg.Equals("--db", StringComparison.OrdinalIgnoreCase));
            if (dbArgIdx >= 0)
                fn = args[dbArgIdx + 1];
#endif

#if local_storage
            DataStorage storage = fn != null ? new DataStorage(fn) : null;
#endif
            var serverPosters = destUrls.Select(url =>
            {
                var ret = new DataPosting(url);
                ret.OnException += (sender, ex) => Console.Error.WriteLine($"Post error ({url}): {ex.GetType()}: {ex.Message}");
                return ret;
            }).ToList();
            
            _dataReceiver.PacketReceived =
                data =>
                {
                    DateTime receivedTime = DateTime.Now;
                    try
                    {
                        var packet = PacketDecoder.DecodeBytes(data as byte[] ?? data.ToArray());
                        //Do this in a Task to avoid waiting if we've scrolled up.
                        Task.Run(() => Console.WriteLine($"Packet {receivedTime}: {packet.ToString()}"));
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
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.Error.WriteLine($"Error {DateTime.Now}: {ex}");
                    }
                };

            if (!Console.IsInputRedirected)
            {
                Task.Run(() => _dataReceiver.Connect(srcAddress));
                Console.WriteLine("Reading data. Press q key to exit.");
                while (true)
                {
                    var key = Console.ReadKey();
                    if (key.KeyChar == 'q')
                    {
                        _dataReceiver.Disconnect();
                        return;
                    }
                }
            }
            else
                while (true)
                    try
                    {
                        _dataReceiver.Connect(srcAddress);
                        return;
                    }
                    catch (Exception ex)
                    {
                        //Just retry forever...
                        Thread.Sleep(1000);
                        Console.Error.Write($"{DateTime.Now} Connect error: {ex}");
                    }
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
