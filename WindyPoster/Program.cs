using System;
using System.Net.Http;
using System.Data.SQLite;
using System.Threading.Tasks;
using System.Threading;
using System.Collections.Generic;
using Newtonsoft.Json;
using System.Text;
using System.Linq;
using System.IO;
using System.Diagnostics;

namespace WindyPoster
{
    static class Program
    {
        static readonly HttpClient _client = new HttpClient();
        static string _url;
        static string _fn;
        static string _extensionLocation;
        static readonly ManualResetEvent _shutdownEvent = new ManualResetEvent(false);
        static readonly TimeSpan _sendInterval = TimeSpan.FromMinutes(5);
        static int[] _stationIds = new [] { 49, 50, 51 };

        static bool ParseArgs(string[] args)
        {
            var helpIndex = Array.FindIndex(args, arg => arg.Equals("--help"));
            if (helpIndex >= 0)
                return false;

            var key = GetKey(args);
            var baseUrl = GetValue(args, "--url") ?? "https://stations.windy.com/pws/update/";
            var db = GetValue(args, "--db");
            var stationIds = GetValue(args, "--stations");
            _extensionLocation = GetValue(args, "--ext") ?? "../web/sqlite3_ext/extension-functions";

            

            if (string.IsNullOrEmpty(key) || string.IsNullOrEmpty(db))
                return false;

            if (!baseUrl.EndsWith("/"))
                baseUrl += "/";

            if (!string.IsNullOrEmpty(stationIds))
                _stationIds = stationIds.Split(',').Select(v => int.Parse(v)).ToArray();

            _url = baseUrl + key;
            _fn = db;

            return true;
        }

        static string GetKey(string[] args)
        {
            var key = GetValue(args, "--key");
            if (!string.IsNullOrEmpty(key))
                return key;
            var keyFile = GetValue(args, "--keyfile");
            if (string.IsNullOrEmpty(keyFile))
                return null;
            return File.ReadAllText(keyFile);
        }

        static string GetValue(string[] args, string key)
        {
            var idx = Array.FindIndex(args, arg => arg.Equals(key));
            if (idx < 0)
                return null;
            if (idx == args.Length - 1)
                throw new InvalidOperationException($"Argument to '{key}' not found.");
            return args[idx + 1];
        }

        static void Help()
        {
            Console.WriteLine(
@"Sierra Gliding Windy Poster.

Usage: dotnet WindyPoster.dll [--db <DB>] [--key <KEY> | --keyfile <FILENAME>] [--url <BASE_URL>] [--ext <Extension_funcs>] [--stations <LIST>]

 Options:
 --db <DB>         [required] Specify the SQLite database to use
 --key <KEY>       [optional] Windy API key. If not specified, then keyfile must be specified
 --keyfile <NAME>  [optional] Location of file that contains Windy API key. If not specified then key must be specified.
 --url <BASE_URL>  [optional] send data to a different endpoint. Default https://stations.windy.com/pws/update/
 --ext <EXT>       [optional] specify where to find SQLite extension functions. Default /../web/sqlite3_ext/extension-functions
 --stations <LIST> [optional] list of station IDs to send. Must be comma separated, no spaces. Default 1,2");
        }

        static void Main(string[] args)
        {
            TaskScheduler.UnobservedTaskException += TaskScheduler_UnobservedTaskException;
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;
            AppDomain.CurrentDomain.ProcessExit += CurrentDomain_ProcessExit;

            if (!ParseArgs(args))
            {
                Help();
                return;
            }

            PostStation( stationID: 48 + 5,
                Name: "Sherwin Ridge",
                lat: 37.607211, 
                lon: -118.98982,
                windheight: 7.5, 
                tempheight: 7.5,
                elevation: 3057).Wait();
            return;

            while (true)
            {
                TimerLoopAsync();
                if (_shutdownEvent.WaitOne(_sendInterval))
                    return;
            }
        }

        static async void TimerLoopAsync()
        {
            try
            {
                var csb = new SQLiteConnectionStringBuilder
                {
                    DataSource = _fn,
                    Pooling = false,
                    ForeignKeys = true,
                    DateTimeFormat = SQLiteDateFormats.UnixEpoch,
                    DateTimeKind = DateTimeKind.Utc
                };
                using SQLiteConnection conn = new SQLiteConnection(csb.ToString());
                conn.Open();

                bool extensionLoaded;
                try
                {
                    conn.EnableExtensions(true);
                    conn.LoadExtension(_extensionLocation);
                    extensionLoaded = true;
                    Console.WriteLine("Extension function loaded successfully.");
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"Unable to load extension functions: {ex}");
                    Console.Error.WriteLine($"Extension location: {_extensionLocation}");
                    extensionLoaded = false;
                }

                string avgWdStr = extensionLoaded ?
                    "(DEGREES(ATAN2(AVG(SIN(RADIANS(Wind_Direction))), AVG(COS(RADIANS(Wind_Direction))))) + 360) % 360"
                    :
                    "AVG(Wind_Direction)";

                using var cmd = new SQLiteCommand(
                    $@"
                    SELECT 
                        ID,
                        Name,
                        Lon,
                        Lat,
                        Elevation,

                        AVG(Windspeed) AS windspeed,
                        MAX(COALESCE(wind_gust, windspeed)) AS gust,
                        AVG(external_temp) AS temp,
                        {avgWdStr} as windDir
                    FROM
                        stations
                    JOIN
                        station_data ON station_data.station_id == stations.ID
                    WHERE 
                        @lastTime <= timestamp AND timestamp < @thisTime
                    AND
                        ID IN ({_stationIds.ToCsv()})
                    GROUP BY
                        ID
                    ",
                    conn);
                
                Dictionary<long, object> stations = new Dictionary<long, object>();
                List<object> readings = new List<object>();                

                DateTimeOffset thisTime = DateTimeOffset.Now;
                var startTs = (thisTime - _sendInterval).ToUnixTimeSeconds();
                cmd.Parameters.AddWithValue("@lastTime", startTs);
                cmd.Parameters.AddWithValue("@thisTime", thisTime.ToUnixTimeSeconds());
                using var reader = cmd.ExecuteReader();
                while (reader.Read())
                {
                    long id = reader.GetInt64(0);
                    if (reader.IsDBNull(5) || reader.IsDBNull(6) || reader.IsDBNull(7) || reader.IsDBNull(8))
                    {
                        Console.WriteLine($"Null values for {id}");
                        continue;
                    }

                    string name = reader.GetString(1);
                    foreach (char c in "() ")
                        name = name.Replace(c, '_');
                    double lon = reader.GetDouble(2);
                    double lat = reader.GetDouble(3);
                    double elevation = reader.GetDouble(4);
                    stations[id] = new { station = id, name, lon, lat, elevation, tempheight = 8, windheight = 8 };

                    double wind = reader.GetDouble(5) / 3.6;
                    double gust = reader.GetDouble(6) / 3.6;
                    double temp = reader.GetDouble(7);
                    double winddir = reader.GetDouble(8);
                    if (id == 50)
                        readings.Add(new { station = id, ts = thisTime.ToUnixTimeSeconds(), wind, gust, winddir });
                    else
                        readings.Add(new { station = id, ts = thisTime.ToUnixTimeSeconds(), wind, gust, temp, winddir });
                }

                await SendReadings(readings);
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Loop exception: {ex}");
            }
        }

