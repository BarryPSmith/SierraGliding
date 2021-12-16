#!/usr/bin/env node

const moment = require('moment');
const turf = require('@turf/turf');
const WebSocketServer = require('ws').Server;
const express = require('express');
const sqlite3 = require('sqlite3');
const path = require('path');
const util = require('util');
const args = require('minimist')(process.argv, {
    string: ['db', 'port'],
    boolean: ['help']
});

const extensionLoaded = true;

const app = express();
const router = new express.Router();

app.disable('x-powered-by');

app.use(express.static(path.resolve(__dirname, 'site/dist')));
// app.use('/all', express.static(path.resolve(__dirname, 'site/dist')));
app.use(express.json());
app.set('json replacer', standardReplacer);
app.use('/api/', router);
app.use('/[^\.]+', express.static(path.resolve(__dirname, 'site/dist')));

if (require.main === module) {
    if (!args.db || args.help) {
        return help();
    }

    database(path.resolve(args.db), false, (err, db) => {
        if (err) throw err;

        return main(db);
    });
} else {
    module.exports = {
        main: main,
        database: database,
        help: help
    };
}

/**
 * Print server CLI help documentation to STDOUT
 */
function help() {
    console.log('');
    console.log('  Start the Sierra Gliding Server');
    console.log('');
    console.log('  Usage ./index.js [--db <DB>] [--help]');
    console.log('');
    console.log('  Options:');
    console.log('  --db <DB.sqlite>     [required] Backend Database to Create or Use');
    console.log('  --port <PORT>        [optional] Choose an alternate port from the default 4000');
    console.log('  --help               [optional] Print this message to stdout');
    console.log('');
    process.exit();
}

/**
 * Instantiate a new Sqlite Database, creating schema is needed
 *
 * @param {String} dbpath Path to existing db or db to create
 * @param {Boolean} drop Should a new database be created
 * @param {Function} cb Callback Function (err, db)
 */
