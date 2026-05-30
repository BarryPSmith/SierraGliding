using core_Receiver.Packets;
using Newtonsoft.Json;
using Nito.AsyncEx;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace core_Receiver
{
    internal class CommandServer
    {
        AsyncAutoResetEvent _pollEvent = new AsyncAutoResetEvent(true);
        AsyncAutoResetEvent _commandEvent = new AsyncAutoResetEvent(false);
        ConcurrentDictionary<(byte stationId, byte messageId), (int foreignId, DateTimeOffset cmdTime)> _localIds = new ConcurrentDictionary<(byte stationId, byte messageId), (int foreignId, DateTimeOffset cmdTime)>();

        CommandInterpreter _commandInterpreter;

        bool _commandsAvailable;

        DateTimeOffset? _nextCommandTime = null;

        TimeSpan CommandInterval = TimeSpan.FromSeconds(5);
        TimeSpan CommandDuration = TimeSpan.FromMinutes(2);
        TimeSpan PollInterval = TimeSpan.FromSeconds(60);

        static HttpClient Client => DataPosting._client;
        string _url;
        byte? _lastMessageId;

        public int Offset { get; set; }

        private TextWriter _output = Console.Out;
        public TextWriter Output
        {
            get => _output;
            set
            {
                _output = value;
            }
        }

        public CommandServer(string url, int offset, CommandInterpreter commandInterpreter)
        {
            _url = url;
            Offset = offset;
            Task.Factory.StartNew(ReceiveLoop, default, TaskCreationOptions.LongRunning, TaskScheduler.Default);
            Task.Factory.StartNew(ProcessLoop, default, TaskCreationOptions.LongRunning, TaskScheduler.Default);
            _commandInterpreter = commandInterpreter;
        }

        async Task ReceiveLoop()
        {
            const int delay = 10000;
            while (true)
            {
                try
                {
                    Output.WriteLine("Connecting to websocket...");
                    var socketUrl = _url.Replace("http", "ws");
                    using var ws = new ClientWebSocket();
                    await ws.ConnectAsync(new Uri(socketUrl), default);
                    byte[] buffer = new byte[0x1000];
                    while (true)
                    {
                        MemoryStream ms = new MemoryStream();
                        WebSocketReceiveResult wsResult;
                        do
                        {
                            wsResult = await ws.ReceiveAsync(buffer, default);
                            ms.Write(buffer, 0, wsResult.Count);
                        } while (!wsResult.EndOfMessage);
                        if (wsResult.MessageType == WebSocketMessageType.Close)
                        {
                            Output.WriteLine("Websocket closed...");
                            await Task.Delay(delay);
                            continue; // re-open 
                        }
                        ms.Seek(0, SeekOrigin.Begin);
                        ProcessMessage(ms);
                    }
                }
                catch (Exception ex)
                {
                    Output.WriteLine($"Websocket error: {ex.Message}");
                    await Task.Delay(delay);
                }
            }
        }

        async Task ProcessLoop()
        {
            while (true)
            {
                try
                {
                    if (!_commandsAvailable)
                    {
                        using CancellationTokenSource timeoutSource = new CancellationTokenSource(PollInterval);
                        await _pollEvent.WaitAsync2(timeoutSource.Token);
                    }
                    await WaitUntilNextCommandSlotAsync();
                    await UpdateCommandsAsync();
                    await SendNextCommandAsync();
                }
                catch (Exception ex)
                {
                    Output.WriteLine($"Command error: {ex.Message}");
                }
            }
        }

        void ProcessMessage(MemoryStream ms)
        {
            StreamReader reader = new StreamReader(ms);
            var str = reader.ReadToEnd();
            var packet = JsonConvert.DeserializeObject<SierraGlidingPacket>(str);

            if (packet == null)
                return;

            if (packet.id < Offset || packet.id > Offset + 0xFF ||
                packet.type != SGType.command ||
                packet.op != SGOps.Add)
                return;

            _pollEvent.Set();
        }

        /* Command logic:
         *  - Have a list of possible comands
         *  - If a command is too old, remove it from the list
         *  - If we get a response to a command, remove it from the list
         *  - We need to remember message IDs to match responses to commands
         *  
         *  Which command to try next:
         *  - Order by attempt count, then by age.
         */

        async Task UpdateCommandsAsync()
        {
            _commands = await FetchCommandsAsync();
        }

        async Task WaitUntilNextCommandSlotAsync()
        {
            var timeToNextCommand = _nextCommandTime - DateTimeOffset.Now;
            if (timeToNextCommand == null || timeToNextCommand <= TimeSpan.Zero)
                return;
            using var timeoutSource = new CancellationTokenSource(timeToNextCommand.Value);
            await _commandEvent.WaitAsync2(timeoutSource.Token);
        }

        IEnumerable<SierraGlidingCommand> _commands;

        async Task<IEnumerable<SierraGlidingCommand>> FetchCommandsAsync()
        {
            var startTs = (DateTimeOffset.UtcNow - CommandDuration).ToUnixTimeSeconds();
            string completeUrl = $"{_url}/api/commands?start={startTs}&unhandled=true";
            var resp = await Client.GetAsync(completeUrl);
            if (!resp.IsSuccessStatusCode)
            {
                throw new Exception($"Unable to fetch commands: {resp.StatusCode}");
            }
            var respStr = await resp.Content.ReadAsStringAsync();
            var commands = JsonConvert.DeserializeObject<List<SierraGlidingCommand>>(respStr)
                .Where(cmd => cmd.station_id > Offset && cmd.station_id < Offset + 0xFF);
            return commands;
        }

        async Task SendNextCommandAsync()
        {
            _commandsAvailable = _commands != null && _commands.Any();
            if (!_commandsAvailable)
                return;
            var nextCommand = _commands.OrderBy(cmd => cmd.attempts)
                .ThenBy(cmd => cmd.request_time)
                .First();
            if (nextCommand.command_type == CommandType.QuerySignal)
                await HandleQuerySignalAsync(nextCommand);
            else
                await SendCommandAsync(nextCommand);
        }

        async Task HandleQuerySignalAsync(SierraGlidingCommand command)
        {
            if (!Program._lastDirectPackets.TryGetValue(normaliseId(command.station_id), out var tpl))
            {
                await PostResponse(command.ID, "No direct packets received.");
                return;
            }
            var age = tpl.received - DateTimeOffset.Now;
            await PostResponse(command.ID,
                $"Age: {age.TotalSeconds:F1} s ({tpl.received:yyyy-MM-dd HH:mm:ss})\n" +
                $"RSSI: {tpl.rssi:F1} dBm\n" +
                $"SNR: {tpl.snr} dB");
        }

        async Task SendCommandAsync(SierraGlidingCommand command)
        {
            try
            {
                Output.WriteLine($"Sending remote command {command.ID} ({command.command_type})");

                byte messageId;
                if (command.command_type == CommandType.Raw)
                {
                    messageId = _commandInterpreter.HandleCommand($"\\b{command.station_id}${command.command_data}", false);
                }
                else
                {
                    List<byte> data = EncodeCommand(command);
                    messageId = _commandInterpreter.HandleCommand(data, false);
                }
                _lastMessageId = messageId;
                var stationId = (byte)(command.station_id - Offset);
                _localIds[(stationId, messageId)] = (command.ID, DateTimeOffset.Now);
                _nextCommandTime = DateTimeOffset.Now + CommandInterval;
                var completeUrl = $"{_url}/api/commands/{command.ID}/attempt";
                try
                {
                    var resp = await Client.PostAsync(completeUrl, null);
                    if (!resp.IsSuccessStatusCode)
                        Output.WriteLine($"Unable to post attempt for {command.ID}: {resp.StatusCode}");
                }
                catch { }
            }
            catch (Exception ex)
            {
                await PostResponse(command.ID, $"Unexpected Exception: {ex.Message}");
            }
            CleanupLocalIds();
        }

        byte[] ObjectToBytes(object o)
        {
            return o switch
            {
                byte b => [b],
                int i => BitConverter.GetBytes(i),
                uint ui => BitConverter.GetBytes(ui),
                double d => BitConverter.GetBytes(d),
                long l => BitConverter.GetBytes(l),
                ulong ul => BitConverter.GetBytes(ul),
                float f => BitConverter.GetBytes(f),
                short s => BitConverter.GetBytes(s),
                ushort us => BitConverter.GetBytes(us),
                _ => throw new Exception()
            };
        }

        byte[] GetBytes(long val, Type t)
        {
            var typedValue = Convert.ChangeType(val, t);
            return ObjectToBytes(typedValue);
        }

        List<byte> EncodeCommand(SierraGlidingCommand command)
        {
            List<byte> ret = [(byte)(command.station_id - Offset)];
            switch (command.command_type)
            {
                case CommandType.Radio:
                    var radioData = JsonConvert.DeserializeObject<GenericCommandOptions<RadioCommandTypes>>(command.command_data);
                    var (radioCmdByte, radioDataType) = RadioCommandDict[radioData.Parameter];
                    ret.Add((byte)'M');
                    ret.Add((byte)radioCmdByte);
                    ret.AddRange(GetBytes(radioData.value, radioDataType));
                    break;
                case CommandType.Relay:
                    var relayData = JsonConvert.DeserializeObject<List<RelaySpec>>(command.command_data);
                    if (!relayData.Any())
                        return null;
                    ret.Add((byte)'R');
                    foreach (var spec in relayData)
                    {
                        ret.Add((byte)(spec.Add ? '+' : '-'));
                        ret.Add((byte)(spec.Command ? 'C' : 'W'));
                        ret.Add(normaliseId(spec.Id));
                    }
                    break;
                case CommandType.Battery:
                    var batteryData = JsonConvert.DeserializeObject<BatteryParameters>(command.command_data);
                    ret.Add((byte)'B');
                    ret.AddRange(BitConverter.GetBytes(batteryData.Threshold));
                    ret.AddRange(BitConverter.GetBytes(batteryData.Emergency));
                    break;
                case CommandType.QueryStatus:
                    ret.AddRange(Encoding.UTF8.GetBytes("QV"));
                    break;
                case CommandType.QueryConfig:
                    ret.AddRange(Encoding.UTF8.GetBytes("QC"));
                    break;
                case CommandType.Restart:
                    ret.AddRange(Encoding.UTF8.GetBytes("F"));
                    break;
                case CommandType.Charging:
                    ret.Add((byte)'C');
                    var chargingData = JsonConvert.DeserializeObject<ChargingParameters>(command.command_data);
                    ret.AddRange(BitConverter.GetBytes(chargingData.DesiredVoltage));
                    ret.AddRange(BitConverter.GetBytes(chargingData.ResponseRate));
                    ret.AddRange(BitConverter.GetBytes(chargingData.FreezingVoltage));
                    ret.Add(chargingData.FreezingPwm);
                    break;
                case CommandType.ID:
                    ret.Add((byte)'U');
                    var newIdData = JsonConvert.DeserializeObject<GenericCommandOptions<object>>(command.command_data);
                    if (newIdData.value == 0)
                        ret.AddRange(Encoding.UTF8.GetBytes("UR"));
                    else
                    {
                        ret.Add((byte)'S');
                        ret.Add(normaliseId(newIdData.value));
                    }
                    break;
                case CommandType.Weather:
                    return null; // Not really used.
                case CommandType.ReportInterval:
                    ret.Add((byte)'I');
                    var newIntervalData = JsonConvert.DeserializeObject<GenericCommandOptions<object>>(command.command_data);
                    ret.AddRange(BitConverter.GetBytes((uint)newIntervalData.value));
                    break;
                default:
                    return null;
            }
            return ret;
        }

        byte normaliseId(long value)
        {
            if (value > 0xFF)
                value -= Offset;
            if (value < 0 || value > 0xFF)
                throw new InvalidDataException("Id must be between 0 and 255.");
            return (byte)value;
        }

        enum RadioCommandTypes
        {
            Power, CMSA_P, CSMA_T, Frequency, Bandwidth, SpreadingFactor, OutboundPreamble,
            InboundPreamble, BoostedRx, CodingRate, RelayListenPeriod
        }
        Dictionary<RadioCommandTypes, (char cmd, Type dataType)> RadioCommandDict = new Dictionary<RadioCommandTypes, (char cmd, Type dataType)>
        {
            { RadioCommandTypes.Power, ('P', typeof(Int16) ) },
            { RadioCommandTypes.CMSA_P, ('C', typeof(byte) ) },
            { RadioCommandTypes.CSMA_T, ('T', typeof(UInt32) ) },
            { RadioCommandTypes.Frequency, ('F', typeof(UInt32)) },
            { RadioCommandTypes.Bandwidth, ('B', typeof(UInt16)) },
            { RadioCommandTypes.SpreadingFactor, ('S', typeof(byte)) },
            { RadioCommandTypes.OutboundPreamble, ('O', typeof(UInt16)) },
            { RadioCommandTypes.InboundPreamble, ('I', typeof(UInt16)) },
            { RadioCommandTypes.BoostedRx, ('R', typeof(bool)) },
            { RadioCommandTypes.CodingRate, ('E', typeof(byte)) },
            { RadioCommandTypes.RelayListenPeriod, ('A', typeof(UInt16)) }
        };

        class GenericCommandOptions<T>
        {
            public T Parameter { get; set; }
            public long value { get; set; }
        }

        class BatteryParameters
        {
            public short Threshold { get; set; }
            public short Emergency { get; set; }
        }

        class RelaySpec
        {
            public bool Add { get; set; }
            public bool Command { get; set; }
            public int Id { get; set; }
        }

        class ChargingParameters
        {
            public short DesiredVoltage { get; set; }
            public short ResponseRate { get; set; }
            public short FreezingVoltage { get; set; }
            public byte FreezingPwm { get; set; }
        }

        void CleanupLocalIds()
        {
            bool anyChanges = true;
            while (anyChanges)
            {
                anyChanges = false;
                foreach (var kvp in _localIds)
                {
                    if (kvp.Value.cmdTime < DateTimeOffset.Now - CommandInterval &&
                        !_commands.Any(c => c.ID == kvp.Value.foreignId))
                    {
                        _localIds.TryRemove(kvp.Key, out _);
                        anyChanges = true;
                        break;
                    }
                }
            }
        }

        public async void OnResponseReceivedAsync(Packet packet)
        {
            try
            {
                if (!_localIds.TryRemove((packet.sendingStation, packet.uniqueID), out var source))
                    return;
                Output.WriteLine($"Posting command response to {source.foreignId}");
                var foreignId = source.foreignId;
                var response = packet.SafeDataString;
                await PostResponse(foreignId, response);
                if (packet.uniqueID == _lastMessageId)
                    _commandEvent.Set();
            }
            catch (Exception ex)
            {
                Output.WriteLine($"Excception CommandServer.OnResponseReceived: {ex.Message}");
            }
        }

        private async Task PostResponse(int foreignId, string response)
        {
            var uri = $"{_url}/api/commands/{foreignId}/response";
            var json = JsonConvert.SerializeObject(new { response });
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var resp = await Client.PostAsync(uri, content);
            if (!resp.IsSuccessStatusCode)
                Output.WriteLine($"Failed to post command response: {resp.StatusCode}");
        }
    }

    enum SGOps { Add, Remove, Attempt, Respond }
    enum SGType { weather, command }

    enum CommandType
    {
        Unknown, QueryConfig, QueryStatus, QuerySignal,
        Battery, Radio, Relay, ReportInterval,
        Charging, Weather, ID, Restart,
        Raw
    }
    class SierraGlidingCommand
    {
        public int ID { get; set; }
        public int station_id { get; set; }
        public CommandType command_type { get; set; } = CommandType.Unknown;
        public string command_data { get; set; }
        public int request_time { get; set; }
        public int attempts { get; set; }
        public int response_time { get; set; }
        public string response { get; set; }
    }

    class SierraGlidingPacket
    {
        public SGType type { get; set; }
        public SGOps op { get; set; }
        public int id { get; set; }
        public uint timestamp { get; set; }
        public double wind_direction { get; set; }
        public double wind_direction_avg { get; set; }
        public double windspeed { get; set; }
        public double windspeed_avg { get; set; }
        public double windspeed_min { get; set; }
        public double windspeed_max { get; set; }
        public int command_id { get; set; }
    }
}
