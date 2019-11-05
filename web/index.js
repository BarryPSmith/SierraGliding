#!/usr/bin/env node

'use strict';

const stats = require('simple-statistics');
const moment = require('moment');
const turf = require('@turf/turf');
const WebSocketServer = require('ws').Server;
const express = require('express');
const sqlite3 = require('sqlite3');
const path = require('path');
const url = require('url');
const args = require('minimist')(process.argv, {
    string: ['db', 'port'],
    boolean: ['help']
});

const app = express();
const router = new express.Router();

app.disable('x-powered-by');

app.use(express.static(path.resolve(__dirname, 'site/dist')));
app.use(express.json());

app.use('/api/', router);

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
        db.run("PRAGMA foreign_keys = ON")
        db.run("PRAGMA journal_mode = WAL")
        db.run("PRAGMA busy_timeout = 1000");

        if (drop) {
            db.run(`
                DROP TABLE IF EXISTS stations;
            `);

            db.run(`
                DROP TABLE IF EXISTS station_data;
            `);

            db.run(`
                DROP TABLE IF EXISTS users;
            `);

            db.run(`
                DROP TABLE IF EXISTS tokens;
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
            CREATE TABLE IF NOT EXISTS stations (
                ID                  INTEGER PRIMARY KEY,
                Name                TEXT UNIQUE NOT NULL,
                Lon                 FLOAT NOT NULL,
                Lat                 FLOAT NOT NULL,
                Wind_Speed_Legend   TEXT NULL,
                Wind_Dir_Legend     TEXT NULL,
                Wind_Direction_Offset FLOAT NOT NULL DEFAULT 0, -- Used to compensate for misaligned stations. Applied when data is added to the station_data
                Display_Level       INTEGER NOT NULL DEFAULT 1, -- If we want to have some stations that are not usually displayed. 0 = Hidden, 1 = Public, 2 = Private (TODO: Figure out admin stuff)
				Status_Message		TEXT NULL --If we want to temporarily display a status message when e.g. a station has a hardware fault
            );
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS station_data (
                Station_ID          INT NOT NULL REFERENCES Stations(id) ON DELETE CASCADE ON UPDATE CASCADE,
                Timestamp           INT NOT NULL,
                Windspeed           FLOAT NOT NULL,
                Wind_Direction      FLOAT NOT NULL,
                Battery_Level       Float NULL,
                PRIMARY KEY(Station_ID, Timestamp)
            );
        `);

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
    res.status(500).json({
        status: 500,
        error: 'Internal Server Error'
    });
}

/**
 * Start the main Sierra Gliding webserver
 *
 * @param {Object} db Initialized Sqlite3 Database
 * @param {Function} cb Callback
 */
function main(db, cb) {
    router.use((req, res, next) => {
        console.error(`[${req.method}] /api${req.url}`);
        return next();
    });

    /**
     * Returns basic metadata about all stations
     * managed in the database as a GeoJSON FeatureCollection
     */
    router.get('/stations', (req, res) => {
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
                } catch(err) {
                    console.error(err);
                }
                try {
                    wdLegend = JSON.parse(station.winddirlegend);
                } catch(err) {
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
                //If we're given numbers, just use the default colours:
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
				Status_Message AS statusMessage
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
            } catch(err) {
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
                    error: 'sample url param must be integer value'
                });
            }
        }

        db.all(`
            SELECT
                Station_ID AS id,
                Timestamp AS timestamp,
                Windspeed AS windspeed,
                Wind_Direction AS wind_direction,
                COALESCE(Battery_Level, 0) AS battery_level
            FROM
                station_data
            WHERE
                ID = $id
                AND timestamp > $start
                AND timestamp < $end
            ORDER BY timestamp ASC
        `, {
            $id: req.params.id,
            $start: parseInt(req.query.start),
            $end: parseInt(req.query.end)
        }, (err, data_points) => {
            if (err) return error(err, res);

            if (!req.query.sample || !data_points.length) {
                return res.json(data_points);
            }

            const sampled = [];

            const bins = [];

            const min = data_points[0];
            const max = data_points[data_points.length - 1];

            for (
                let bin_lwr = max.timestamp - req.query.sample;
                bin_lwr >= min.timestamp;
                bin_lwr -= req.query.sample
            ) {
                const bin = [];

                if (!data_points.length) break;

                let pt = data_points.length - 1;

                while (
                    data_points[pt]
                    && data_points[pt].timestamp > bin_lwr
                    && data_points[pt].timestamp <= bin_lwr + req.query.sample
                ) {
                    bin.push(data_points.pop());

                    if (!data_points.length) break;

                    pt = data_points.length - 1;
                }

                if (!bin.length) continue;

                sampled.push({
                    timestamp: bin_lwr,
                    windspeed: stats.median(bin.map(b => b.windspeed)),
                    wind_direction: stats.median(bin.map(b => b.wind_direction)),
                    battery_level: stats.median(bin.map(b => b.battery_level))
                });
            }

            res.json(sampled.reverse());
        });
    });

    /**
     * Save a new data point to a a given station
     */
    router.post('/station/:id/data', (req, res) => {
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
            } catch(err) {
                return res.status(400).json({
                    status: 400,
                    error: 'timestamp must be an integer (unix) date'
                });
            }
        }

        db.get('SELECT Wind_Direction_Offset FROM stations WHERE id = $id'
            , { $id: req.params.id }
            , (err, data) =>
        {
            if (err) return error(err, res);
            if (!data)  return error(`Station ${req.params.id} not found.`, res);
            req.body.wind_direction = ((req.body.wind_direction + data.Wind_Direction_Offset) % 360 + 360) % 360;
            db.run(`
                INSERT INTO Station_Data (
                    Station_ID,
                    Timestamp,
                    Windspeed,
                    Wind_Direction,
                    Battery_Level
                ) VALUES (
                    $id,
                    $timestamp,
                    $windspeed,
                    $winddir,
                    $battery
                )
            `, {
                $id: req.params.id,
                $timestamp: moment.unix(req.body.timestamp).unix(),
                $windspeed: req.body.wind_speed,
                $winddir: req.body.wind_direction,
                $battery: req.body.battery ? req.body.battery : null
            }, (err) => {
                if (err) return error(err, res);

                res.json("success");
                        
                //Notify websockets that a particular station has updated:
                wss.clients.forEach((client) => {
                    client.send(JSON.stringify({
                        id: req.params.id,
                        timestamp: moment.unix(req.body.timestamp).unix(),
                        windspeed: req.body.wind_speed,
                        wind_direction: req.body.wind_direction,
                        battery_level: req.body.battery ? req.body.battery : null
                    }));
                });
            }); //exec insert
        }); //get wind_direction_offset
    }); //declare post /station/:id/data

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
            $id: req.params.id,
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