function database(dbpath, drop, cb) {
    const db = new sqlite3.Database(dbpath);

    db.serialize(() => {
        db.run('PRAGMA foreign_keys = ON');
        db.run('PRAGMA journal_mode = WAL');
        db.run('PRAGMA busy_timeout = 1000');

        db.loadExtension('sqlite3_ext/extension-functions', (err, res) => {
            if (err) {
                console.error(err);
                const extensionLoaded = false;
            }
        });

        if (drop) {
            db.run(`
                DROP TABLE IF EXISTS stations;
                DROP TABLE IF EXISTS station_data;
                DROP TABLE IF EXISTS station_data_100;
                DROP TABLE IF EXISTS station_data_600;
                DROP TABLE IF EXISTS users;
                DROP TABLE IF EXISTS tokens;
                DROP TABLE IF EXISTS station_groups;
            `);
        }

        db.run(`
            CREATE TABLE IF NOT EXISTS users (
                id                  SERIAL,
                username            TEXT PRIMARY KEY,
                password            TEXT NOT NULL
            )
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS tokens (
                uid                 INT NOT NULL REFERENCES users(id),
                token               TEXT PRIMARY KEY
            )
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS station_groups (
                ID INTEGER PRIMARY KEY,
                Name TEXT UNIQUE NOT NULL COLLATE NOCASE
            )
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS stations (
                ID                      INTEGER PRIMARY KEY,
                Name                    TEXT UNIQUE NOT NULL,
                Lon                     FLOAT NOT NULL,
                Lat                     FLOAT NOT NULL,
                Elevation               FLOAT NOT NULL,
                Wind_Speed_Legend       TEXT NULL,
                Wind_Dir_Legend         TEXT NULL,
                Wind_Direction_Offset   FLOAT NOT NULL DEFAULT 0, -- Used to compensate for misaligned stations. Applied when data is added to the station_data
                Display_Level           INTEGER NOT NULL DEFAULT 1, -- If we want to have some stations that are not usually displayed. 0 = Hidden, 1 = Public, 2 = Private (TODO: Figure out admin stuff)
                Status_Message          TEXT NULL, --If we want to temporarily display a status message when e.g. a station has a hardware fault
                Battery_Range           TEXT NULL,
                Group_ID                INTEGER NULL REFERENCES station_groups ON DELETE SET NULL ON UPDATE CASCADE
            );
        `);

        for (const postFix of ['', '_100', '_600']) {
            db.run(`
                CREATE TABLE IF NOT EXISTS station_data${postFix} (
                    Station_ID          INT NOT NULL REFERENCES Stations(id) ON DELETE CASCADE ON UPDATE CASCADE,
                    Timestamp           INT NOT NULL,
                    Windspeed           FLOAT NOT NULL,
                    Wind_Direction      FLOAT NOT NULL,
                    Battery_Level       Float NULL,
                    Internal_Temp       FLOAT NULL,
                    External_Temp       FLOAT NULL,
                    Wind_Gust           FLOAT NULL,
                    Pwm                 INT NULL,
                    Current             INT NULL,
                    PRIMARY KEY(Station_ID, Timestamp)
                );
            `);
        }

        for (const interval of ['100', '600']) {
            db.run(`CREATE TRIGGER IF NOT EXISTS
                    Trg_Station_Data_${interval}
                    AFTER INSERT ON Station_Data
                    BEGIN
                      INSERT OR REPLACE INTO Station_Data_${interval}
                        (Station_ID, Timestamp, Windspeed,
                        Wind_Direction, Battery_Level,
                        Internal_Temp, External_Temp, Wind_Gust, Pwm, Current)
                      SELECT
                        NEW.Station_ID,
                        CEIL(NEW.Timestamp/${interval}.0) * ${interval} AS Timestamp,
                        AVG(windspeed) AS Windspeed,
                        (DEGREES(ATAN2(AVG(SIN(RADIANS(Wind_Direction))), AVG(COS(RADIANS(Wind_Direction))))) + 360) % 360 AS Wind_Direction,
                        AVG(Battery_Level) AS Battery_Level,
                        AVG(Internal_Temp) AS Internal_Temp,
                        AVG(External_Temp) AS External_Temp,
                        MAX(Windspeed) AS Wind_Gust,
                        AVG(Pwm) AS Pwm,
                        AVG(Current) AS Current
                      FROM
                        Station_Data
                      WHERE
                        (CEIL(NEW.Timestamp/${interval}.0) - 1) * ${interval} < Timestamp
                        AND Timestamp <= CEIL(NEW.Timestamp/${interval}.0) * ${interval}
                        AND Station_ID = NEW.Station_ID;
                    END;
            `);

            db.run(`CREATE TEMP TRIGGER IF NOT EXISTS 
                    Trg_Station_Data_Del_${interval} 
                    AFTER DELETE ON main.Station_Data
                    BEGIN
                        INSERT OR REPLACE INTO Station_Data_${interval}
                            (Station_ID, Timestamp, Windspeed, 
                            Wind_Direction, Battery_Level,
                            Internal_Temp, External_Temp, Wind_Gust, Pwm, Current)
                        SELECT
                            OLD.Station_ID,
                            CEIL(OLD.Timestamp/${interval}.0) * ${interval} AS Timestamp,
                            AVG(windspeed) AS Windspeed,
                            (DEGREES(ATAN2(AVG(SIN(RADIANS(Wind_Direction))), AVG(COS(RADIANS(Wind_Direction))))) + 360) % 360 AS Wind_Direction,
                            AVG(Battery_Level) AS Battery_Level,
                            AVG(Internal_Temp) AS Internal_Temp,
                            AVG(External_Temp) AS External_Temp,
                            MAX(Windspeed) AS Wind_Gust,
                            AVG(Pwm) AS Pwm,
                            AVG(Current) AS Current
                        FROM
                            Station_Data
                        WHERE 
                            (CEIL(OLD.Timestamp/${interval}.0) - 1) * ${interval} < Timestamp
                            AND Timestamp <= CEIL(OLD.Timestamp/${interval}.0) * ${interval}
                            AND Station_ID = OLD.Station_ID;

                        DELETE FROM 
                            Station_Data_${interval}
                        WHERE 
                            Station_ID = OLD.Station_ID
                            AND Timestamp = CEIL(OLD.Timestamp/${interval}.0) * ${interval}
                            AND NOT EXISTS (
                                SELECT 1
                                FROM Station_Data
                                WHERE                         
                                    (CEIL(OLD.Timestamp/${interval}.0) - 1) * ${interval} < Timestamp
                                    AND Timestamp <= CEIL(OLD.Timestamp/${interval}.0) * ${interval}
                                    AND Station_ID = OLD.Station_ID
                                );
                    END;`
            );
        }

        db.run(`
            CREATE TABLE IF NOT EXISTS discarded_data
            AS SELECT * FROM station_data LIMIT 0;
        `);

        db.get('SELECT sqlite_version() AS ver', (err, res) => {
            if (err) console.error(err);
            else (console.error('Sqlite3 Version: ' + res.ver));
        });

        return cb(null, db);
    });
}

