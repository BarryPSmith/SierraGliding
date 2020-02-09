using core_Receiver.Packets;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace core_Receiver
{
    class DataPosting : DataStorageBase
    {
        private static readonly HttpClient _client = new HttpClient();

        public event EventHandler<Exception> OnException;
        
        readonly string _url;

        public TextWriter OutputWriter => Program.OutputWriter;

        public DataPosting(string url)
        {
            _url = url;
        }

        public async Task SendWeatherDataAsync(Packet packet, DateTime receivedTime)
        {
            if (packet.type != 'W')
                throw new InvalidOperationException();

            var now = new DateTimeOffset(receivedTime).ToUniversalTime();

            foreach (var subPacket in packet.packetData as IList<SingleWeatherData>)
            {
                var identifier = new PacketIdentifier()
                {
                    packetType = 'W',
                    stationID = subPacket.sendingStation,
                    uniqueID = subPacket.uniqueID
                };

                if (IsPacketDoubleReceived(identifier, now))
                    continue;
                receivedPacketTimes[identifier] = now;

                var toSerialize = new
                {
                    timestamp = now.ToUnixTimeSeconds(),
                    wind_speed = subPacket.windSpeed,
                    wind_direction = subPacket.windDirection,
                    battery = subPacket.batteryLevelH,
                    external_temp = subPacket.externalTemp,
                    internal_temp = subPacket.internalTemp
                };
                var json = JsonConvert.SerializeObject(toSerialize);
                var content = new StringContent(json, Encoding.UTF8, "application/json");
                var url = _url + "/api/station/" + subPacket.sendingStation + "/data";
                var uri = new Uri(url);

                try
                {
                    var response = await _client.PostAsync(uri, content);
                    if (response.IsSuccessStatusCode)
                        OutputWriter.WriteLine($"Succesfully posted {packet.sendingStation}/{packet.type}{packet.uniqueID} to {url}.");
                    else
                    {
                        OutputWriter.WriteLine($"Post of {packet.sendingStation}/{packet.type}{packet.uniqueID} to {url} failed ({response.StatusCode}): {response.ReasonPhrase}");
                        OutputWriter.WriteLine($"Content: {response.Content.ReadAsStringAsync().Result}");
                        OutputWriter.WriteLine($"JSON: {json}");
                    }
                }
                catch (Exception ex)
                {
                    OnException?.Invoke(this, ex);
                }
            }
        }
    }
}
