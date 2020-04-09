using System;
using System.Collections.Generic;
using System.IO;
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
        int _sendPort;
        volatile bool _connected = true;

        public TextWriter OutputWriter => throw new NotImplementedException();

        public Action<string> StreamError { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }

        public SocketListener(int listenPort, int sendPort)
        {
            _client = new UdpClient();
            _client.Client.Bind(new IPEndPoint(IPAddress.Any, listenPort));
            _sendPort = sendPort;
        }

        public async Task StartAsync(CancellationToken token = default(CancellationToken))
        {
            if (token.CanBeCanceled)
                token.Register(() => _client.Close());
            while (_connected && !token.IsCancellationRequested)
            {
                try
                {
                    var result = await _client.ReceiveAsync();
                    PacketReceived?.Invoke(this, result.Buffer);
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
            if (writeType != 0)
                throw new NotImplementedException("SocketListener can only send type zero packets");
            if (!_connected)
                throw new ObjectDisposedException("SocketListener is already closed!");
            _client.Send(data, data.Length, new IPEndPoint(IPAddress.Broadcast, _sendPort));
        }

        public event EventHandler<IList<Byte>> PacketReceived;

        public void Disconnect()
        {
            _connected = false;
            _client.Close();
        }
    }
}
