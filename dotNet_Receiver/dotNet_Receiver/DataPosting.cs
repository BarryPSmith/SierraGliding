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

        public bool LogSuccessfulPosts { get; set; } = false;
        
        readonly string _url;

        public int Offset { get; set; }
        public TextWriter OutputWriter => Program.OutputWriter;

        public DataPosting(string url)
        {
            _url = url;
        }

        class ServerError
        {
            public string Error { get; set; }
            public string Status { get; set; }
        }

        void CalculatePacketTimes(IList<SingleWeatherData> subPackets,
            DateTimeOffset now, byte sendingStation)
        {
            var groupedPackets = subPackets.GroupBy(p => p.sendingStation);
            foreach (var grp in groupedPackets.Where(grp => grp.Count() > 1))
            {
                byte spaceAfter(SingleWeatherData d)
                {
                    if (grp.Count() == 1)
                        return 255;
                    var otherPackets = grp.Where(p => p != d);
                    var nextId = otherPackets.Where(p => p.uniqueID > d.uniqueID)
                        .Min(p => (byte?)p.uniqueID);
                    if (nextId != null)
                        return (byte)(nextId - d.uniqueID);
                    var firstId = otherPackets.Min(p => p.uniqueID);
                    return (byte)(firstId + (255 - d.uniqueID));
                }
                var lastId = grp.OrderBy(p => spaceAfter(p)).Last().uniqueID;
                TimeSpan timeBetweenPackets = TimeSpan.FromSeconds(4);
                foreach (var subPacket in grp)
                {
                    byte idSpace = (byte)(lastId - subPacket.uniqueID);
                    var packetTime = now - idSpace * timeBetweenPackets;
                    subPacket.calculatedTime = packetTime;
                }
            }

            foreach (var subPacket in subPackets.OrderBy(p => p.calculatedTime ?? now))
            {
                var identifier = new PacketIdentifier()
                {
                    packetType = 'W',
                    stationID = subPacket.sendingStation,
                    uniqueID = subPacket.uniqueID
                };
                subPacket.calculatedTime = EstimatePacketArrival(
                    subPacket, now, subPacket.sendingStation == sendingStation);
            }
        }

        public async Task SendWeatherDataAsync(Packet packet, DateTimeOffset receivedTime)
        {
            if (packet.type != PacketTypes.Weather && packet.type != PacketTypes.Overflow
                && packet.type != PacketTypes.Overflow2)
                throw new InvalidOperationException();

            var now = receivedTime.ToUniversalTime();

            var subPackets = packet.packetData as IList<SingleWeatherData>;
            CalculatePacketTimes(subPackets, receivedTime, packet.sendingStation);

            foreach (var subPacket in subPackets.OrderBy(p => p.calculatedTime ?? now))
            {
                var identifier = new PacketIdentifier()
                {
                    packetType = 'W',
                    stationID = subPacket.sendingStation,
                    uniqueID = subPacket.uniqueID
                };

                if (IsPacketDoubleReceived(identifier, subPacket.calculatedTime ?? now))
                    continue;
                receivedPacketTimes[identifier] = subPacket.calculatedTime ?? now;


                var toSerialize = new
                {
                    timestamp = (subPacket.calculatedTime ?? now).ToUnixTimeSeconds(),
                    wind_speed = subPacket.windSpeed,
                    wind_direction = subPacket.windDirection,
                    battery = subPacket.batteryLevelH,
                    external_temp = subPacket.externalTemp,
                    internal_temp = subPacket.internalTemp,
                    wind_gust = subPacket.gust,
                    pwm = subPacket.pwmValue,
                    current = subPacket.current,
                };
                var json = JsonConvert.SerializeObject(toSerialize);
                var content = new StringContent(json, Encoding.UTF8, "application/json");
                var url = _url + $"/api/station/{subPacket.sendingStation + Offset}/data";
                var uri = new Uri(url);

                try
                {
                    var response = await _client.PostAsync(uri, content);
                    if (response.IsSuccessStatusCode)
                    {
                        if (LogSuccessfulPosts)
                            OutputWriter.WriteLine($"Succesfully posted {subPacket.sendingStation}/{packet.type.ToChar()}{subPacket.uniqueID} to {url}.");
                    }
                    else
                    {
                        var err = JsonConvert.DeserializeObject<ServerError>(await response.Content.ReadAsStringAsync());
                        OutputWriter.WriteLine($"Post of {packet.sendingStation}/{packet.type.ToChar()}{packet.uniqueID:X2} to {url} failed ({response.StatusCode}): {err.Error}");
                        /*OutputWriter.WriteLine($"Content: {response.Content.ReadAsStringAsync().Result}");
                        OutputWriter.WriteLine($"JSON: {json}");*/
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