/**
 * Standard error logging and internal error response
 *
 * @param {Error} err Error to log and respond to
 * @param {Object} res express result object
 */
function error(err, res) {
    console.error(err);
    if (err.code == 'SQLITE_CONSTRAINT') {
        res.status(422).json({
            status: 422,
            error: err.message
        });
    } else {
        res.status(500).json({
            status: 500,
            error: 'Internal Server Error'
        });
    }
}


const defaultStatSize = 600;

/**
 * Start the main Sierra Gliding webserver
 *
 * @param {Object} db Initialized Sqlite3 Database
 * @param {Function} cb Callback
 */
function main(db, cb) {
    const dbGet = util.promisify(sqlite3.Database.prototype.get).bind(db);
    const dbRun = util.promisify(sqlite3.Database.prototype.run).bind(db);


    router.use((req, res, next) => {
        console.error(`[${req.method}] /api${req.url}`);
        return next();
    });


    /**
     * Returns basic metadata about all stations
     * managed in the database as a GeoJSON FeatureCollection
     */
    router.get('/stationFeatures', (req, res) => {
        db.all(`
            SELECT
                ID AS id,
                Name AS name,
                Lon AS lon,
                Lat AS lat,
                Wind_Speed_Legend AS windspeedlegend,
                Wind_Dir_Legend AS winddirlegend
            FROM
                stations
            WHERE
                id >= 0;
        `, (err, stations) => {
            if (err) return error(err, res);

            const pts = [];

            stations = turf.featureCollection(stations.map((station) => {
                let wsLegend = null;
                let wdLegend = null;
                try {
                    wsLegend = JSON.parse(station.windspeedlegend);
                } catch (err) {
                    console.error(err);
                }
                try {
                    wdLegend = JSON.parse(station.winddirlegend);
                } catch (err) {
                    console.error(err);
                }

                pts.push([station.lon, station.lat]);

                return turf.point([station.lon, station.lat], {
                    name: station.name,
                    legend: {
                        wind_speed: wsLegend,
                        wind_dir: wdLegend
                    }
                },{
                    id: station.id,
                    bbox: turf.bbox(turf.buffer(turf.point([station.lon, station.lat]), 0.5))
                });
            }));

            if (pts.length) {
                stations.bbox = turf.bbox(turf.buffer(turf.multiPoint(pts), 5));
            }

            res.json(stations);
        });
    });

    router.get('/stations', (req, res) => {
        db.all(`
            SELECT
                ID AS id,
                Name AS name,
                Wind_Speed_Legend,
                Wind_Dir_Legend,
                Battery_Range
            FROM
                stations
            WHERE
                id >= 0
                AND (
                        (Group_ID IS (SELECT ID FROM station_groups WHERE Name = $groupName)
                        AND
                        Display_Level <= 1)
                    OR
                    1 = $showAll)
        `,
        {
            $specificId: req.query.stationID,
            $showAll: req.query.all === 'true' ? 1 : 0,
            $groupName: req.query.groupName
        }
        , (err, stations) => {
            if (err) return error(err, res);

            for (const idx in stations) {
                try {
                    stations[idx].Wind_Speed_Legend = JSON.parse(stations[idx].Wind_Speed_Legend);
                } catch (err) {
                    console.error(err);
                }
                try {
                    stations[idx].Wind_Dir_Legend = JSON.parse(stations[idx].Wind_Dir_Legend);
                } catch (err) {
                    console.error(err);
                }
                try {
                    stations[idx].Battery_Range = JSON.parse(stations[idx].Battery_Range);
                } catch (err) {
                    console.error(err);
                }

            }

            res.json(stations);
        });
    });

    /**
     * Create a new station
     */
    router.post('/station', (req, res) => {
        if (!req.body) {
            return res.status(400).json({
                status: 400,
                error: 'request body required'
            });
        }

        for (const key of ['id', 'name', 'lon', 'lat']) {
            if (req.body[key] === undefined) {
                return res.status(400).json({
                    status: 400,
                    error: `${key} key required`
                });
            }
        }

        for (const key of ['windspeedlegend', 'winddirlegend']) {
            if (req.body[key] && !Array.isArray(req.body[key])) {
                return res.status(400).json({
                    status: 400,
                    error: `${key} must be an Array`
                });
            }
        }

        if (req.body.windspeedlegend) {
            let lastVal = 0;
            const colors = ['Blue', 'Green', 'Yellow', 'Red'];
            for (const idx in req.body.windspeedlegend) {
                // If we're given numbers, just use the default colours:
                if (!isNaN(parseFloat(req.body.windspeedlegend[idx]))) {
                    if (idx > 3) {
                        return res.status(400).json({
                            status:400,
                            error:'windspeedlegend must be less than or equal 4 elements if color is not supplied.'
                        });
                    }
                    req.body.windspeedlegend[idx] = {
                        top: parseFloat(req.body.windspeedlegend[idx]),
                        color: colors[i]
                    };
                }
                const entry = req.body.windspeedlegend[idx];
                if (entry.top === undefined
                    || entry.color === undefined) {
                    return res.status(400).json({
                        status: 400,
                        error: 'Each Wind Speed Legend entry must have "top" and "color" defined.'
                    });
                }
                if (entry.top <= lastVal) {
                    return res.status(400).json({
                        status: 400,
                        error: 'Wind Speed Legend must go from low -> high values'
                    });
                }
                else {
                    lastVal = entry.top;
                }
            }
        }

        // TODO check lengths of windspeed/dir if they exist
        // winddir should be array of array of 2 length values

        db.run(`
            INSERT INTO Stations (
                ID,
                Name,
                Lat,
                Lon,
                Wind_Speed_Legend,
                Wind_Dir_Legend
            ) VALUES (
                $id,
                $name,
                $lat,
                $lon,
                $windspeed,
                $winddir
            )
        `, {
            $id: req.body.id,
            $name: req.body.name,
            $lat: req.body.lat,
            $lon: req.body.lon,
            $windspeed: JSON.stringify(req.body.windspeedlegend ? req.body.windspeedlegend : []),
            $winddir: JSON.stringify(req.body.winddirlegend ? req.body.winddirlegend : [])
        }, (err) => {
            if (err) return error(err, res);

            res.status(200).json(true);
        });
    });

    /**
     * Get all information about a single station
     */
    router.get('/station/:id', (req, res) => {
        db.all(`
            SELECT
                ID AS id,
                Name AS name,
                Lat AS lat,
                Lon AS lon,
                Wind_Speed_Legend AS windspeedlegend,
                Wind_Dir_Legend AS winddirlegend,
                Status_Message AS statusMessage,
                Battery_Range as batteryRange
            FROM
                stations
            WHERE
                ID = $id
        `, {
            $id: req.params.id
        }, (err, station) => {
            if (err) return error(err, res);

            if (!station.length) {
                return res.status(404).json({
                    status: 404,
                    error: 'No station by that ID found'
                });
            }

            station = station[0];

            station.windspeedlegend = JSON.parse(station.windspeedlegend);
            station.winddirlegend = JSON.parse(station.winddirlegend);
            station.batteryRange = JSON.parse(station.batteryRange);

            res.json(station);
        });
    });

    /**
     * Get data from a given station for a given time period
     */
    router.get('/station/:id/data', (req, res) => {
        for (const key of ['start', 'end']) {
            if (!req.query[key]) {
                return res.status(400).json({
                    status: 400,
                    error: `${key} url param required`
                });
            }

            if (isNaN(parseInt(req.query[key]))) {
                return res.status(400).json({
                    status: 400,
                    error: `${key} url param required`
                });
            } else {
                req.query[key] = parseInt(req.query[key]);
            }

            try {
                moment.unix(req.query[key]);
            } catch (err) {
                return res.status(400).json({
                    status: 400,
                    error: `${key} url param must be integer of seconds since unix epoch`
                });
            }
        }

        if (req.query.sample) {
            req.query.sample = parseInt(req.query.sample);

            if (isNaN(req.query.sample)) {
                return res.status(400).json({
                    status: 400,
                    error: 'sample url param must be numeric value'
                });
            }
        }

        const start = req.query.start;
        const end = req.query.end;
        const expectedSamples = req.query.sample || 4;
        const maxSamples = 100000;
        if ((end - start) /expectedSamples > maxSamples) {
            return res.status(413).json({
                status: 413,
                error: `exceeds maximum expected sample count of ${maxSamples}`
            });
        }

        const avg_wd_String = extensionLoaded ?
            '(DEGREES(ATAN2(AVG(SIN(RADIANS(Wind_Direction))), AVG(COS(RADIANS(Wind_Direction))))) + 360) % 360'
            :
            'AVG(Wind_Direction)';
        const avg_wd_windowed = extensionLoaded ?
            '(DEGREES(ATAN2(AVG(SIN(RADIANS(Wind_Direction))) OVER stat_wind, AVG(COS(RADIANS(Wind_Direction))) OVER stat_wind)) + 360) % 360'
            :
            'AVG(Wind_Direction) OVER stat_wind';

        let stat_len = parseFloat(req.query.stat_len);
        stat_len = Number.isNaN(stat_len) ? defaultStatSize : stat_len;
        const dataSource = req.query.sample < 600 ? req.query.sample < 100 ? 'Station_Data' : 'Station_Data_100' : 'Station_Data_600';
        const gustCol = req.query.sample < 100 ? 'Windspeed' : 'Wind_Gust';
        db.all(`
            SELECT
                $id AS ID,
                timestamp,
                windspeed,
                AVG(windspeed) OVER stat_wind AS windspeed_avg,
                MAX(windspeed_sample_max) OVER stat_wind AS windspeed_max,
                MIN(windspeed_sample_min) OVER stat_wind AS windspeed_min,
                wind_direction,
                ${avg_wd_windowed} AS wind_direction_avg,
                battery_level,
                internal_temp,
                external_temp
                FROM (
                    SELECT
                        timestamp,
                        AVG(Windspeed) as windspeed,
                        MIN(Windspeed) as windspeed_sample_min,
                        MAX(${gustCol}) as windspeed_sample_max,
                        ` + avg_wd_String + ` AS wind_direction,
                        AVG(Battery_Level) AS battery_level,
                        AVG(Internal_Temp) as internal_temp,
                        AVG(External_Temp) as external_temp
                    FROM
                        ${dataSource}
                    WHERE
                        Station_ID = $id
                        AND timestamp > $start - $stat_len
                        AND timestamp < $end
                    GROUP BY ROUND(Timestamp / $sample)
                )
                WINDOW stat_wind AS (ORDER BY Timestamp ASC RANGE $stat_len PRECEDING)
                ORDER BY Timestamp ASC
        `, {
            $id: req.params.id,
            $start: start,
            $end: end,
            $sample: parseFloat(req.query.sample),
            $stat_len: stat_len
        }, (err, data_points) => {
            if (err) return error(err, res);
            return res.json(data_points.filter((p) => p.timestamp > start));
        });
    });

    /**
     * Save a new data point to a a given station
     */
    router.post('/station/:id/data', async (req, res) => {
        if (!req.body) {
            return res.status(400).json({
                status: 400,
                error: 'request body required'
            });
        }

        for (const key of ['timestamp', 'wind_speed', 'wind_direction']) {
            if (req.body[key] === undefined) {
                return res.status(400).json({
                    status: 400,
                    error: `${key} key required`
                });
            }

            try {
                moment.unix(req.body.timestamp);
            } catch (err) {
                return res.status(400).json({
                    status: 400,
                    error: 'timestamp must be an integer (unix) date'
                });
            }
        }

        try {
            const wsOffset = await (dbGet('SELECT Wind_Direction_Offset FROM stations WHERE id = $id'
                , { $id: req.params.id }))
            if (!wsOffset) {
                return res.status(404).json({
                    status:404,
                    error:`Station ${req.params.id} not found.`
                });
            }

            const invalidWindspeed = typeof(req.body.wind_gust) == 'number' && req.body.wind_speed > 2 * (req.body.wind_gust + 2)
            const dest = invalidWindspeed ? 'Discarded_Data' : 'Station_Data';

            req.body.wind_direction = ((req.body.wind_direction + wsOffset.Wind_Direction_Offset) % 360 + 360) % 360;
            await dbRun(`
                INSERT INTO ${dest} (
                    Station_ID,
                    Timestamp,
                    Windspeed,
                    Wind_Direction,
                    Battery_Level,
                    Internal_Temp,
                    External_Temp,
                    Wind_Gust,
                    Pwm,
                    Current
                ) VALUES (
                    $id,
                    $timestamp,
                    $windspeed,
                    $winddir,
                    $battery,
                    $internal_temp,
                    $external_temp,
                    $wind_gust,
                    $pwm,
                    $current
                )
            `, {
                $id: req.params.id,
                $timestamp: moment.unix(req.body.timestamp).unix(),
                $windspeed: req.body.wind_speed,
                $winddir: req.body.wind_direction,
                $battery: typeof(req.body.battery) == 'number' ? req.body.battery : null,
                $internal_temp: typeof(req.body.internal_temp) == 'number' ? req.body.internal_temp : null,
                $external_temp: typeof(req.body.external_temp) == 'number' ? req.body.external_temp : null,
                $wind_gust: typeof(req.body.wind_gust) == 'number' ? req.body.wind_gust : req.body.wind_speed,
                $pwm: typeof(req.body.pwm) == 'number' ? req.body.pwm : null,
                $current: typeof(req.body.current) == 'number' ? req.body.current : null
            });

            if (invalidWindspeed) {
                return res.status(422).json({
                    status: 422,
                    error: 'Invalid windspeed stronger than gust'
                });
            }
            res.json("success");
        } catch (err) {
            return error(err, res);
        }

        try {
            const avg_wd_String = extensionLoaded ?
                '(DEGREES(ATAN2(AVG(SIN(RADIANS(Wind_Direction))), AVG(COS(RADIANS(Wind_Direction))))) + 360) % 360'
                :
                'AVG(Wind_Direction)';
            const stats = await dbGet(`
                SELECT
                    MIN(windspeed) as windspeed_min,
                    MAX(windspeed) as windspeed_max,
                    AVG(windspeed) as windspeed_avg,
                    ${avg_wd_String} as wind_direction_avg
                FROM station_data
                WHERE
                    Station_ID = $id
                    AND $statStart <= Timestamp
                    AND Timestamp <= $statEnd`,
            {
                $id: req.params.id,
                $statStart: req.body.timestamp - defaultStatSize,
                $statEnd: req.body.timestamp
            });

            await checkCullInvalidWindspeed(db, req.params.id, req.body.wind_speed, req.body.timestamp, wss);

            wss.clients.forEach((client) => {
                client.send(JSON.stringify({
                    op: 'Add',
                    id: req.params.id,
                    timestamp: moment.unix(req.body.timestamp).unix(),
                    windspeed: req.body.wind_speed,
                    windspeed_avg: stats.windspeed_avg,
                    windspeed_max: stats.windspeed_max,
                    windspeed_min: stats.windspeed_min,
                    wind_direction: req.body.wind_direction,
                    wind_direction_avg: stats.wind_direction_avg,
                    battery_level: req.body.battery,
                    internal_temp: req.body.internal_temp,
                    external_temp: req.body.external_temp
                }, standardReplacer));
            });
        } catch (err) {
            console.error(err);
        }
    }); // declare post /station/:id/data

    /**
     * Return the latest datapoint for a given station
     */
    router.get('/station/:id/data/latest', (req, res) => {
        db.all(`
            SELECT
                Station_ID AS id,
                Timestamp AS timestamp,
                Windspeed AS windspeed,
                Wind_Direction AS wind_direction,
                Battery_Level AS battery_level
            FROM
                station_data
            WHERE
                ID = $id
            ORDER BY
                timestamp DESC
            LIMIT 1

        `, {
            $id: req.params.id
        }, (err, data) => {
            if (err) return error(err, res);

            res.json(data);
        });
    });

    if (!args.port) args.port = 4000;

    const server = app.listen(args.port, (err) => {
        if (err) throw err;

        console.error(`Server listening http://localhost:${args.port}`);

        if (cb) return cb();
    });

    const wss = new WebSocketServer({
        server: server
    });
}

