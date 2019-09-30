using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace dotNet_Receiver
{
    class Program
    {
        static void Main(string[] args)
        {
            string fn = @"C:\temp\data3.sqlite";
            string log = @"c:\temp\stationLog.csv";

            var communicator = new KissCommunication();
            DataStorage storage = new DataStorage(fn);
            
            communicator.PacketReceived =
                data =>
                {
                    try
                    {
                        var packet = PacketDecoder.DecodeBytes(data as byte[] ?? data.ToArray());
                        Console.WriteLine($"Packet {DateTime.Now}: {packet.ToString()}");
                        switch (packet.type)
                        {
                            case 'W': //Weather data
                                storage.StoreWeatherData(packet);
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
    }
}
