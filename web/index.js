#!usr/bin/env node

import Err from '@openaddresses/batch-error';
import moment from 'moment';
import morgan from 'morgan';
import turf from '@turf/turf';
import { WebSocketServer } from 'ws';
import express from 'express';
import sqlite3 from 'sqlite3';
import path from 'path';
import util from 'util';
import minimist from 'minimist';
import database from './lib/database.js';

const args = minimist(process.argv, {
    string: ['db', 'port'],
    boolean: ['help']
});

const extensionLoaded = true;

const app = express();
const router = new express.Router();

app.disable('x-powered-by');

app.use(express.static(new URL('./site/dist', import.meta.url).pathname));

app.use(express.json());
app.set('json replacer', standardReplacer);
app.use('/api/', router);

if (import.meta.url === `file://${process.argv[1]}`) {
    if (!args.db || args.help) {
        help();
    } else {
        database(path.resolve(args.db), false, (err, db) => {
            if (err) throw err;

            return main(db);
        });
    }
}

export { main, database, help };

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

const defaultStatSize = 600;

/**
 * Start the main Sierra Gliding webserver
 *
 * @param {Object} db Initialized Sqlite3 Database
 * @param {Function} cb Callback
 */
function main(db, cb) {
    const dbAll = util.promisify(sqlite3.Database.prototype.all).bind(db);
    const dbGet = util.promisify(sqlite3.Database.prototype.get).bind(db);
    const dbRun = util.promisify(sqlite3.Database.prototype.run).bind(db);


    // Logging Middleware
    router.use(morgan('combined'));

    const getAllStations = async (req) => {
        const stations = await dbAll(`
            SELECT
                ID AS id,
                Name AS name,
                Wind_Speed_Legend,
                Wind_Dir_Legend,
                Battery_Range,
                Missing_Features,
                Lat as lat,
                Lon as lon,
                Elevation as elevation
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
        `, {
            $specificId: req.query.stationID,
            $showAll: req.query.all === 'true' ? 1 : 0,
            $groupName: req.query.groupName
        });

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

        return stations;
    };

    /**
     * Returns basic metadata about all stations
     * managed in the database as a GeoJSON FeatureCollection
     */
    router.get('/stationFeatures', async (req, res) => {
        try {
            const stations = await getAllStations(req);
            const pts = stations.map((station) => [station.lon, station.lat]);

            const stationFeatures = turf.featureCollection(stations.map((station) => {
                return turf.point([station.lon, station.lat], {
                    name: station.name,
                    station: station
                },{
                    id: station.id,
                    bbox: turf.bbox(turf.buffer(turf.point([station.lon, station.lat]), 0.5))
                });
            }));

            if (pts.length) {
                stationFeatures.bbox = turf.bbox(turf.buffer(turf.multiPoint(pts), 5));
            }

            res.json(stationFeatures);
        } catch (err) {
            return Err.respond(err, res);
        }
    });

    router.get('/stations', async (req, res) => {
        try {
            const stations = await getAllStations(req);
            res.json(stations);
        } catch (err) {
            return Err.respond(err, res);
        }
    });

    /**
     * Create a new station
     */
    router.post('/station', async (req, res) => {
        try {
            if (!req.body) throw new Err(400, null, 'request body required');

            for (const key of ['id', 'name', 'lon', 'lat']) {
                if (req.body[key] === undefined) {
                    throw new Err(400, null, `${key} key required`);
                }
            }

            for (const key of ['windspeedlegend', 'winddirlegend']) {
                if (req.body[key] && !Array.isArray(req.body[key])) {
                    throw new Err(400, null, `${key} must be an Array`);
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
            await dbRun(`
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
            });

            return res.status(200).json(true);
        } catch (err) {
            return Err.respond(err, res);
        }
    });

    /**
     * Get all information about a single station
     */
    router.get('/station/:id', async (req, res) => {
        try {
            let station = await dbAll(`
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
            });

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
        } catch (err) {
            return Err.respond(err, res);
        }
    });

    /**
     * Get data from a given station for a given time period
     */
    router.get('/station/:id/data', async (req, res) => {
        try {
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
            if ((end - start) / expectedSamples > maxSamples) {
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
            const data_points = await dbAll(`
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
                    external_temp,
                    current,
                    pwm
                    FROM (
                        SELECT
                            timestamp,
                            AVG(Windspeed) as windspeed,
                            MIN(Windspeed) as windspeed_sample_min,
                            MAX(${gustCol}) as windspeed_sample_max,
                            ` + avg_wd_String + ` AS wind_direction,
                            AVG(Battery_Level) AS battery_level,
                            AVG(Internal_Temp) as internal_temp,
                            AVG(External_Temp) as external_temp,
                            AVG(Current) as current,
                            AVG(PWM) as pwm
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
            });

            return res.json(data_points.filter((p) => p.timestamp > start));
        } catch (err) {
            return Err.respond(err, res);
        }
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
                , { $id: req.params.id }));
            if (!wsOffset) {
                return res.status(404).json({
                    status:404,
                    error:`Station ${req.params.id} not found.`
                });
            }

            const invalidWindspeed = typeof(req.body.wind_gust) == 'number' && req.body.wind_speed > 2 * (req.body.wind_gust + 2);
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
            res.json('success');
        } catch (err) {
            return Err.respond(err, res);
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
                    external_temp: req.body.external_temp,
                    current: req.body.current,
                    pwm: req.body.pwm
                }, standardReplacer));
            });
        } catch (err) {
            return Err.respond(err, res);
        }
    }); // declare post /station/:id/data

    /**
     * Return the latest datapoint for a given station
     */
    router.get('/station/:id/data/latest', async (req, res) => {
        try {
            const data = await dbAll(`
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
            });

            res.json(data);
        } catch (err) {
            return Err.respond(err, res);
        }
    });

    router.get('/groups', async (req, res) => {
        try {
            const ret = await dbAll(`
                SELECT
                    id, name FROM Station_Groups
                WHERE
                    $Name IS NULL
                    OR Name = $Name
            `,{
                $Name: req.query.name
            });

            res.json(ret);
        } catch (err) {
            return Err.respond(err, res);
        }
    });

    app.get('/:group', async (req, res) => {
        try {
            const stationThere = await dbGet(`
                SELECT
                    1
                FROM
                    Station_Groups
                WHERE
                    Name = $name
            `, {
                $name: req.params.group
            });

            if (!!stationThere || req.params.group === 'all') {
                const fn = new URL('./site/dist/index.html', import.meta.url).pathname;
                return res.sendFile(fn);
            } else {
                throw new Err(404, null, 'Group Not Found');
            }
        } catch (err) {
            return Err.respond(err, res);
        }
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
        SELECT
            timestamp timestamp, windspeed windspeed
        FROM
            station_data
        WHERE
            station_id = $id
            AND timestamp < $timestamp
        ORDER BY
            timestamp DESC
        LIMIT
            2
    `, {
        $id: id,
        $timestamp: timestamp
    });

    if (lastRecords.length < 2) return;

    const testSpeed = lastRecords[0].windspeed;

    const closeToPrevious = lastRecords[1].timestamp > lastRecords[0].timestamp - 150;
    const closeToCurrent = lastRecords[0].timestamp > timestamp - 150;
    const distantFromBoth = !closeToPrevious && !closeToCurrent;

    // Allow a certain amount of variance:
    if (testSpeed < 12) return;

    if (closeToCurrent) {
        const threshold = windspeed * 4;
        if (testSpeed < threshold) return;
    }

    if (closeToPrevious) {
        const threshold = lastRecords[1].windspeed * 4;
        if (testSpeed < threshold) return;

        const stats = await dbGet(`
            SELECT
                MIN(windspeed) as windspeed_min,
                MAX(windspeed) as windspeed_max,
                AVG(windspeed) as windspeed_avg
            FROM
                station_data
            WHERE
                Station_ID = $id
                AND $statStart <= Timestamp
                AND Timestamp <= $statEnd
        `,{
            $id: id,
            $statStart: lastRecords[1].timestamp - defaultStatSize,
            $statEnd: lastRecords[1].timestamp
        });
        const threshold2 = stats.windspeed_avg * 4;
        const threshold3 = stats.windspeed_max;
        if (testSpeed < threshold2 || testSpeed < threshold3) return;
    }

    if (distantFromBoth) {
        const threshold = Math.max(lastRecords[1].windspeed, windspeed, 7) * 4 + 10;
        if (testSpeed < threshold) return;
    }

    const tsToDelete = lastRecords[0].timestamp;
    await dbRun('INSERT INTO discarded_data SELECT * FROM station_data WHERE station_id = $id AND timestamp = $tsToDelete;', {
        $id: id,
        $tsToDelete: tsToDelete
    });

    await dbRun('DELETE FROM station_data WHERE station_id = $id AND timestamp = $tsToDelete;', {
        $id: id,
        $tsToDelete: tsToDelete
    });

    wss.clients.forEach((client) => {
        client.send(JSON.stringify({
            id,
            timestamp: tsToDelete,
            op: 'Remove'
        }));
    });
}

function standardReplacer(key, val) {
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
