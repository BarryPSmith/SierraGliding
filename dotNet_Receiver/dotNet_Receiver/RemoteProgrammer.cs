using core_Receiver.Packets;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace core_Receiver
{
    class ProgrammerException : Exception
    {
        public ProgrammerException(string message) : base(message)
        {
        }
    }

    class RemoteProgrammer
    {
        const int MaxPacketSize = 240; //Max packet size on the modem is 255. There's some overhead, so...
        const int MaxPacketCount = 192; //This is hardcoded in the stations.
        const int MaxImageSize = 32768 - 1024; //32768: Atmega328P flash size, 1024: bootload size.

        private readonly List<byte> _image;
        private readonly ICommunicator _communicator;
        private readonly string _fn;
        public string Fn => _fn;

        public string StatusMessage { get; private set; }

        public UInt16 CRC { get; private set; }

        public int PacketSize { get; private set; }
        public int PacketCount { get; private set; }

        public bool FullBandwidth { get; set; } = true;

        public TimeSpan PacketInterval { get; set; } = TimeSpan.FromSeconds(3);

        public TimeSpan ResponseTimeout { get; set; } = TimeSpan.FromSeconds(5);

        public TextWriter OutWriter { get; set; } = Console.Out;
        
        public RemoteProgrammer(string hexSourceFile, ICommunicator commuicator)
        {
            _fn = hexSourceFile;
            _image = ReadHex(hexSourceFile);
            PacketSize = GetPacketSize(_image.Count);
            if (PacketSize < 192)
            {
                _image.AddRange(Enumerable.Repeat((byte)0xFF, 192 - _image.Count % 192));
                PacketSize = 192;
                Debug.Assert(_image.Count % 192 == 0);
            }
            if (_image.Count > MaxImageSize)
                throw new ProgrammerException($"Cannot fit image. Size: {_image.Count}. Max: {MaxImageSize}");
            PacketCount = _image.Count / PacketSize;
            if (PacketCount > MaxPacketCount)
                throw new ProgrammerException($"Cannot fit image. Required packet count: {PacketCount}");
            CRC = CalculateCRC(_image);
            _communicator = commuicator;
        }

        #region Methods
        /// <summary>
        /// Performs a complete upload for a set of stations.
        /// </summary>
        /// <param name="stationIDs">The stations to upload the new firmware to</param>
        /// <param name="demandRelay">Whether messages should be sent with demandRelay</param>
        /// <param name="timeout">The timeout for the entire operation</param>
        /// <param name="confirm">If <code>true</code>, instructs stations to apply the new firmware. Otherwise just performs the upload.</param>
        /// <returns></returns>
        /// <remarks>
        /// <para>First initialises the upload and waits for confirmation from every station.</para>
        /// <para>Then performs a bulk upload. If more than one station is supplied, sends the bulk upload to station ID zero (i.e. all listening stations).</para>
        /// <para>Then ensures every station has a complete image.</para>
        /// <para>Finally, instructs all stations to apply the image (only if <paramref name="confirm"/> is <code>true</code>).</para>
        /// </remarks>
        public async Task GoAsync(IEnumerable<byte> stationIDs, bool demandRelay, TimeSpan timeout,
            bool confirm)
        {
            OutWriter?.WriteLine($"Initiating upload to ({stationIDs.ToCsv()})");
            InitMultiple(stationIDs, demandRelay);

            OutWriter?.WriteLine("Performing bulk upload...");
            CancellationTokenSource cts = new CancellationTokenSource();
            cts.CancelAfter(timeout);
            if (stationIDs.Count() == 1)
                await BulkUploadToStation(stationIDs.Single(), demandRelay, cts.Token);
            else
                await BulkUploadToStation(0, demandRelay, cts.Token);
            OutWriter?.WriteLine("Bulk upload complete.");

            foreach (var id in stationIDs)
            {
                OutWriter?.WriteLine($"Completing for {id}...");
                await CompleteUploadToStationAsync(id, cts.Token);
                OutWriter?.WriteLine($"Image upload complete for {id}.");
            }

            if (confirm)
                foreach (var id in stationIDs)
                {
                    ConfirmProgramming(id);
                    OutWriter?.WriteLine($"Confirmed for {id}.");
                }
        }

        private void InitMultiple(IEnumerable<byte> stationIDs, bool demandRelay)
        {
            ConcurrentDictionary<byte, BasicResponse> responses = new ConcurrentDictionary<byte, BasicResponse>();
            Dictionary<byte, AutoResetEvent> acksReceived = stationIDs
                .ToDictionary(id => id, id => new AutoResetEvent(false));
            ConcurrentDictionary<byte, byte> unIds = new ConcurrentDictionary<byte, byte>();
            void packetReceived(object sender, (IList<byte> data, bool corrupt) e)
            {
                var data = e.data;
                var corrupt = e.corrupt;
                if (corrupt)
                    return;
                try
                {
                    var packet = PacketDecoder.DecodeBytes(data.ToArray(), DateTimeOffset.Now);

                    var packetSrc = packet.sendingStation;
                    if (packet.type == PacketTypes.Response &&
                        stationIDs.Any(id => id == packetSrc) &&
                        unIds.TryGetValue(packetSrc, out var statUnId) && 
                        statUnId == packet.uniqueID)
                    {
                        responses[packetSrc] = (BasicResponse)packet.packetData;
                        acksReceived[packetSrc].Set();
                    }
                }
                catch { }
            }
            _communicator.PacketReceived += packetReceived;
            try
            {
                foreach (var id in stationIDs)
                    unIds[id] = InitUploadToStation(id, demandRelay);
                OutWriter?.WriteLine("Waiting for acknowledgements...");
                if (!WaitHandle.WaitAll(acksReceived.Values.ToArray(), ResponseTimeout))
                    throw new ProgrammerException($"No response received from programming init message. " +
                        $"Failed: {acksReceived.Where(kvp => kvp.Value.WaitOne(0) == false).Select(kvp => kvp.Key).ToCsv()}");
                if (responses.Values.Any(r => r != BasicResponse.OK))
                    throw new ProgrammerException($"Non-OK response received from programming init message for stations " +
                        $"({responses.Where(kvp => kvp.Value != BasicResponse.OK).Select(kvp => kvp.Key).ToCsv()})");
            }
            finally
            {
                _communicator.PacketReceived -= packetReceived;
            }
        }


        /// <summary>
        /// Sends the initial message to a station indicating that it should prepare to be reprogrammed.
        /// </summary>
        /// <param name="destinationStationID">The station to reprogram.
        /// This can be <code>0</code>, which will instruct all stations to prepare for reprogramming.</param>
        /// <param name="demandrelay">Indicates whether every station should relay the message</param>
        /// <param name="token"></param>
        /// <returns></returns>
        public byte InitUploadToStation(byte destinationStationID, bool demandrelay)
        {
            using MemoryStream ms = new MemoryStream();
            using BinaryWriter writer = new BinaryWriter(ms, Encoding.ASCII);
            byte cmdByte = demandrelay ? (byte)('C' | 0x80) : (byte)'C';
            var unID = _communicator.GetNextUniqueID();
            writer.Write(cmdByte);
            writer.Write(destinationStationID);
            writer.Write(unID);
            writer.Write((byte)'P'); //Programming
            writer.Write((byte)'B'); //Begin
            writer.Write((byte)PacketCount);
            writer.Write((byte)PacketSize);
            writer.Write((UInt16)CRC);
            writer.Flush();
            var data = AppendCommandCrc(destinationStationID, ms);
            _communicator.Write(data);
            return unID;
        }
        
        /// <summary>
        /// Uploads all image to a station. Doesn't care whether or not the station hears it.
        /// </summary>
        /// <param name="destinationStationID">The station to upload data to.
        /// This can be zero, which will upload to all stations currently listening.</param>
        /// <param name="demandrelay">Indicates whether every station should relay the messages</param>
        /// <param name="token"></param>
        /// <returns></returns>
        public Task BulkUploadToStation(byte destinationStationID, bool demandrelay, CancellationToken token)
        {
            return Task.Run(() =>
            {
                byte cmdByte = demandrelay ? (byte)('C' | 0x80) : (byte)'C';
                for (int i = 0; i < PacketCount; i++)
                {
                    token.ThrowIfCancellationRequested();
                    SendImagePacket(destinationStationID, cmdByte, i);
                    Thread.Sleep(PacketInterval);
                }
            }, token);
        }

        private byte SendImagePacket(byte destinationStationID, byte cmdByte, int i)
        {
            var ms = new MemoryStream();
            var writer = new BinaryWriter(ms, Encoding.ASCII);
            var uniqueID = _communicator.GetNextUniqueID();
            writer.Write(cmdByte);
            writer.Write(destinationStationID);
            writer.Write(uniqueID);
            writer.Write('P');
            writer.Write('I');
            writer.Write((byte)i);
            writer.Write((UInt16)CRC);
            writer.Write(_image.Skip(i * PacketSize).Take(PacketSize).ToArray());
            writer.Flush();
            var data = AppendCommandCrc(destinationStationID, ms);
            PacketDecoder.RecentCommands[uniqueID] = "P";
            _communicator.Write(data);
            return uniqueID;
        }

        private static byte[] AppendCommandCrc(byte destinationStationID, MemoryStream ms)
        {
            var data = ms.ToArray().ToList();
            HashSet<char> nonCrcStations = new HashSet<char>() 
                {  };
            if (!nonCrcStations.Contains(destinationStationID.ToChar()))
                CommandInterpreter.AddCrc(data);
            return data.ToArray();
        }

        /// <summary>
        /// Finds out which packets a station still needs and sends them until it's got everything.
        /// </summary>
        /// <param name="destinationStationID">The station to upload to.
        /// This cannot be zero - it must be a specific station</param>
        /// <param name="token"></param>
        /// <returns></returns>
        public Task CompleteUploadToStationAsync(byte destinationStationID, CancellationToken token)
        {
            if (destinationStationID == 0)
                throw new ArgumentOutOfRangeException(nameof(destinationStationID), "destinationStationID cannot be zero for complete upload.");
            return Task.Run(() =>
            {
                AutoResetEvent replyReceived = new AutoResetEvent(false);
                byte lastUniqueID = 0;
                ProgrammingResponse lastResponse = null;

                void packetReceived(object sender, (IList<byte> data, bool corrupt) e)
                {
                    var data = e.data;
                    var corrupt = e.corrupt;
                    if (corrupt)
                        return;
                    try
                    {
                        var packet = PacketDecoder.DecodeBytes(data.ToArray(), DateTimeOffset.Now);
                        if (packet?.type == PacketTypes.Response && packet.uniqueID == lastUniqueID && packet.sendingStation == destinationStationID)
                        {
                            lastResponse = packet.packetData as ProgrammingResponse ?? lastResponse;
                            replyReceived.Set();
                        }
                    }
                    catch
                    { }
                }

                _communicator.PacketReceived += packetReceived;
                try
                {
                    int cycle = 1;
                    int totalPackets = 0;
                    while (true)
                    {
                        StatusMessage = $"Querying station status. Cycle {cycle}";
                        token.ThrowIfCancellationRequested();
                        // Get the station status:
                        OutWriter.WriteLine($"Programmer: Query status of {destinationStationID}");
                        replyReceived.Reset();
                        lastUniqueID = QueryStationProgramming(destinationStationID);
                        if (WaitHandle.WaitAny(new WaitHandle[] { replyReceived, token.WaitHandle }, ResponseTimeout) == WaitHandle.WaitTimeout ||
                            lastResponse == null)
                            continue;
                        // Check that the station is expecting us:
                        if (lastResponse.expectedCRC != CRC)
                            throw new ProgrammerException($"Station/Programmer CRC mismatch. Station: {lastResponse.expectedCRC:X}, Programmer: {CRC:X}");
                        if (lastResponse.totalExpectedPackets != PacketCount)
                            throw new ProgrammerException($"Station/Programmer packet count mismatch. Station: {lastResponse.totalExpectedPackets}, Programmer: {PacketCount}");
                        // Check if it's all done
                        if (lastResponse.allThere)
                        {
                            if (!lastResponse.crcMatch)
                                throw new ProgrammerException($"Station total CRC mistmatch."); //Nothing we can do at this point. Gotta fail.
                            else
                            {
                                StatusMessage = $"Upload complete. Required {totalPackets} packets ({(double)PacketCount / totalPackets:P1} efficiency). {cycle} cycles required.";
                                return; //All there and CRC matches.
                            }
                        }
                        if (lastResponse.receivedPackets.All(a => a))
                            throw new ProgrammerException("Packet doesn't report all there, but reports all packets received.");
                        // Send what's not done:
                        int packetsToSend = lastResponse.receivedPackets.Take(PacketCount)
                            .Count(b => !b);
                        int packetsSent = 0;
                        for (int i = 0; i < PacketCount; i++)
                        {
                            token.ThrowIfCancellationRequested();
                            if (lastResponse.receivedPackets[i])
                                continue;
                            StatusMessage = $"Uploading packet {++packetsSent}/{packetsToSend} ({(float)packetsSent / packetsToSend:P0}). Cycle {cycle}";
                            lastUniqueID = SendImagePacket(destinationStationID, (byte)'C', i);
                            totalPackets++;

                            if (FullBandwidth)
                            {
                                replyReceived.Reset();
                                Stopwatch sw = new Stopwatch();
                                sw.Start();
                                Console.WriteLine($"Waiting for {lastUniqueID:X2}");
                                replyReceived.WaitOne(PacketInterval);
                                sw.Stop();
                                Console.WriteLine($"Response time: {sw.ElapsedMilliseconds} ms");
                            }
                            else
                                Thread.Sleep(PacketInterval);
                        }
                        cycle++;
                    }
                }
                finally
                {
                    _communicator.PacketReceived -= packetReceived;
                }
            }, token);
        }

        public byte QueryStationProgramming(byte destinationStationID)
        {
            var ms = new MemoryStream();
            var writer = new BinaryWriter(ms, Encoding.ASCII);
            byte uniqueID = _communicator.GetNextUniqueID();
            writer.Write('C');
            writer.Write(destinationStationID);
            writer.Write(uniqueID);
            writer.Write('P');
            writer.Write('Q');
            writer.Flush();
            var data = AppendCommandCrc(destinationStationID, ms);
            PacketDecoder.RecentCommands[uniqueID] = "PQ";
            _communicator.Write(data);
            return uniqueID;
        }

        public byte QueryStationFlash(byte destinationStationID, ushort address, byte payloadSize)
        {
            var ms = new MemoryStream();
            var writer = new BinaryWriter(ms, Encoding.ASCII);
            byte uniqueID = _communicator.GetNextUniqueID();
            writer.Write('C');
            writer.Write(destinationStationID);
            writer.Write(uniqueID);
            writer.Write('P');
            writer.Write('F');
            writer.Write('R');
            writer.Write(address);
            writer.Write(payloadSize);
            writer.Flush();
            var data = AppendCommandCrc(destinationStationID, ms);
            PacketDecoder.RecentCommands[uniqueID] = "PFR";
            _communicator.Write(data);
            return uniqueID;
        }

        /// <summary>
        /// Instructs a station to install its new software.
        /// </summary>
        /// <param name="destinationStationID">The station to instruct</param>
        public void ConfirmProgramming(byte destinationStationID)
        {
            var ms = new MemoryStream();
            var writer = new BinaryWriter(ms, Encoding.ASCII);
            writer.Write('C');
            writer.Write(destinationStationID);
            writer.Write(_communicator.GetNextUniqueID());
            writer.Write('P');
            writer.Write('C');
            writer.Write(CRC);
            writer.Flush();
            var data = AppendCommandCrc(destinationStationID, ms);
            _communicator.Write(data);
        }

        private static List<byte> ReadHex(string hexSourceFile)
        {
            var lines = File.ReadAllLines(hexSourceFile);
            int curAddress = 0;
            int lineIdx = 0;
            List<byte> ret = new List<byte>();
            bool ended = false;
            foreach (var line in lines)
            {
                lineIdx++;
                if (ended)
                    throw new InvalidDataException($"Line {lineIdx} occurs after EOF marker.");
                if (line[0] != ':')
                    throw new InvalidDataException($"Line {lineIdx}: Each line in a hex file must start with a colon");
                if (!int.TryParse(line.AsSpan(1, 2), NumberStyles.HexNumber, null, out int byteCount))
                    throw new InvalidDataException($"Line {lineIdx}: Cannot read byte count.");
                if (!int.TryParse(line.AsSpan(3, 4), NumberStyles.HexNumber, null, out int address))
                    throw new InvalidDataException($"Line {lineIdx}: Cannot read address");
                if (!int.TryParse(line.AsSpan(7, 2), NumberStyles.HexNumber, null, out int recordType))
                    throw new InvalidDataException($"Line {lineIdx}: Cannot read recordType");
                string newDataStr = byteCount > 0 ? line.Substring(9, byteCount * 2) : null;
                if (!int.TryParse(line.AsSpan(9 + byteCount * 2, 2), NumberStyles.HexNumber, null, out int checksum))
                    throw new InvalidDataException($"Line {lineIdx}: Cannot read checksum.");

                byte[] newData = new byte[byteCount];
                for (int i = 0; i < byteCount; i++)
                    newData[i] = byte.Parse(newDataStr.Substring(i * 2, 2), NumberStyles.HexNumber);

                byte calculatedChecksum = 0;
                for (int i = 1; i <= 7 + byteCount * 2; i += 2)
                    calculatedChecksum += byte.Parse(line.AsSpan(i, 2), NumberStyles.HexNumber);
                calculatedChecksum = (byte)(~calculatedChecksum + 1);
                //byte calculatedChecksum = (byte)~(newData.Sum(b => (int)b) & 0xFF);
                if (calculatedChecksum != checksum)
                    throw new InvalidDataException($"Line {lineIdx}: Checksum Mismatch. Calculated: {calculatedChecksum:X}, expected: {checksum:X}");

                switch (recordType)
                {
                    case 0: // Data
                        if (address != curAddress)
                            throw new InvalidDataException($"Line {lineIdx}: Unexpected address {address:X}. Expect: {curAddress:X}");
                        ret.AddRange(newData);
                        curAddress += byteCount;
                        break;
                    case 1: //EOF
                        if (address != 0)
                            throw new InvalidDataException($"Line {lineIdx}: EOF With non-zero address.");
                        if (byteCount != 0)
                            throw new InvalidDataException($"Lien {lineIdx}: EOF with non-zero byte count.");
                        ended = true;
                        break;
                    default:
                        throw new NotImplementedException($"Line {lineIdx}: Record Type {recordType:X} not implemented");
                }
            }
            if (!ended)
                throw new InvalidDataException("No EOF marker in file.");
            return ret;
        }

        private static int GetPacketSize(int imageSize)
        {
            for (int i = MaxPacketSize; i > 0; i--)
                if (imageSize % i == 0)
                    return i;
            //Should never get here. Packet size of 1 should suffice.
            throw new Exception("Unexpected code flow.");
        }

        public static UInt16 CalculateCRC(IList<byte> data, UInt16 seed = 0xFFFF)
        {
            UInt16 crc = seed;
            foreach (var singleByte in data)
                crc = crc_ccitt_update(crc, singleByte);
            return crc;
        }

        /// <summary>
        /// Updates the CCITT CRC. This is what the stations use.
        /// It might not be correct but it's the same as AVR libc implements, which is more important.
        /// https://www.microchip.com/webdoc/avrlibcreferencemanual/group__util__crc_1ga1c1d3ad875310cbc58000e24d981ad20.html
        /// </summary>
        /// <param name="crc">The current CRC</param>
        /// <param name="data">The next byte in the data</param>
        /// <returns>The updated CRC</returns>
        /// <remarks>
        /// Turns out that the AVR libc documentation is incorrect. So we just transcoded the assembly from the header file.
        /// </remarks>
        public static UInt16 crc_ccitt_update(UInt16 crc, byte data)
        {
            return crc_ccitt_update2(crc, data);

            /*data ^= (byte)(crc & 0xFF);
            data ^= (byte)(data << 4);

            return (UInt16)((((UInt16)data << 8) | (crc & 0xFF00)) ^ (byte)(data >> 4)
                    ^ ((UInt16)data << 3));*/
        }

        public static UInt16 crc_ccitt_update2(UInt16 crc, byte data)
        {
            List<bool> facts = new List<bool>();
            //You know you've fucked up when you're writing assembly in C#.
            byte crcLo = (byte)(crc & 0xFF);
            byte crcHi = (byte)(crc >> 8);

            byte loin = crcLo;
            byte hiin = crcHi;

            byte tmpReg;

            crcLo ^= data;
            var h = tmpReg = crcLo;
            swapNibbles(ref crcLo);
            var g = crcLo &= 0xF0;
            var c = crcLo ^= tmpReg;

            tmpReg = crcHi;

            crcHi = crcLo;

            swapNibbles(ref crcLo);
            var e = crcLo &= 0x0F;
            var b = tmpReg ^= crcLo;

            var d = crcLo >>= 1;

            // hi = hi^lo
            // lo = former hi
            crcHi ^= crcLo;
            crcLo ^= crcHi;

            var a = crcLo <<= 3;
            crcLo ^= tmpReg;

            facts.Add(crcHi == (d ^ c));
            facts.Add(crcLo == (a ^ b));
            facts.Add(a == ((c << 3) & 0xFF));
            facts.Add(c == (g ^ h));
            facts.Add(g == (getSwapNibbles(h) & 0xF0));
            facts.Add(b == ((byte)(crc >> 8) ^ e));
            facts.Add(e == (getSwapNibbles(c) & 0x0F));
            facts.Add(d == e >> 1);

            facts.Add(bit(crcLo, 0) == (bit(hiin, 0) ^ bit(h, 0) ^ bit(h, 4)));
            facts.Add(bit(crcLo, 1) == (bit(hiin, 1) ^ bit(h, 1) ^ bit(h, 5)));
            facts.Add(bit(crcLo, 2) == (bit(hiin, 2) ^ bit(h, 2) ^ bit(h, 6)));
            facts.Add(bit(crcLo, 3) == (bit(hiin, 3) ^ bit(h, 3) ^ bit(h, 7)) ^ bit(h, 0));
            facts.Add(bit(crcLo, 4) == (bit(hiin, 4) ^ bit(h, 1)));
            facts.Add(bit(crcLo, 5) == (bit(hiin, 5) ^ bit(h, 2)));
            facts.Add(bit(crcLo, 6) == (bit(hiin, 6) ^ bit(h, 3)));
            facts.Add(bit(crcLo, 7) == (bit(hiin, 7) ^ bit(h, 4) ^ bit(h, 0)));
            facts.Add(bit(crcHi, 0) == (bit(h, 0) ^ bit(h, 1) ^ bit(h, 5)));
            facts.Add(bit(crcHi, 1) == (bit(h, 1) ^ bit(h, 2) ^ bit(h, 6)));
            facts.Add(bit(crcHi, 2) == (bit(h, 2) ^ bit(h, 3) ^ bit(h, 7)));
            facts.Add(bit(crcHi, 3) == (bit(h, 3)));
            facts.Add(bit(crcHi, 4) == (bit(h, 4) ^ bit(h, 0)));
            facts.Add(bit(crcHi, 5) == (bit(h, 5) ^ bit(h, 1)));
            facts.Add(bit(crcHi, 6) == (bit(h, 6) ^ bit(h, 2)));
            facts.Add(bit(crcHi, 7) == (bit(h, 7) ^ bit(h, 3)));
            bool allTrue = facts.All(f => f);

            return (ushort)((crcHi << 8) | crcLo);
        }
        static byte swapNibbles(ref byte b) => b = (byte)((b << 4) | (b >> 4));
        static byte getSwapNibbles(byte b) => b = (byte)((b << 4) | (b >> 4));
        static bool bit(byte b, int bit) => ((b >> bit) & 0x01) == 1;
        #endregion Methods

        public override string ToString()
        {
            return $"Packet Size: {PacketSize}, Packet Count: {PacketCount}, CRC: {CRC:X}, Size: {_image.Count}, " +
                $"Interval: {PacketInterval.TotalSeconds}, Timeout: {ResponseTimeout.TotalSeconds}, Source: {_fn}, Full Bandwidth: {FullBandwidth}";
        }

        public string DataAsSingleLineHex => _image.ToCsv(i => i.ToString("X2"), Environment.NewLine);

        public Task<List<byte>> ReadRemoteImageAsync(byte stationID, CancellationToken token)
        {
            int imageOffset = 10;
            const int packetSize = 192;
            StringBuilder s = new StringBuilder();
            byte lastUniqueID = 0;
            byte[] lastData = null;
            var replyReceived = new AutoResetEvent(false);
            return Task.Run(() =>
            {
                void packetReceived(object sender, (IList<byte> data, bool corrupt) e)
                {
                    var bytes = e.data;
                    var corrupt = e.corrupt;
                    if (corrupt)
                        return;
                    var packet = PacketDecoder.DecodeBytes(bytes.ToArray(), DateTimeOffset.Now);
                    if (packet?.type == PacketTypes.Response && packet.uniqueID == lastUniqueID && packet.sendingStation == stationID)
                    {
                        lastData = (byte[])packet.packetData;
                        replyReceived.Set();
                    }
                }
                _communicator.PacketReceived += packetReceived;
                try
                {
                    var ret = new List<byte>(_image.Count);
                    for (int i = 0; i < _image.Count; i += packetSize)
                    {
                        int payloadSize = Math.Min(_image.Count - i, packetSize);
                        do
                        {
                            token.ThrowIfCancellationRequested();
                            lastUniqueID = QueryStationFlash(stationID, (ushort)(i + imageOffset), (byte)payloadSize);
                        }
                        while (WaitHandle.WaitAny(new WaitHandle[] { replyReceived, token.WaitHandle }, ResponseTimeout) == WaitHandle.WaitTimeout ||
                            lastData == null);
                        Debug.Assert(lastData.Length == payloadSize);
                        ret.AddRange(lastData);
                    }
                    Debug.Assert(ret.Count == _image.Count);
                    return ret;
                }
                finally
                {
                    _communicator.PacketReceived -= packetReceived;
                }
            }, token);
        }
    }
}
