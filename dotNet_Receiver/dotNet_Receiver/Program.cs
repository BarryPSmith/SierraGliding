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
        static void Main(string[] args)
        {
            TaskScheduler.UnobservedTaskException += TaskScheduler_UnobservedTaskException;
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;

            string fn = @"C:\temp\data3.sqlite";
            string log = @"c:\temp\stationLog.csv";
            string url = "http://localhost:4000";

            var communicator = new KissCommunication();
            DataStorage storage = fn != null ? new DataStorage(fn) : null;
            var serverPoster = url != null ? new DataPosting(url) : null;
            
            communicator.PacketReceived =
                data =>
                {
                    DateTime receivedTime = DateTime.Now;
                    try
                    {
                        var packet = PacketDecoder.DecodeBytes(data as byte[] ?? data.ToArray());
                        //Do this in a Task to avoid waiting if we've scrolled up.
                        Task.Run(() => Console.WriteLine($"Packet {DateTime.Now}: {packet.ToString()}"));
                        switch (packet.type)
                        {
                            case 'W': //Weather data
                                try
                                {
                                    storage?.StoreWeatherData(packet, receivedTime);
                                }
                                catch (Exception ex)
                                {
                                    Console.Error.WriteLine($"Store Error {DateTime.Now}: {ex}");
                                }
                                serverPoster?.SendWeatherDataAsync(packet, receivedTime);
                                break;
                            case 'D': //Wind direciton debug data
                                File.AppendAllLines(log, new[] { packet.GetDataString(packet.packetData) });
                                break;
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.Error.WriteLine($"Error {DateTime.Now}: {ex}");
                    }
                };

            Task.Run(() => communicator.Connect("127.0.0.1", 8001));

            Console.WriteLine("Reading data. Press any key to exit.");
            Console.ReadKey();
            communicator.Disconnect();
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
