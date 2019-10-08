using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace dotNet_Receiver
{
    class KissCommunication
    {
        const byte FEND = 0xC0;
        const byte FESC = 0xDB;
        const byte TFEND = 0xDC;
        const byte TFESC = 0xDD;

        string _address;
        int _port;
        volatile bool _disconnectRequested = false;

        public Action<IList<Byte>> PacketReceived { get; set; }
        public Action<string> StreamError { get; set; }
        public int MaxPacketSize { get; set; } = 30000; //Arbitrary maximum packet size to avoid consuming too much memory
        public bool IsConnected { get; private set; } = false;

        public static IPEndPoint CreateIPEndPoint(string endPoint)
        {
            string[] ep = endPoint.Split(':');
            if (ep.Length != 2) throw new FormatException("Invalid endpoint format");
            IPAddress ip;
            if (!IPAddress.TryParse(ep[0], out ip))
            {
                throw new FormatException("Invalid ip-adress");
            }
            int port;
            if (!int.TryParse(ep[1], NumberStyles.None, NumberFormatInfo.CurrentInfo, out port))
            {
                throw new FormatException("Invalid port");
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
            _address = address;
            _port = port;
            var ep = new IPEndPoint(IPAddress.Parse(address), port);
            ConnectInternal(ep);
        }

        private void ConnectInternal(IPEndPoint ep)
        {
            using (TcpClient client = new TcpClient(ep.Address.ToString(), ep.Port))
            {
                //client.Connect(ep);
                using (var netStream = client.GetStream())
                {
                    ReadStream(netStream);
                    IsConnected = false;
                }
            }
        }

        public void ReadStream(Stream netStream)
        {
            bool escaped = false;
            bool inPacket = false;
            List<byte> curPacket = new List<byte>();
            for (var curByte = netStream.ReadByte(); curByte != -1; curByte = netStream.ReadByte())
            {
                if (_disconnectRequested)
                    break;

                if (curByte == FEND)
                {
                    if (escaped)
                    {
                        escaped = false;
                        StreamError?.Invoke("Unexpected escaped FEND");
                        continue;
                    }
                    else if (curPacket.Count > 0)
                    {
                        PacketReceived?.Invoke(curPacket);
                        curPacket.Clear();
                    }
                    inPacket = true;
                    continue;
                }
                if (!inPacket)
                    continue;

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

        public void Disconnect()
        {
            _disconnectRequested = true;
        }
    }
}
