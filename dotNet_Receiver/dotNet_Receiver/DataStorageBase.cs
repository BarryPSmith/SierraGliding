using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

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

        protected ConcurrentDictionary<PacketIdentifier, DateTimeOffset> receivedPacketTimes = new ConcurrentDictionary<PacketIdentifier, DateTimeOffset>();

        protected bool IsPacketDoubleReceived(PacketIdentifier identifier, DateTimeOffset now)
            => receivedPacketTimes.TryGetValue(identifier, out var lastReceivedTime) &&
                lastReceivedTime + InvalidationTime > now;

        protected DateTimeOffset EstimatePacketArrival(PacketIdentifier identifier, DateTimeOffset expected, DateTimeOffset now)
        {
            TimeSpan maxSearch = TimeSpan.FromMinutes(5);
            double maxSpan = 40;

            var test = identifier;
            List<(int uid, DateTimeOffset received)> toTest = new List<(int, DateTimeOffset)>(20);
            for (int i = 0; i < 20; i++)
            {
                test.uniqueID--;
                if (receivedPacketTimes.TryGetValue(test, out var lastReceived))
                    if (lastReceived > expected - maxSearch)
                        toTest.Add((test.uniqueID, lastReceived));
            }
            // Remove any that are obviously out of order. Although we should never actually have a problem with this.
            // And maybe it does more harm than good.
            toTest.Reverse();
            for (int i = toTest.Count - 1; i > 0; i--)
                if (toTest[i].received < toTest.Take(i).Max(tpl => tpl.received))
                {
                    //toTest.RemoveAt(i);
                }
            // We cannot check if the first one is out of order... so remove it for in case.
            /*if (toTest.Count > 0)
                toTest.RemoveAt(0);*/
            // Only estimate if we have at least 5 records
            if (toTest.Count < 5)
            {
                Debug.WriteLine($"{identifier.stationID},{identifier.uniqueID}, Skipped for no data");
                return expected;
            }
            // Put the expected value as a data point to slowly correct any errors:
            toTest.Add((identifier.uniqueID, expected));

            var maybeBestFitParams = GenerateLinearBestFit(toTest.Select(tpl => 
                ((double)(byte)(tpl.uid - identifier.uniqueID - 1), (double)tpl.received.ToUnixTimeSeconds())).ToList());
            if (maybeBestFitParams == null)
            {
                Debug.WriteLine($"{identifier.stationID},{identifier.uniqueID}, Skipped for no fit");
                return expected;
            }
            var bestFitParams = maybeBestFitParams.Value;
            if (bestFitParams.r2 < 0.7 || bestFitParams.a > maxSpan)
            {
                Debug.WriteLine($"{identifier.stationID},{identifier.uniqueID}, Skipped for bad fit ({bestFitParams.r2})");
                return expected;
            }
            var calculated = DateTimeOffset.FromUnixTimeSeconds((long)(Interpolate(bestFitParams.a, bestFitParams.b, 255)));
            var diff = calculated - expected;
            if (bestFitParams.a < 3 || (bestFitParams.a > 4.5 && bestFitParams.a < 28))
            { }
            Debug.WriteLine($"{identifier.stationID}, {identifier.uniqueID}, {calculated.ToUnixTimeSeconds()}, {expected.ToUnixTimeSeconds()}, {now.ToUnixTimeSeconds()}, {diff.TotalSeconds}, {calculated}, " +
                $"{bestFitParams.a}");
            if (Math.Abs(diff.TotalSeconds) > maxSpan)
                return expected;
            // We cannot give it a time greater than the present.
            // We compare to expected rather than now because expected has been adjusted for the fact that there are later sub-packets in this same packet to be processed.
            else if (calculated > expected)
                return expected;
            else
                return calculated;
        }

        // From https://stackoverflow.com/questions/12946341/algorithm-for-scatter-plot-best-fit-line
        public static (double a, double b, double r2)? GenerateLinearBestFit(List<(double X, double Y)> points)
        {
            int numPoints = points.Count;
            if (numPoints == 0)
                return null;
            double meanX = points.Average(point => point.X);
            double meanY = points.Average(point => point.Y);

            double sumXSquared = points.Sum(point => point.X * point.X);
            double sumXY = points.Sum(point => point.X * point.Y);

            var denom = (sumXSquared / numPoints - meanX * meanX);
            if (denom == 0)
                return null;

            var a = (sumXY / numPoints - meanX * meanY) / (sumXSquared / numPoints - meanX * meanX);
            var b = -(a * meanX - meanY);

            double sumYSquared = points.Sum(point => (point.Y - meanY) * (point.Y - meanY));
            double sumYDiffSquareSum = points.Sum(p =>
            {
                var diff = Interpolate(a, b, p.X) - p.Y;
                return diff * diff;
            });

            var r2 = 1 - sumYDiffSquareSum / sumYSquared;
            if (r2 < 0)
            { }

            return (a, b, r2);
        }

        public static double Interpolate(double a, double b, double x)
            => a * x + b;
    }
}
