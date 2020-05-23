using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace core_Receiver
{
    class SocketListener : ICommunicator
    {
        UdpClient _client;
        UdpClient _client6;
        int _sendPort;
        int _sendPort6;
        volatile bool _connected = true;
        ConcurrentQueue<Guid> _receivedGuids = new ConcurrentQueue<Guid>();

        public TextWriter OutputWriter => throw new NotImplementedException();

        public Action<string> StreamError { get; set; }

        public SocketListener(int listenPort, int sendPort, int port6, bool port6In)
        {
            _client = new UdpClient();
            _client.Client.Bind(new IPEndPoint(IPAddress.Any, listenPort));
            if (port6In)
            {
                Console.WriteLine($"Binding client6 to listen on {port6}");
                _client6 = new UdpClient();
                _client6.Client.Bind(new IPEndPoint(IPAddress.Any, port6));
            }
            _sendPort = sendPort;
            _sendPort6 = port6;
        }

        public List<Task> StartAsync(CancellationToken token = default(CancellationToken))
        {
            if (token.CanBeCanceled)
                token.Register(() => _client.Close());
            List<Task> tasks = new List<Task>();
            Console.WriteLine($"Listening for UDP messages...");
            tasks.Add(StartInternalAsync(_client, false));
            if (_client6 != null)
            {
                Console.WriteLine($"Listening for HW messages on {_sendPort6}");
                tasks.Add(StartInternalAsync(_client6, true));
            }
            return tasks;
        }

        private async Task StartInternalAsync(UdpClient client, bool is6)
        {
            while (_connected)
            {
                try
                {
                    var result = await client.ReceiveAsync();
                    
                    if (result.Buffer.Length < 16)
                    {
                        Console.Error.WriteLine("Received packet with length too short for GUID");
                        continue;
                    }
                    var guid = new Guid(result.Buffer.AsSpan(0, 16));
                    if (_receivedGuids.Contains(guid))
                        continue;
                    _receivedGuids.Enqueue(guid);
                    while (_receivedGuids.Count > 100)
                        _receivedGuids.TryDequeue(out var notUsed);

                    if (is6)
                        PacketReceived6?.Invoke(this, result.Buffer.AsSpan(16).ToArray());
                    else
                        PacketReceived?.Invoke(this, result.Buffer.AsSpan(16).ToArray());
                }
                catch (ObjectDisposedException) { }
                catch (Exception ex)
                {
                    Console.Error.WriteLine(ex);
                }
            }
        }

        private int _cmdUniqueID = new Random().Next();
        public byte GetNextUniqueID()
        {
            return (byte)(Interlocked.Increment(ref _cmdUniqueID) & 0xFF);
        }

        public void Write(byte[] data, byte writeType = 0)
        {
            int port;
            switch (writeType)
            {
                case 0:
                    port = _sendPort;
                    break;
                case 6:
                    port = _sendPort6;
                    break;
                default:
                    throw new InvalidOperationException("Can only write type 0 and 6");
            }
            if (!_connected)
                throw new ObjectDisposedException("SocketListener is already closed!");
            var dataToSend = Guid.NewGuid().ToByteArray().Concat(data).ToArray();
            _client.Send(dataToSend, dataToSend.Length, new IPEndPoint(IPAddress.Broadcast, port));
        }

        public event EventHandler<IList<Byte>> PacketReceived;
        public event EventHandler<IList<Byte>> PacketReceived6;

        public void Disconnect()
        {
            _connected = false;
            _client.Close();
        }
    }
}
