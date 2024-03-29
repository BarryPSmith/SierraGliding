﻿using core_Receiver.Packets;
using System;
using System.Collections.Generic;
using System.Data;
using System.Data.SQLite;
using System.Linq;
using System.Text;

namespace core_Receiver
{
    class LocalDatastore
    {
        public bool StoreWeather { get; set; }

        SQLiteConnection _dbConn;
        public LocalDatastore(string fn)
        {
            SQLiteConnectionStringBuilder csb = new SQLiteConnectionStringBuilder
            {
                ForeignKeys = true,
                DataSource = fn
            };
            _dbConn = new SQLiteConnection(csb.ToString());
            _dbConn.Open();
            using var cmd = _dbConn.CreateCommand();
            cmd.CommandText = @"
                CREATE TABLE IF NOT EXISTS All_Packets (
                    Timestamp INTEGER NOT NULL,
                    Station_ID INTEGER NOT NULL,
                    Type INTEGER NOT NULL,
                    Unique_ID INTEGER NOT NULL,
                    To_String TEXT NULL,
                    Data BLOB NULL
                );

                CREATE INDEX IF NOT EXISTS idx_packet_timestamp ON All_Packets(Timestamp);

                CREATE TABLE IF NOT EXISTS Undecipherable (
                    Timestamp INTEGER NOT NULL,
                    Data BLOB NOT NULL
                );
            
                CREATE TABLE IF NOT EXISTS ProgramLog (
                    Timestamp INTEGER NOT NULL,
                    Message TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS Packet_Stats(
                    Timestamp INTEGER NOT NULL,
                    Station_Id INTEGER NOT NULL,
                    Type INTEGER NOT NULL,
                    RSSI REAL NOT NULL,
                    SNR REAL NOT NULL
                );
                CREATE INDEX IF NOT EXISTS id_stat_timestamp ON Packet_Stats(Timestamp);

                CREATE VIEW IF NOT EXISTS View_PacketStats AS
                SELECT
                    DateTime(Timestamp, 'unixepoch', 'localtime') AS FriendlyTime,
                    CASE WHEN (Station_ID BETWEEN 32 AND 126) THEN
                        CHAR(Station_ID)
                    ELSE
                        NULL
                    END                                                 AS Station_ID_Char,

                    CASE WHEN (Type BETWEEN 32 AND 126) THEN
                        CHAR(Type)
                    ELSE
                        NULL
                    END                                                 AS Type_Char,
                    *
                FROM Packet_Stats;

                CREATE VIEW IF NOT EXISTS View_All_Packets AS
                SELECT
                    Timestamp AS TS_Num,
                    DateTime(Timestamp, 'unixepoch', 'localtime')       AS Timestamp,

                    CASE WHEN (Station_ID BETWEEN 32 AND 126) THEN
                        CHAR(Station_ID)
                    ELSE
                        NULL
                    END                                                 AS Station_ID_Char,

                    Station_ID,

                    CASE WHEN (Type BETWEEN 32 AND 126) THEN
                        CHAR(Type)
                    ELSE
                        NULL
                    END                                                 AS Type_Char,

                    Type,
                    Unique_ID,
                    To_String,
                    Data
                FROM All_Packets;

                CREATE VIEW IF NOT EXISTS View_Undecipherable AS
                SELECT DateTime(Timestamp, 'unixepoch', 'localtime') AS Timestamp, Data FROM Undecipherable;

                CREATE VIEW IF NOT EXISTS View_ProgramLog AS
                SELECT DateTime(Timestamp, 'unixepoch', 'localtime') AS Timestamp, Message FROM ProgramLog;
                ";
            cmd.ExecuteNonQuery();
            using var cmd2 = _dbConn.CreateCommand();
            cmd2.CommandText = "INSERT INTO ProgramLog (Timestamp, Message) VALUES ($Timestamp, $Message);";
            cmd2.Parameters.AddWithValue("$Timestamp", DateTimeOffset.UtcNow.ToUnixTimeSeconds());
            cmd2.Parameters.AddWithValue("$Message", "Startup");
            cmd2.ExecuteNonQuery();

            Console.WriteLine($"Database Initialisation Successful with filename '{_dbConn.DataSource}'.");
        }

        public void RecordUndecipherable(byte[] bytes)
        {
            if (_dbConn == null)
                return;
            if (bytes == null || !bytes.Any())
                return;

            using var cmd = _dbConn.CreateCommand();
            cmd.CommandText = "INSERT INTO Undecipherable (Timestamp, Data) Values ($Timestamp, $Data)";
            cmd.Parameters.AddWithValue("$Timestamp", DateTimeOffset.UtcNow.ToUniversalTime().ToUnixTimeSeconds());
            cmd.Parameters.AddWithValue("$Data", bytes);
            cmd.ExecuteNonQuery();
        }

        public void RecordPacket(Packet p)
        {
            if (_dbConn == null)
                return;
            if (p.type == PacketTypes.Stats)
                return;
            if (StoreWeather || p.type != PacketTypes.Weather || 
                (p.packetData as IList<SingleWeatherData>)?.Any(pd => pd.extras?.Length > 0) == true)
            {
                using (var cmd = _dbConn.CreateCommand())
                {
                    cmd.CommandText = @"INSERT INTO All_Packets
                    (Timestamp, Station_ID, Type, Unique_ID, To_String, To_String, Data)
                    VALUES
                    ($Timestamp, $Station_ID, $Type, $Unique_ID, $To_String, $To_String, $Data);";
                    cmd.Parameters.AddWithValue("$Timestamp", DateTimeOffset.Now.ToUniversalTime().ToUnixTimeSeconds());
                    cmd.Parameters.AddWithValue("$Station_ID", p.sendingStation);
                    cmd.Parameters.AddWithValue("$Type", (byte)p.type);
                    cmd.Parameters.AddWithValue("$Unique_ID", p.uniqueID);
                    var dataString = p.SafeDataString;
                    dataString = dataString?.Replace((char) 0, ' ').Trim();

                    cmd.Parameters.AddWithValue("$To_String", (object)dataString ?? DBNull.Value);
                    Packet interiorPacket = p;
                    //Unwrap our packets, as we might find after querying the station data store:
                    while (interiorPacket.packetData is Packet dataPacket)
                        interiorPacket = dataPacket;
                    if (interiorPacket.packetData is IEnumerable<byte> data)
                        cmd.Parameters.AddWithValue("$Data", data.ToArray());
                    else
                        cmd.Parameters.AddWithValue("$Data", (object)p.originalData ?? DBNull.Value);
                    cmd.ExecuteNonQuery();
                }
            }
        }

        public void RecordStats(Packet packet, StatsResponse stats)
        {
            if (_dbConn == null)
                return;

            using var cmd = _dbConn.CreateCommand();
            cmd.CommandText = "INSERT INTO Packet_Stats " +
                "(Timestamp, Station_ID, Type, RSSI, SNR) " +
                "VALUES ($Timestamp, $Station_ID, $Type, $RSSI, $SNR)";
            cmd.Parameters.AddWithValue("$Timestamp", DateTimeOffset.UtcNow.ToUniversalTime().ToUnixTimeSeconds());
            cmd.Parameters.AddWithValue("$Station_ID", packet.sendingStation);
            cmd.Parameters.AddWithValue("$Type", (byte)packet.type);
            cmd.Parameters.AddWithValue("$RSSI", stats.RSSI);
            cmd.Parameters.AddWithValue("$SNR", stats.SNR);
            cmd.ExecuteNonQuery();
        }
    }
}
