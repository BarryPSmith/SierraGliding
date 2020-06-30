using System;
using System.Collections.Generic;
using System.Text;

namespace core_Receiver.Packets
{
    class SingleWeatherData
    {
        public byte sendingStation;
        public byte uniqueID;
        public double windDirection;
        public double windSpeed;
        public double? batteryLevelH;
        public double? internalTemp;
        public double? externalTemp;
        public double? gust;
        public byte[] extras;

        public override string ToString()
        {
            var ret = $"ID:{sendingStation} WD:{windDirection:F1} WS:{windSpeed:F2}";
            if (gust.HasValue)
                ret += $" G: {gust}";
            if (batteryLevelH.HasValue)
                ret += $" B:{batteryLevelH:F2}";
            if (externalTemp.HasValue)
                ret += $" ET:{externalTemp:F1}";
            if (internalTemp.HasValue)
                ret += $" IT:{internalTemp:F1}";
            if (extras?.Length > 0)
                ret += $" ({extras.ToCsv(b => b.ToString("X2"), " ")})";
            return ret;
        }
    }
}
