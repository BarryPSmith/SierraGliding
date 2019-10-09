#! /usr/bin/env node
'use strict';

const OS = require('os');
const path = require('path');
const test = require('tape');
const srv = require('../index');
const request = require('request');
const moment = require('moment');
const args = require('minimist')(process.argv, {
    boolean: ['help', 'unit', 'mock']
});

// Tick count is the number of 4 second periods
// starting from 24 hours from when the server was
// started. This value is used as a seed to generate
// fake data to post to the server so that all
// data values are not identical.
let tick = 60 * 60 / 4 * -1;

if ((!args.unit && !args.mock) || args.help) {
    console.log();
    console.log('  Run unit tests or create a mock server instances');
    console.log();
    console.log('  Usage: test/server.test.js [--mock] [--test] [--help]');
    console.log()
    console.log('  Options:');
    console.log('  --mock       Start a mock server, feeding it fake data to test UI changes');
    console.log('  --unit       Run unit tests against the server');
    console.log('  --help       Display this help message');
    console.log();
    process.exit();
}

test('Stations', (t) => {
    t.test('Stations - Server', (q) => {
        srv.database(path.resolve(OS.tmpdir(), 'data.sqlite'), true, (err, db) => {
            q.error(err);

            srv.main(db, () => {
                q.end();
            });
        });
    });

    t.test('Stations - Empty Stations', (q) => {
        request('http://localhost:4000/api/stations', (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), {
                type: 'FeatureCollection',
                features: []
            });
            q.end();
        });
    });

    t.test('Stations - Create Windy Ridge', (q) => {
        request({
            url: 'http://localhost:4000/api/station',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                id: 1,
                name: 'Windy Ridge',
                lon: -118.44926834106445,
                lat: 37.335497334999936,
                windspeedlegend: [10, 20, 25, 30]
            })
        }, (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), true);
            q.end();
        });
    });

    t.test('Stations - Single Stations', (q) => {
        request('http://localhost:4000/api/stations', (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), {
                type: 'FeatureCollection',
                bbox: [ -118.49418410527042, 37.299776417065324, -118.40435257685847, 37.371201273908426 ],
                features: [{
                    id: 1,
                    type: 'Feature',
                    bbox: [ -118.45375991748506, 37.331926007166906, -118.44477676464385, 37.339068493042674 ],
                    properties: {
                        name: 'Windy Ridge',
                        legend: {
                            wind_speed: [ 10, 20, 25, 30 ],
                            wind_dir: []
                        }
                    },
                    geometry: {
                        type: 'Point',
                        coordinates: [ -118.44926834106445, 37.335497334999936 ]
                    }
                }]
            });
            q.end();
        });
    });

    t.test('Stations - Create Bishop Centre', (q) => {
        request({
            url: 'http://localhost:4000/api/station',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                id: 2,
                name: 'Bishop Centre',
                lon: -118.4044647216797,
                lat: 37.36633719814676,
                windspeedlegend: [5, 10, 50, 80]
            })
        }, (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), true);
            q.end();
        });
    });

    t.test('Stations - Create Pine Creek', (q) => {
        request({
            url: 'http://localhost:4000/api/station',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                id: 3,
                name: 'Pine Creek',
                lon: -118.67053985595703,
                lat: 37.38625355900476
            })
        }, (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), true);
            q.end();
        });
    });

    t.test('Stations - Create White Mountains', (q) => {
        request({
            url: 'http://localhost:4000/api/station',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                id: 4,
                name: 'White Mountains',
                lon: -118.2403564453125,
                lat: 37.55764242679522,
                windspeedlegend: [15, 16, 19, 22]
            })
        }, (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), true);
            q.end();
        });
    });

    t.test('Stations - Populate Data', (q) => {
        ticker();

        function ticker() {
            tick++;

            data(() => {
                if (tick < 0) return ticker();

                q.end();
            });
        }
    });

    if (!args.mock) {
        t.test('Stations - End Server', (q) => {
            q.end();
            t.end();
            process.exit();
        });
    } else {
        t.test('Stations - Server Running', (q) => {
            console.log('ok - Server: http://localhost:4000/');
            console.log('ok - CTRL + C to stop server');

            timer();

            function timer() {
                setTimeout(() => {
                    data(timer);
                }, 4000);
            }
        });
    }
});

function data(cb) {
    let timestamp = moment();

    if (tick < 0) {
        timestamp.subtract(tick * -4, 'seconds');
    }

    request.post({
        url: 'http://localhost:4000/api/station/1/data',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            timestamp: timestamp.unix(),
            wind_speed: 20,
            wind_direction: 100,
            battery: Math.sin(tick)
        })
    }, (err, res) => {
        if (err) throw err;

        if (cb) cb();
    });
}
