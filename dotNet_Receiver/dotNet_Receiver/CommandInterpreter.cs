using dotNet_Receiver;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace core_Receiver
{
    class CommandInterpreter
    {
        readonly KissCommunication _modem;

        public TextWriter Output { get; set; }

        public CommandInterpreter(KissCommunication modem)
        {
            _modem = modem;
        }

        public bool HandleLine(string line)
        {
            if (line.Length < 1)
                return true;

            switch (line[0])
            {
                case 'q': //Quit
                    return false;
                case ':': //Commands (TODO)
                    break;
                case '/': //Simple
                    var reader = new SimpleLineReader(line.Substring(1));
                    reader.Go();
                    var data = reader.Encoded;
                    Output.WriteLine("Command: ");
                    for (int i = 0; i < data.Length; i++)
                        Output.Write($"{Encoding.ASCII.GetChars(data, i, 1)[0]} {data[i]:X2} ");
                    Output.Write("Continue? ");
                    var confirmation = Console.ReadLine();
                    if (string.IsNullOrWhiteSpace(confirmation) ||
                        confirmation.StartsWith("Y", StringComparison.OrdinalIgnoreCase))
                    {
                        using MemoryStream dataStream = new MemoryStream(data);
                        _modem.WriteSerial(dataStream);
                        Output.WriteLine("Command Sent.");
                    }
                    else
                        Output.WriteLine("Command Cancelled.");
                    break;
            }

            return true;
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
                        _curIntWrite(_curCount);
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
                    _curIntWrite(_curCount);
                    Enter(State.Normal);
                }
                else if (Str[_i] == '\\')
                {
                    _curIntWrite(_curCount);
                    Enter(State.Escaped);
                }
                else if (byte.TryParse(Str.AsSpan(_i, 1), _curNumStyle, null, out var b))
                {
                    _curCount *= _curMult;
                    _curCount += b;
                }
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

                    _curIntWrite(_curCount);
                    _i--;
                    Enter(State.Normal);
                }
            }
            Action<int> _curIntWrite;

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
                        _curMult = 10;
                        _curNumStyle = NumberStyles.Integer;
                        break;
                }
                switch (state)
                {
                    case State.Byte:
                        _curIntWrite = v => _bw.Write((byte)v);
                        break;
                    case State.Short:
                        _curIntWrite = v => _bw.Write((short)v);
                        break;
                    case State.Int:
                        _curIntWrite = v => _bw.Write(v);
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
