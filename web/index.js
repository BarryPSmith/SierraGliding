#!/usr/bin/env node

'use strict';

const moment = require('moment');
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
        }

        db.run(`
            CREATE TABLE IF NOT EXISTS stations (
                ID                  INTEGER PRIMARY KEY,
                Name                TEXT UNIQUE NOT NULL,
                Lon                 FLOAT NOT NULL,
                Lat                 FLOAT NOT NULL,
                Wind_Speed_Legend   TEXT NULL,
                Wind_Dir_Legend     TEXT NULL
            );
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS station_data (
                Station_ID          INT NOT NULL,
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

    //A list of all websockets, so we can send messages to them when we get new data.
    //Does this belong here? I don't know.
    let sockets = [];

    /**
     * Returns basic metadata about all stations
     * managed in the database
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
                stations;
        `, (err, stations) => {
            if (err) return error(err, res);

            stations = stations.map((station) => {
                station.windspeedlegend = JSON.parse(station.windspeedlegend);
                station.winddirlegend = JSON.parse(station.winddirlegend);

                return station;
            });

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

        // TODO check lengths of windspeedlegend/dir if they exist
        // Windspeed should be 3 values

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
                Wind_Dir_Legend AS winddirlegend
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

            try {
                moment(req.query.start);
                moment(req.query.end);
            } catch(err) {
                return res.status(400).json({
                    status: 400,
                    error: `start/end param mustb be date`
                });
            }
        }

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
                AND timestamp > $start
                AND timestamp < $end
            ORDER BY timestamp ASC
        `, {
            $id: req.params.id,
            $start: moment(req.query.start).unix(),
            $end: moment(req.query.end).unix()
        }, (err, data) => {
            if (err) return error(err, res);

            res.json(data);
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
                console.error(`${key} key required.`);
                console.error(req.body);
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

        db.all(`
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
        }, (err, data) => {
            if (err) return error(err, res);

            res.json(data);

            //Notify websockets that a particular station has updated:
            for (const socket of sockets) {
                socket.send({
                    id: req.params.id,
                    timestamp: moment.unix(req.body.timestamp).unix(),
                    wind_speed: req.body.wind_speed,
                    wind_direction: req.body.wind_direction,
                    battery: req.body.battery ? req.body.battery : null
                });
            }
        });
    });

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
    }).on('connection', (ws) => {
        sockets.push(ws);
    });
}
