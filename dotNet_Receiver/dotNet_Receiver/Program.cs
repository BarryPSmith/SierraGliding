using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace dotNet_Receiver
{
    class Program
    {
        static void Main(string[] args)
        {
            string fn = @"C:\temp\data.sqlite";

            var communicator = new KissCommunication();
            DataStorage storage = new DataStorage(fn);

            communicator.PacketReceived =
                data =>
                {
                    var packet = PacketDecoder.DecodeBytes(data as byte[] ?? data.ToArray());
                    Console.WriteLine($"{DateTime.Now}: {packet.ToString()}");
                    if (packet.type == 'W')
                        storage.StoreWeatherData(packet);
                };

            Task.Run(() => communicator.Connect("127.0.0.1", 8001));

            Console.WriteLine("Reading data. Press any key to exit.");
            Console.ReadKey();
            communicator.Disconnect();
        }
    }
}
