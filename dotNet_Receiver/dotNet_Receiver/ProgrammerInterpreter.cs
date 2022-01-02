using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace core_Receiver
{
    partial class CommandInterpreter
    {
        class ProgrammerInterpreter
        {
            public const string Help =
@"Programmer Regex:
P(?<id>\d*)(\.(?<method>\w+))?(\s+(?<argstring>.+))?
If id is not present, uses P0

Initialisation:
P(?<id>\d*) (?<Filename>.+)

Commands:
 P            Display programmer info.
 P.Cancel     Cancels the current command
 P.Go         Performs Init, Bulk, Complete (optionally Confirm)
              First parameter: List of stationIDs. Comma separated numbers. No Zero.
              Second parameter (opt): Timeout in seconds for entire command.
              Third parameter (opt): 'True' to perform Confirm.

These commands all take a single station ID as their parameter
 P.Init       Initialise a station to receive the image.
 P.Bulk       Performs a bulk upload. 
 P.Complete   Uploads until a station has a complete image.
 P.Confirm    Instructs a station with a complete image to use that image.
 P.Query      Query a station for current image status.
 P.ReadRemote Reads the image present on a remote station (For debugging)

Adjust programmer settings
 P.Timeout    Sets the timeout for each message, in seconds.
 P.Interval   Sets the interval between message, in seconds.
";

            readonly Dictionary<int, ProgrammerWrapper> _programmers = new Dictionary<int, ProgrammerWrapper>();
            readonly ICommunicator _comminucator;
            private TextWriter _outWriter = Console.Out;
            public TextWriter OutWriter
            {
                set
                {
                    _outWriter = value;
                    foreach (var wrapper in _programmers.Values)
                        wrapper.OutWriter = value;
                }
                get => _outWriter;
            }

            class ProgrammerWrapper
            {
                public int ID { get; private set; }
                public TextWriter OutWriter
                {
                    get => Programmer.OutWriter;
                    set => Programmer.OutWriter = value;
                }
                public RemoteProgrammer Programmer { get; private set; }
                public CancellationTokenSource TokenSource { get; private set; }
                public Task Task { get; private set; }
                public string LastCommand { get; private set; }
                public ProgrammerWrapper(string fn, int id, ICommunicator comminucator)
                {
                    Programmer = new RemoteProgrammer(fn, comminucator);
                    //File.WriteAllText(Path.ChangeExtension(Programmer.Fn, ".orig"), Programmer.DataAsSingleLineHex);
                    ID = id;
                }

                public void HandleCommand(string cmd, string method, string argString)
                {
                    if (string.IsNullOrEmpty(method) || method == "ToString")
                    {
                        OutWriter.WriteLine(ToString());
                        return;
                    }

                    if (method == "Cancel")
                    {
                        if (TokenSource == null)
                            throw new CommandException("No task to cancel.");
                        else if (Task?.IsCompleted == true)
                            throw new CommandException("Task has already completed.");
                        else if (TokenSource.IsCancellationRequested)
                            throw new CommandException("Task has already had cancellation requested.");
                        else
                            TokenSource.Cancel();
                        return;

                    }

                    if (Task?.IsCompleted == false)
                        throw new CommandException($"Programmer is busy. Current task: {LastCommand}");

                    var args = argString.Split(' ', StringSplitOptions.RemoveEmptyEntries);

                    Task t = null;
                    TokenSource = new CancellationTokenSource();

                    byte? stationID = args.Length >= 1 ? ParseStationID(args[0]) : null;
                    bool demandRelay = args.Length >= 2 && bool.TryParse(args[1], out var tmpDemandRelay) && tmpDemandRelay;
                    switch (method)
                    {
                        case "BulkUpload":
                        case "Bulk":
                        case "B":
                        case nameof(RemoteProgrammer.BulkUploadToStation):
                            if (stationID == null)
                                throw new CommandException($"BulkUpload requires station ID as first parameter (can be zero).");
                            t = Programmer.BulkUploadToStation(stationID.Value, demandRelay, TokenSource.Token);
                            break;
                        case "Complete":
                        case "CompleteUpload":
                        case "CU":
                        case nameof(RemoteProgrammer.CompleteUploadToStationAsync):
                            if (stationID == null)
                                throw new CommandException("CompleteUpload requires station ID as first parameter (cannot be zero).");
                            t = Programmer.CompleteUploadToStationAsync(stationID.Value, TokenSource.Token);
                            break;
                        case "Confirm":
                        case "CP":
                        case nameof(RemoteProgrammer.ConfirmProgramming):
                            if (stationID == null)
                                throw new CommandException("ConfirmProgramming requires stationID as first parameter (cannot be zero).");
                            Programmer.ConfirmProgramming(stationID.Value);
                            break;
                        case "Go":
                        case nameof(RemoteProgrammer.GoAsync):
                            if (args.Length < 1)
                                throw new CommandException("Go requires stationID list as first parameter (cannot contain zero).");
                            var stationIDs = args[0].Split(',').Select(str => ParseStationID(str).Value);
                            TimeSpan timeout = TimeSpan.FromMinutes(5);
                            if (args.Length >= 2)
                                timeout = TimeSpan.FromSeconds(double.Parse(args[1]));
                            bool confirm = false;
                            if (args.Length >= 3)
                                confirm = bool.Parse(args[2]);
                            t = Programmer.GoAsync(stationIDs, demandRelay, timeout, confirm);
                            break;
                        case "Init":
                        case "I":
                        case nameof(RemoteProgrammer.InitUploadToStation):
                            if (stationID == null)
                                throw new CommandException("InitUpload requires stationID as first parameter (can be zero).");
                            Programmer.InitUploadToStation(stationID.Value, demandRelay);
                            break;
                        case "Query":
                        case "Q":
                        case nameof(RemoteProgrammer.QueryStationProgramming):
                            Programmer.QueryStationProgramming(stationID ?? 0);
                            break;
                        case "Timeout":
                        case nameof(RemoteProgrammer.ResponseTimeout):
                            if (args.Length < 1 || !double.TryParse(args[0], out var timeoutSeconds))
                                throw new CommandException("Cannot parse timeout seconds.");
                            Programmer.ResponseTimeout = TimeSpan.FromSeconds(timeoutSeconds);
                            break;
                        case "Interval":
                        case nameof(RemoteProgrammer.PacketInterval):
                            if (args.Length < 1 || !double.TryParse(args[0], out var intervalSeconds))
                                throw new CommandException("Cannot parse interval seconds.");
                            Programmer.PacketInterval = TimeSpan.FromSeconds(intervalSeconds);
                            break;
                        case "ReadRemote":
                        case nameof(RemoteProgrammer.ReadRemoteImageAsync):
                            if (stationID == null)
                                throw new CommandException("ReadRemote requires stationID as first parameter");
                            t = Programmer.ReadRemoteImageAsync(stationID.Value, TokenSource.Token)
                                .ContinueWith(task => File.WriteAllText(Path.ChangeExtension(Programmer.Fn, ".remote"),
                                    task.Result.ToCsv(b => b.ToString("X2"), Environment.NewLine)));
                            break;
                        case nameof(RemoteProgrammer.FullBandwidth):
                        case "FB":
                            if (args.Length < 1 || !bool.TryParse(args[0], out var fb))
                                throw new CommandException("Cannot parse full bandwidth value (must be True or False).");
                            Programmer.FullBandwidth = fb;
                            break;
                        default:
                            throw new CommandException($"Command {cmd} not recognised.");
                    }

                    if (t == null)
                        TokenSource = null;
                    
                    Task = t;
                    t?.ContinueWith(T =>
                    {
                        if (T.IsFaulted)
                        {
                            OutWriter.WriteLine($"{ID}: Exception performing: {LastCommand}");
                            OutWriter.WriteLine(T.Exception);
                        }
                        else if (T.IsCanceled)
                            OutWriter.WriteLine($"{ID}: Task canceled: {LastCommand}");
                        else
                            OutWriter.WriteLine($"{ID}: Task completed: {LastCommand}");
                    });

                    LastCommand = cmd;
                }

                byte? ParseStationID(string s)
                {
                    if (s.StartsWith('\\') && byte.TryParse(s.Substring(1), out var tmpStationId))
                        return tmpStationId;
                    else if (s.Length == 1)
                        return (byte)s[0];
                    else
                        return null;
                }

                public override string ToString()
                {
                    string ret = Programmer.ToString();
                    if (Task != null)
                    {
                        ret += Environment.NewLine;
                        ret += $"Last Command: {LastCommand} ({Task.Status}) / {Programmer.StatusMessage}";
                        if (Task.Status == TaskStatus.Faulted)
                        {
                            ret += Environment.NewLine;
                            ret += Task.Exception;
                        }
                    }
                    return ret;
                }
            }

            public ProgrammerInterpreter(ICommunicator comminucator)
            {
                _comminucator = comminucator;
            }

            static readonly Regex _programmerRegex = new Regex(@"P(?<id>\d*)(\.(?<method>\w+))?(\s+(?<argstring>.+))?",
                RegexOptions.ExplicitCapture | RegexOptions.Compiled);
            public void HandleProgrammingCommand(string cmd)
            {
                var match = _programmerRegex.Match(cmd);
                if (!match.Success)
                    throw new CommandException("Unable to parse programmer command.");
                int id = 0;
                var idStr = match.Groups["id"].Value;
                if (idStr.Length > 0)
                    id = int.Parse(idStr);
                bool isConstruct = !match.Groups["method"].Success && match.Groups["argstring"].Success;
                bool isDelete = match.Groups["method"].Value.Equals("Delete", StringComparison.OrdinalIgnoreCase);
                if (isConstruct)
                {
                    if (_programmers.ContainsKey(id))
                        throw new CommandException($"Programmer with ID {id} already exists.");
                    if (!match.Groups["argstring"].Success)
                        throw new CommandException($"Programmer constructor: No filename specified. Expected input: P<id>? <filename>");

                    var fn = match.Groups["argstring"].Value;

                    _programmers[id] = new ProgrammerWrapper(fn, id, _comminucator)
                    {
                        OutWriter = OutWriter
                    };
                    OutWriter.WriteLine($"Programmer {id}: {_programmers[id]}");
                }
                else
                {
                    if (!_programmers.TryGetValue(id, out var programmer))
                        throw new CommandException($"Programmer with ID {id} doesn't exist.");
                    if (isDelete)
                        _programmers.Remove(id);
                    else
                        programmer.HandleCommand(cmd, match.Groups["method"].Value, match.Groups["argstring"].Value);
                }
            }
        }
    }

}
