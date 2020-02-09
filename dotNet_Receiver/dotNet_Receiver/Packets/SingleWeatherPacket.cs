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

        public override string ToString()
        {
            var ret = $"WD:{windDirection:F1} WS:{windSpeed:F2}";
            if (batteryLevelH.HasValue)
                ret += $" B:{batteryLevelH:F2}";
            if (externalTemp.HasValue)
                ret += $" ET:{externalTemp:F1}";
            if (internalTemp.HasValue)
                ret += $" IT:{internalTemp:F1}";
            return ret;
        }
    }
}
