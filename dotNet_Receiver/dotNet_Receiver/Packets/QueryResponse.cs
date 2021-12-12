using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace core_Receiver.Packets
{
    class QueryResponse
    {
        public string Version { get; set; }

        protected int ArraySize => VersionNumber >= new Version(2, 3) ? 15 : 20;

        public QueryResponse(Span<byte> inputData, out int consumed)
        {
            int i = 0;
            while (true)
            {
                if (i >= inputData.Length)
                    throw new InvalidDataException("Terminating null not found on version string.");
                if (inputData[i] == 0)
                    break;
                i++;
            }
            Version = inputData.Slice(0, i).ToArray().ToAscii();
            var secondDot = Version.IndexOf('.', Version.IndexOf('.') + 1);
            if (secondDot > 0)
                VersionNumber = new Version(Version.Substring(1, secondDot - 1));
            consumed = i + 1;
        }

        public Version VersionNumber { get; set; }
    }
}
