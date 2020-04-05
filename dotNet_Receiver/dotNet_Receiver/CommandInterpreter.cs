using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace core_Receiver
{
    class CommandException : Exception
    {
        public CommandException(string message) : base(message)
        {
        }
    }

    partial class CommandInterpreter
    {
        readonly KissCommunication _modem;
        readonly ProgrammerInterpreter _programmerInterpreter;

        private TextWriter _output = Console.Out;
        public TextWriter Output 
        {
            get => _output;
            set
            {
                _output = value;
            }
        } 

        public TextWriter ProgrammerOutput
        {
            set => _programmerInterpreter.OutWriter = value;
        }

        public CommandInterpreter(KissCommunication modem)
        {
            _modem = modem;
            _programmerInterpreter = new ProgrammerInterpreter(_modem);
        }

        public bool HandleLine(string line)
        {
            if (line.Length < 1)
                return true;

            switch (line[0])
            {
                case 'q': //Quit
                    return false;
                case 'h':
                case 'H':
                    PrintHelp(line);
                    break;
                case ':': //Commands
                    HandleCommand(line);
                    break;
                case '/': //Simple
                    HandleSimpleLine(line, 0x00);
                    break;
                case '6': //Modem instruction
                    HandleSimpleLine(line, 0x06);
                    break;
                case 'P': //Programming...
                    _programmerInterpreter.HandleProgrammingCommand(line);
                    break;
            }

            return true;
        }

        void PrintHelp(string line)
        {
            if (line.Length > 1)
            {
                switch (line[1])
                {
                    case '/':
                        Output.WriteLine(c_slashHelp);
                        return;
                    case '6':
                        Output.WriteLine(c_6Help);
                        return;
                    case ':':
                        Output.WriteLine(c_cmdHelp);
                        return;
                    case 'P':
                        Output.WriteLine(ProgrammerInterpreter.Help);
                        return;
                }
            }
            Output.WriteLine("Command Line Interface");
            Output.WriteLine("Start characters:");
            Output.WriteLine(" H : Show this message");
            Output.WriteLine(" / : Send data directly over the radio");
            Output.WriteLine(" : : Issue a command over the radio");
            Output.WriteLine(" P : Use the programmer interface");
            Output.WriteLine(" 6 : Send data directly to the modem");
            Output.WriteLine();
            Output.WriteLine("Type H followed by a start character for more help on that character");
            Output.WriteLine();
            Output.WriteLine("Special characters (For /, :, and 6 modes)");
            Output.WriteLine(@" \HH : Send a byte. H is a hexadecimal character. First character must not be lower case b.");
            Output.WriteLine(@" \bn : Send a byte. n is a number. If n starts with x or 0x, it will be interpreted as hexadecimal.");
            Output.WriteLine(@" \sn : Send 2 bytes. n is a number.");
            Output.WriteLine(@" \in : Send 4 bytes. n is a number.");
            Output.WriteLine(@" \fn : Send a 4 byte float. n is a number.");
            Output.WriteLine("Number parsing stops when an invalid character is encountered. Use '$' to stop number parsing without adding anything to the stream.");
        }

        const string c_slashHelp =
@" / : Enter data to be sent directly over the radio.
Standard message format is (MsgType)(StationID)(MessageID)[Message...]";
        const string c_cmdHelp =
@" : : Enter a command to send over the radio.
(StationID)[Command...]
A C type message with the next MessageID will be sent to (StationID) with contents [Command...].
Command starting characters:
 R : Change relay settings.     : ((+|-)(W|C)(?<StationID>.))+
 I : Change reporting interval. : (datalength)(shortInterval)(longInterval)
 B : Change battery thresholds. : (2 byte new threshold in mV)
 Q : Query station.             : QV for volatile data. QC for config data.
 O : Set Override interval.     : (L|S)(4 byte new interval)(H|M)
 M : Change radio settings.     : Same as modem. H6 for more info. (P|C|T|F|B|S|O)
 W : Change weather settings    : (C|O|G)(newValue) C: calibrate wind O: set temp offset G: set temp gain
 P : Reprogram station          : (Use programmer interface instead. )
 U : Change station ID          : UR for random. US(newID) to specify.";
        const string c_6Help =
@" 6 : Enter data to be sent to the modem.
Command starting characters:
 P : TxPower.          2 bytes  signed.
 C : CSMA Probability. 1 byte   from 0 - 255
 T : CSMA Timeslot.    4 bytes  in microseconds
 F : Frequency.        4 bytes  in Hertz
 B : Bandwidth.        2 bytes  in HectoHertz (kHz * 10)
 S : Spreading Factor. 1 byte
 O : Outbound Preamble Length. 2 bytes
 I : Get radio stats. Modem only.";


        void HandleSimpleLine(string line, byte packetType)
        {
            var reader = new SimpleLineReader(line.Substring(1));
            reader.Go();
            HandleMessage(reader.Encoded, packetType);
        }

        void HandleCommand(string line)
        {
            var reader = new SimpleLineReader(line.Substring(1));
            reader.Go();
            var data = reader.Encoded.ToList();
            if (data.Count < 1)
                throw new InvalidOperationException("Must specify destination station ID.");
            data.Insert(0, (byte)'C');
            //Byte 1 of the encoded data is the destination station ID.
            data.Insert(2, (byte)_modem.GetNextUniqueID());
            HandleMessage(data.ToArray(), 0);
        }

        void HandleMessage(byte[] data, byte packetType)
        {
            Output.WriteLine("Command: ");
            for (int i = 0; i < data.Length; i++)
                Output.Write($"{Encoding.ASCII.GetChars(data, i, 1)[0]} {data[i]:X2} ");
            Output.Write("Continue? ");
            var confirmation = Console.ReadLine();
            if (string.IsNullOrWhiteSpace(confirmation) ||
                confirmation.StartsWith("Y", StringComparison.OrdinalIgnoreCase))
            {
                RecordSentCommand(data);
                _modem.WriteSerial(data, packetType);
                Output.WriteLine("Command Sent.");
            }
            else
                Output.WriteLine("Command Cancelled.");
        }

        static void RecordSentCommand(byte[] data)
        {
            //0 1    2    3 4
            //# Stat UnId ...
            //C 0x00 0x00 Q V 
            if (data.Length < 4 || data[0] != 'C')
                return;
            var cmdType = (char)data[3];
            string commandIdentifier = cmdType.ToString();
            switch (cmdType)
            {
                case 'Q':
                    char queryType = 'C';
                    if (data.Length >= 5)
                        queryType = (char)data[4];
                    commandIdentifier += queryType;
                    break;
                case 'P':
                    if (data.Length >= 5)
                        if (data[4] == 'Q')
                            commandIdentifier += data[4];
                    break;
            }
            PacketDecoder.RecentCommands[data[2]] = commandIdentifier;
        }

        class SimpleLineReader
        {
            public SimpleLineReader(string str)
            {
                Str = str;
                _bw = new BinaryWriter(_ms);
            }

            public string Str { get; private set; }

            int _i = 0;
            readonly MemoryStream _ms = new MemoryStream();
            readonly BinaryWriter _bw;
            enum State
            {
                Normal,
                Escaped,
                Byte,
                Short,
                Int,
                Float,
            }
            State _curState;

            int _curStart = 0;
            int _curCount = 0;
            int _curSign = 1;
            int _curMult = 10;
            NumberStyles _curNumStyle;

            public Byte[] Encoded
            {
                get
                {
                    _bw.Flush();
                    return _ms.ToArray();
                }
            }

            public void Go()
            {
                while (_i < Str.Length)
                {
                    Step();
                    _i++;
                }
                End();
            }

            void Step()
            {
                switch (_curState)
                {
                    case State.Normal:
                        StepNormal();
                        return;
                    case State.Escaped:
                        StepEscaped();
                        return;
                    case State.Byte:
                    case State.Short:
                    case State.Int:
                        StepInteger();
                        return;
                    case State.Float:
                        StepFloat();
                        return;
                }
            }

            void End()
            {
                switch (_curState)
                {
                    case State.Byte:
                    case State.Int:
                    case State.Short:
                        _curLongWrite(_curSign * _curCount);
                        break;
                }
            }

            void StepNormal()
            {
                switch (Str[_i])
                {
                    case '\\':
                        Enter(State.Escaped);
                        return;
                    default:
                        _bw.Write(Str[_i]);
                        return;
                }
            }

            void StepEscaped()
            {
                switch (Str[_i])
                {
                    case '\\':
                        _bw.Write('\\');
                        Enter(State.Normal);
                        break;
                    case 'b':
                        Enter(State.Byte);
                        break;
                    case 's':
                        Enter(State.Short);
                        break;
                    case 'i':
                        Enter(State.Int);
                        break;
                    case 'f':
                        Enter(State.Float);
                        break;
                    default:
                        if (byte.TryParse(Str.AsSpan(_i, 2), NumberStyles.HexNumber, null, out var b))
                        {
                            _i++;
                            Enter(State.Normal);
                            _bw.Write(b);
                            break;
                        }
                        else
                        {
                            throw new Exception($"Unexpected escaped '{Str[_i]}' character at location {_i}");
                        }
                }
            }

            void StepInteger()
            {
                if (Str[_i] == '$')
                {
                    _curLongWrite(_curSign * _curCount);
                    Enter(State.Normal);
                }
                else if (Str[_i] == '\\')
                {
                    _curLongWrite(_curSign * _curCount);
                    Enter(State.Escaped);
                }
                else if (byte.TryParse(Str.AsSpan(_i, 1), _curNumStyle, null, out var b))
                {
                    _curCount *= _curMult;
                    _curCount += b;
                }
                else if (_curCount == 0 && Str[_i] == '-' && _curSign == 1)
                    _curSign = -1;
                else if (_curCount == 0 && Str[_i] == 'x' && _curNumStyle != NumberStyles.HexNumber)
                {
                    _curMult = 16;
                    _curNumStyle = NumberStyles.HexNumber;
                }
                else
                {
                    var msg = $"Integer terminated by '{Str[_i]}' at location {_i}";
                    if (_curStart == _i)
                        throw new Exception(msg);
                    else
                        Console.Error.WriteLine(msg);

                    _curLongWrite(_curCount);
                    _i--;
                    Enter(State.Normal);
                }
            }
            Action<long> _curLongWrite;

            void StepFloat()
            {
                float lastVal = 0;
                int start = _i;
                while (++_i < Str.Length)
                {
                    if (float.TryParse(Str.AsSpan(start, _i - start), out lastVal))
                        continue;
                    else if(Str[_i] == '$')
                        break;
                    else
                    {
                        string msg = $"Float terminated by '{Str[_i]}' at location {_i}";
                        if (_i == start)
                            throw new Exception(msg);
                        else
                            Console.Error.WriteLine(msg);
                        _i--;
                        break;
                    }
                }
                _bw.Write(lastVal);
            }

            void Enter(State state)
            {
                switch (state)
                {
                    case State.Byte:
                    case State.Short:
                    case State.Int:
                        _curStart = 0;
                        _curCount = 0;
                        _curSign = 1;
                        _curMult = 10;
                        _curNumStyle = NumberStyles.Integer;
                        break;
                }
                switch (state)
                {
                    case State.Byte:
                        _curLongWrite = v => _bw.Write((byte)v);
                        break;
                    case State.Short:
                        _curLongWrite = v => _bw.Write((ushort)v);
                        break;
                    case State.Int:
                        _curLongWrite = v => _bw.Write((uint)v);
                        break;
                }
                _curState = state;
            }
        }
    }
    public static class Extensions
    {
        public static bool IsHex(this char c)
        {
            return (c >= '0' && c <= '9') ||
                     (c >= 'a' && c <= 'f') ||
                     (c >= 'A' && c <= 'F');
        }
    }

}
