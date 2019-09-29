#!/usr/bin/env node

'use strict';

const express = require('express');
const sqlite3 = require('sqlite3');
const path = require('path');
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
                ID                  INT,
                Name                TEXT,
                Lon                 FLOAT,
                Lat                 FLOAT,
                Wind_Speed_Legend   TEXT,
                Wind_Dir_Legend     TEXT
            );
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS station_data (
                Station_ID          INT,
                Timestamp           INT,
                Windspeed           FLOAT,
                Wind_Direction      FLOAT,
                Battery_Level       Float
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
     * managed in the database
     */
    router.get('/stations', (req, res) => {
        db.all(`
            SELECT
                ID AS id,
                Name AS name,
                Lon AS lon,
                Lat AS lat
            FROM
                stations;
        `, (err, stations) => {
            if (err) return error(err, res);
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
            if (!req.body[key]) {
                return res.status(400).json({
                    status: 400,
                    error: `${key} key required`
                });
            }
        }

        db.run(`
            INSERT INTO Stations (
                ID,
                Name,
                Lat,
                Lon
            ) VALUES (
                $id,
                $name,
                $lat,
                $lon
            )
        `, {
            $id: req.body.id,
            $name: req.body.name,
            $lat: req.body.lat,
            $lon: req.body.lon
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

    app.listen(args.port ? parseInt(args.port) : 4000, (err) => {
        if (err) throw err;

        console.error('Server listening http://localhost:4000');

        if (cb) return cb();
    });
}
