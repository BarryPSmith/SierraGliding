using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.IO.Ports;
using System.Diagnostics;
using System.Collections.Concurrent;
using System.Threading;

namespace core_Receiver
{
    class KissCommunication : ICommunicator
    {
        const byte FEND = 0xC0;
        const byte FESC = 0xDB;
        const byte TFEND = 0xDC;
        const byte TFESC = 0xDD;

        volatile bool _disconnectRequested = false;

        private int _cmdUniqueID = new Random().Next();
        public byte GetNextUniqueID()
        {
            return (byte)(Interlocked.Increment(ref _cmdUniqueID) & 0xFF);
        }

        public event EventHandler<IList<Byte>> PacketReceived;
        public Action<string> StreamError { get; set; }
        public int MaxPacketSize { get; set; } = 30000; //Arbitrary maximum packet size to avoid consuming too much memory

        public Stream NonPacketStream { get; set; }

        //public Stream SerialStream { get; private set; }
        ConcurrentDictionary<Stream, ConcurrentQueue<(byte[] data, byte writeType)>> _writeQueue =
            new ConcurrentDictionary<Stream, ConcurrentQueue<(byte[] data, byte writeType)>>();

        public TextWriter OutputWriter => Program.OutputWriter;

        public static IPEndPoint CreateIPEndPoint(string endPoint)
        {
            string[] ep = endPoint.Split(':');
            if (ep.Length != 2) throw new FormatException($"Invalid endpoint format {endPoint}");
            IPAddress ip;
            if (!IPAddress.TryParse(ep[0], out ip))
            {
                throw new FormatException($"Invalid ip-adress {ep[0]}");
            }
            int port;
            if (!int.TryParse(ep[1], NumberStyles.None, NumberFormatInfo.CurrentInfo, out port))
            {
                throw new FormatException($"Invalid port {ep[1]}");
            }
            return new IPEndPoint(ip, port);
        }

        public void Connect(string fullAddress)
        {
            var ep = CreateIPEndPoint(fullAddress);
            ConnectInternal(ep);
        }

        public void Connect(string address, int port)
        {
            var ep = new IPEndPoint(IPAddress.Parse(address), port);
            ConnectInternal(ep);
        }

        private void ConnectInternal(IPEndPoint ep)
        {
            using TcpClient client = new TcpClient(ep.Address.ToString(), ep.Port);
            //client.Connect(ep);
            using var netStream = client.GetStream();
            netStream.ReadTimeout = 100;
            Connect(netStream);
        }

        public void ConnectSerial(string portName)
        {
            if (string.IsNullOrEmpty(portName))
            {
                OutputWriter.WriteLine("No Serial port specified, using first available...");
                var allPorts = SerialPort.GetPortNames();
                foreach (var testPortName in allPorts)
                {
                    using var port = new SerialPort(testPortName);
                    try
                    {
                        port.Open();
                        portName = testPortName;
                        break;
                    }
                    catch { }
                }
            }
            if (string.IsNullOrEmpty(portName))
            {
                Console.Error.WriteLine("No Serial port found.");
                return;
            }

            OutputWriter.WriteLine($"Using serial port {portName}");
            using (var port = new SerialPort(portName))
            {
                port.BaudRate = 38400;
                port.Parity = Parity.None;
                port.DataBits = 8;
                port.StopBits = StopBits.One;
                port.Handshake = Handshake.None;
                port.Open();
                port.BaseStream.ReadTimeout = 100;
                Connect(port.BaseStream);
                port.Close();
            }
        }

        int ReadOrWriteStream(Stream stream)
        {
            while (true)
            {
                try
                {
                    return stream.ReadByte();
                }
                catch (Exception ex) when (ex is TimeoutException || ex is IOException)
                {
                    if (_disconnectRequested)
                        return -1;
                    while (_writeQueue[stream].TryDequeue(out var toWrite))
                    {
                        WriteStreamInternal(stream, toWrite.data, toWrite.writeType);
                    }
                }
            }
        }

