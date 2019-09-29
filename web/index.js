const express = require('express');
const sqlite3 = require('sqlite3');
const fs = require('fs');
const path = require('path');

const app = express();
const router = new express.Router();

app.disable('x-powered-by');

app.use(express.static(path.resolve(__dirname, 'site/dist')));
app.use('/api/', router);

const db = new sqlite3.Database(path.resolve(__dirname, 'data.sqlite'));

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

    return main(db);
});

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

    app.listen(4000, (err) => {
        if (err) throw err;

        console.error('Server listening http://localhost:4000');
    });
}
