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
        public byte? pwmValue;
        public byte? current;
        public byte[] extras;
        public DateTimeOffset? calculatedTime;
        public byte? timestampByte;
        public DateTimeOffset? timeStamp;
        public ushort? lastErrorTSShort;
        public DateTimeOffset? lastErrorTimestamp;
        public short? lastErrorCode;

        public override string ToString()
        {
            var ret = $"ID:{sendingStation}/{uniqueID:X2} WD:{windDirection:F1} WS:{windSpeed:F1}";
            if (gust.HasValue)
                ret += $" G: {gust}";
            if (batteryLevelH.HasValue)
                ret += $" B:{batteryLevelH:F2}";
            if (externalTemp.HasValue)
                ret += $" ET:{externalTemp:F1}";
            if (internalTemp.HasValue)
                ret += $" IT:{internalTemp:F1}";
            if (pwmValue.HasValue)
                ret += $" PWM:{pwmValue:X}";
            if (current.HasValue)
                ret += $" Cur:{current}";
            if (timeStamp.HasValue)
                ret += $" Delay:{(DateTimeOffset.Now - timeStamp).Value.TotalSeconds:F0}s";
            if (lastErrorCode.HasValue)
                ret += $" ERR:{lastErrorCode}";
            if (lastErrorTimestamp.HasValue)
            {
                var spacing = (lastErrorTimestamp - timeStamp).Value.TotalSeconds;
                ret += $" ({spacing}s)";
            }
            if (extras?.Length > 0)
                ret += $" ({extras.ToCsv(b => b.ToString("X2"), " ")})";
            return ret;
        }
    }
}
