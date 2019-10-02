using System;
using System.Collections.Generic;
using System.Data.SQLite;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace dotNet_Receiver
{
    class DataStorage
    {
        string _fn;
        string _connectionString;

        /// <summary>
        /// If two packets with the same sending station and identifier are received within this period, the second will be ignored.
        /// </summary>
        public TimeSpan InvalidationTime { get; set; } = TimeSpan.FromMinutes(2);

        public DataStorage(string fn)
        {
            _fn = fn;
            SQLiteConnectionStringBuilder csb = new SQLiteConnectionStringBuilder();
            csb.DataSource = fn;
            csb.ForeignKeys = true;
            csb.FailIfMissing = true;
            csb.BusyTimeout = 10000;
            _connectionString = csb.ToString();
        }

        struct PacketIdentifier
        {
            public char packetType;
            public byte stationID;
            public byte uniqueID;

            public override int GetHashCode()
                => packetType.GetHashCode() ^ (stationID.GetHashCode() << 8) ^ (uniqueID.GetHashCode() << 16);

            public override bool Equals(object obj)
            {
                if (!(obj is PacketIdentifier))
                    return false;
                var other = (PacketIdentifier)obj;
                return packetType == other.packetType &&
                    stationID == other.stationID &&
                    uniqueID == other.uniqueID;
            }

            public override string ToString()
                => $"{packetType} {stationID} {uniqueID}";

        }

        Dictionary<PacketIdentifier, DateTimeOffset> receivedPacketTimes = new Dictionary<PacketIdentifier, DateTimeOffset>();

        public void StoreWeatherData(Packet packet)
        {
            //TODO: Handle 'R' packets
            if (packet.type != 'W')
                throw new InvalidOperationException("Can only store W packets");

            var thisTime = new DateTimeOffset(DateTime.Now);
            var now = thisTime.ToUniversalTime();

            using (var conn = new SQLiteConnection(_connectionString))
            {
                conn.Open();
                using (var tx = conn.BeginTransaction())
                {
                    using (var cmd = new SQLiteCommand(
                        "INSERT INTO station_data " +
                        "(Station_ID, Timestamp, Windspeed, Wind_Direction, Battery_Level) " +
                        "VALUES " +
                        "(:id,:dateTime,:windSpeed,:windDirection,:battery)",
                        conn))
                    {
                        cmd.Transaction = tx;
                        foreach (var subPacket in packet.packetData as IList<SingleWeatherData>)
                        {
                            var identifier = new PacketIdentifier()
                            {
                                packetType = 'W',
                                stationID = subPacket.sendingStation,
                                uniqueID = subPacket.uniqueID
                            };
                            //Ignore packets we've seen recently. Perhaps they've been double relayed.
                            //TODO: Log this.
                            if (receivedPacketTimes.TryGetValue(identifier, out var lastReceivedTime) &&
                                lastReceivedTime + InvalidationTime > now)
                                continue;
                            receivedPacketTimes[identifier] = now;

                            cmd.Parameters.Add(new SQLiteParameter("id", subPacket.sendingStation));
                            cmd.Parameters.Add(new SQLiteParameter("dateTime", now.ToUnixTimeSeconds())); //TODO: Fancy logic if we get multiple packets from the same station in a single relay.
                            cmd.Parameters.Add(new SQLiteParameter("windSpeed", subPacket.windSpeed));
                            cmd.Parameters.Add(new SQLiteParameter("windDirection", subPacket.windDirection));
                            cmd.Parameters.Add(new SQLiteParameter("battery", subPacket.batteryLevelH));
                            cmd.ExecuteNonQuery();
                        }
                    }
                    tx.Commit();
                }
                conn.Close();
            }
        }
    }
}
