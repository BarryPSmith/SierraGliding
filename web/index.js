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
app.use('/api/', router);


if (require.main === module) {
    if (!args.db || args.help) {
        return help();
    }

    database(path.resolve(__dirname, 'data.sqlite'), (err, db) => {
        if (err) throw err;

        return main(db);
    });
} else {
    module.exports = {
        main: main
    }
}

/**
 * Print server CLI help documentation to STDOUT
 *
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
 * @param {Function} cb Callback Function (err, db)
 */
function database(dbpath, cb) {
    const db = new sqlite3.Database(dbpath);

    db.serialize(() => {
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
 * Start the main Sierra Gliding webserver
 *
 * @param {Object} db Initialized Sqlite3 Database
 */
function main(db) {
    router.use((req, res, next) => {
        console.error(`[${req.method}] /api${req.url}`);
        return next();
    });

    /**
     * Returns basic metadata about all stations
     * managed in the database
     */
    router.get('/api/stations', (req, res) => {

    });

    router.post('/api/station', (req, res) => {

    });

    app.listen(args.port ? parseInt(args.port) : 4000, (err) => {
        if (err) throw err;

        console.error('Server listening http://localhost:4000');
    });
}