        private static async Task PostStation(int stationID, string Name, double lat, double lon,
            double windheight, double tempheight, double elevation)
        {
            var obj = new
            {
                stations = new[] 
                { new 
                    {
                    station = stationID,
                    name = Name,
                    lat,
                    lon,
                    windheight, tempheight, elevation
                    } 
                }
            };
            var json = JsonConvert.SerializeObject(obj);
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var response = await _client.PostAsync(_url, content);
            Console.WriteLine($"Status Code: {response.StatusCode}");
            Console.WriteLine(await response.Content.ReadAsStringAsync());
        }

        private static async Task SendReadings(List<object> readings)
        {
            const int batchSize = 100;
            for (int batch = 0; batch < readings.Count; batch += batchSize)
            {
                string json = JsonConvert.SerializeObject(new
                {
                    //stations = stations.Values,
                    observations = readings.Skip(batch).Take(batchSize)
                }, Formatting.Indented);
                Console.WriteLine($"Posting: {json}");
                var content = new StringContent(json, Encoding.UTF8, "application/json");

                for (int i = 0; i < 3; i++)
                {
                    var response = await _client.PostAsync(_url, content);
                    if (response.IsSuccessStatusCode)
                    {
                        Console.WriteLine("Succesfully posted.");
                        Console.WriteLine($"Response: {response.Content.ReadAsStringAsync().Result}");
                        break;
                    }
                    else
                    {
                        Console.Error.WriteLine($"Post failed ({response.StatusCode}): {response.ReasonPhrase}");
                        Console.Error.WriteLine($"Content: {response.Content.ReadAsStringAsync().Result}");
                        Console.Error.WriteLine($"JSON: {json}");
                        await Task.Delay(30000);
                    }
                }

                await Task.Delay(5000);
            }
        }

        private static void CurrentDomain_ProcessExit(object sender, EventArgs e)
        {
            _shutdownEvent.Set();
        }

        private static void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
        }

        private static void TaskScheduler_UnobservedTaskException(object sender, UnobservedTaskExceptionEventArgs e)
        {
            Console.WriteLine(e.Exception);
            e.SetObserved();
        }

        public static string ToCsv<T>(this IEnumerable<T> source, Func<T, string> transformer = null, string separator = ", ")
        {
            if (transformer == null)
                transformer = A => A.ToString();
            return source?.Aggregate("", (run, cur) => run + (string.IsNullOrEmpty(run) ? "" : separator) + transformer(cur));
        }
    }
}