async function checkCullInvalidWindspeed(db, id, windspeed, timestamp, wss) {
    const dbAll = util.promisify(sqlite3.Database.prototype.all).bind(db);
    const dbRun = util.promisify(sqlite3.Database.prototype.run).bind(db);
    const dbGet = util.promisify(sqlite3.Database.prototype.get).bind(db);

    const lastRecords = await dbAll(`
        SELECT timestamp timestamp, windspeed windspeed
        FROM station_data
        WHERE station_id = $id
        AND timestamp < $timestamp
        ORDER BY timestamp DESC
        LIMIT 2`,
    {
        $id: id,
        $timestamp: timestamp
    });
    if (lastRecords.length < 2) {
        return;
    }

    const testSpeed = lastRecords[0].windspeed;

    const closeToPrevious = lastRecords[1].timestamp > lastRecords[0].timestamp - 150;
    const closeToCurrent = lastRecords[0].timestamp > timestamp - 150;
    const distantFromBoth = !closeToPrevious && !closeToCurrent;

    //Allow a certain amount of variance:
    if (testSpeed < 12)
        return;
    
    if (closeToCurrent) {
        const threshold = windspeed * 4;
        if (testSpeed < threshold)
            return;
    }
    if (closeToPrevious) {
        const threshold = lastRecords[1].windspeed * 4;
        if (testSpeed < threshold)
            return;
        const stats = await dbGet(`
            SELECT
                MIN(windspeed) as windspeed_min,
                MAX(windspeed) as windspeed_max,
                AVG(windspeed) as windspeed_avg
            FROM station_data
            WHERE
                Station_ID = $id
                AND $statStart <= Timestamp
                AND Timestamp <= $statEnd`,
            {
                $id: id,
                $statStart: lastRecords[1].timestamp - defaultStatSize,
                $statEnd: lastRecords[1].timestamp
            });
        const threshold2 = stats.windspeed_avg * 4;
        const threshold3 = stats.windspeed_max;
        if (testSpeed < threshold2 || testSpeed < threshold3)
            return;
    }
    if (distantFromBoth) {
        const threshold = Math.max(lastRecords[1].windspeed, windspeed, 7) * 4 + 10;
        if (testSpeed < threshold) {
            return;
        }
    }
    
    const tsToDelete = lastRecords[0].timestamp;
    await dbRun('INSERT INTO discarded_data SELECT * FROM station_data WHERE station_id = $id AND timestamp = $tsToDelete;',
        {
            $id: id,
            $tsToDelete: tsToDelete
        });
    await dbRun('DELETE FROM station_data WHERE station_id = $id AND timestamp = $tsToDelete;',
        {
            $id: id,
            $tsToDelete: tsToDelete
        });
    wss.clients.forEach((client) => {
        client.send(JSON.stringify({
            id: id,
            timestamp: tsToDelete,
            op: 'Remove'
        }));
    });
}

function standardReplacer(key, val)
{
    // This remove null entries
    if (val === null) {
        return undefined;
    }
    if (typeof (val) == 'number') {
        if (key.startsWith('battery')) {
            return +val.toFixed(2);
        } else if (key.startsWith('wind') || key.endsWith('temp')) {
            return +val.toFixed(1);
        } else {
            return +val.toFixed(4);
        }
    }
    return val;
}
