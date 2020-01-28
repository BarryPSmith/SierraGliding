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

namespace dotNet_Receiver
{
    class KissCommunication
    {
        const byte FEND = 0xC0;
        const byte FESC = 0xDB;
        const byte TFEND = 0xDC;
        const byte TFESC = 0xDD;

        volatile bool _disconnectRequested = false;

        public Action<IList<Byte>> PacketReceived { get; set; }
        public Action<string> StreamError { get; set; }
        public int MaxPacketSize { get; set; } = 30000; //Arbitrary maximum packet size to avoid consuming too much memory
        public bool IsConnected { get; private set; } = false;

        public Stream NonPacketStream { get; set; }

        public Stream SerialStream { get; private set; }

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
            ReadStream(netStream);
            IsConnected = false;
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
                SerialStream = port.BaseStream;
                ReadStream(port.BaseStream);
                SerialStream = null;
                port.Close();
            }
        }

        int ReadOrWriteStream(Stream stream)
        {
            while (true)
            {
                try
                {
                    //Console.WriteLine("Reading a byte...");
                    return stream.ReadByte();
                }
                catch (TimeoutException)
                {
                    if (_disconnectRequested)
                        return -1;
                    if (_writeQueue.TryDequeue(out var toWrite))
                    {
                        WriteSerialInternal(toWrite.stream, toWrite.writeType);
                    }
                }
            }
        }

        public void ReadStream(Stream netStream)
        {
            bool escaped = false;
            bool inPacket = false;
            bool awaitingNull = false;
            List<byte> curPacket = new List<byte>();
            for (var curByte = ReadOrWriteStream(netStream); curByte != -1; curByte = ReadOrWriteStream(netStream))
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
                            PacketReceived?.Invoke(curPacket);
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
                    NonPacketStream?.WriteByte((byte)curByte);
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
        }

        ConcurrentQueue<(Stream stream, byte writeType)> _writeQueue = new ConcurrentQueue<(Stream stream, byte writeType)>();

        public void WriteSerial(Stream dataStream, byte writeType = 0x00)
        {
            if (SerialStream == null)
                throw new InvalidOperationException("Serial Stream is not present.");

            _writeQueue.Enqueue((dataStream, writeType));
        }

        private void WriteSerialInternal(Stream dataStream, byte writeType)
        {
            lock (SerialStream)
            {
                using BinaryWriter writer = new BinaryWriter(SerialStream, Encoding.ASCII, true);
                writer.Write(FEND);
                writer.Write(writeType);
                for (var cur = dataStream.ReadByte(); cur != -1; cur = dataStream.ReadByte())
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