        public void Connect(Stream stream)
        {
            bool escaped = false;
            bool inPacket = false;
            bool awaitingNull = false;
            List<byte> curPacket = new List<byte>();
            if (!_writeQueue.TryAdd(stream, new ConcurrentQueue<(byte[] data, byte writeType)>()))
                throw new InvalidOperationException($"Already connected to stream '{stream}.");

            DateTimeOffset lastNpsTimestamp = DateTimeOffset.MinValue;
            bool npsNeedsTimestamp = true;

            for (var curByte = ReadOrWriteStream(stream); curByte != -1; curByte = ReadOrWriteStream(stream))
            {
                if (_disconnectRequested)
                    break;

                if (curByte == FEND)
                {
                    if (inPacket)
                    {
                        if (escaped)
                        {
                            escaped = false;
                            StreamError?.Invoke("Unexpected escaped FEND");
                            continue;
                        }
                        else if (inPacket && curPacket.Count > 0)
                        {
                            PacketReceived?.Invoke(this, curPacket);
                            curPacket.Clear();
                            inPacket = false;
                        }
                    }
                    awaitingNull = true;
                    continue;
                }
                if (awaitingNull)
                {
                    awaitingNull = false;
                    if (curByte == 0x00)
                    {
                        inPacket = true;
                        continue;
                    }
                }

                if (!inPacket)
                {
                    if (npsNeedsTimestamp && (DateTimeOffset.Now - lastNpsTimestamp) > TimeSpan.FromSeconds(1))
                    {
                        var timestamp = Encoding.UTF8.GetBytes($"{DateTime.Now}: ");
                        NonPacketStream?.Write(timestamp, 0, timestamp.Length);
                    }
                    NonPacketStream?.WriteByte((byte)curByte);
                    npsNeedsTimestamp = curByte == '\n';
                    continue;
                }

                if (curByte == FESC)
                {
                    if (escaped)
                        StreamError?.Invoke("Unexpected escaped FESC");
                    escaped = !escaped;
                    continue;
                }
                if (escaped)
                {
                    escaped = false;
                    if (curByte == TFEND)
                        curByte = FEND;
                    else if (curByte == TFESC)
                        curByte = FESC;
                    else
                    {
                        StreamError?.Invoke($"Unexpected escaped 0x{curByte:X2}");
                        continue;
                    }
                }
                if (curPacket.Count >= MaxPacketSize)
                {
                    StreamError?.Invoke($"Packet size exceeds {MaxPacketSize}. Discarding packet and all bytes until next FEND.");
                    curPacket.Clear();
                    inPacket = false;
                    escaped = false;
                }
                curPacket.Add((byte)curByte);
            }
            if (!_writeQueue.TryRemove(stream, out var notUsed))
                Console.Error.WriteLine("Unable to remove stream from write queue");
        }

        public void Write(byte[] data, byte writeType = 0x00)
        {
            if (!_writeQueue.Any())
                throw new InvalidOperationException("Serial Stream is not present.");

            foreach (var kvp in _writeQueue)
                kvp.Value.Enqueue((data, writeType));
        }

        private void WriteStreamInternal(Stream stream, byte[] data, byte writeType)
        {
            lock (stream)
            {
                using BinaryWriter writer = new BinaryWriter(stream, Encoding.ASCII, true);
                writer.Write(FEND);
                writer.Write(writeType);
                foreach (byte cur in data)
                {
                    var curByte = (byte)cur;
                    if (curByte == FESC)
                    {
                        writer.Write(FESC);
                        writer.Write(TFESC);
                    }
                    else if (curByte == FEND)
                    {
                        writer.Write(FESC);
                        writer.Write(TFEND);
                    }
                    else
                        writer.Write(curByte);
                }
                writer.Write(FEND);
            }
        }
        public void Disconnect()
        {
            _disconnectRequested = true;
        }
    }
}
