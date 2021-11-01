using System;
using System.Collections.Generic;

namespace core_Receiver
{
    //TODO: Rename this something more useful
    class DataStorageBase
    {
        protected struct PacketIdentifier
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

        /// <summary>
        /// If two packets with the same sending station and identifier are received within this period, the second will be ignored.
        /// </summary>
        public TimeSpan InvalidationTime { get; set; } = TimeSpan.FromMinutes(1);

        protected Dictionary<PacketIdentifier, DateTimeOffset> receivedPacketTimes = new Dictionary<PacketIdentifier, DateTimeOffset>();

        protected bool IsPacketDoubleReceived(PacketIdentifier identifier, DateTimeOffset now)
            => receivedPacketTimes.TryGetValue(identifier, out var lastReceivedTime) &&
                lastReceivedTime + InvalidationTime > now;
    }
}
