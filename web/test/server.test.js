'use strict';

const test = require('tape');
const srv = require('../index');
const request = require('request');

test('Stations', (t) => {
    t.test('Stations - Server', (q) => {
        srv.database('/tmp/data.sqlite', true, (err, db) => {
            q.error(err);

            srv.main(db, () => {
                q.end();
            });
        });
    });

    t.test('Stations - Empty Stations', (q) => {
        request('http://localhost:4000/api/stations', (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), []);
            q.end();
        });
    });

    t.test('Stations - Create Station', (q) => {
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
                windspeedlegend: [10, 20, 25]
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
            q.deepEquals(JSON.parse(res.body), [{
                id: 1,
                name: 'Windy Ridge',
                lon: -118.44926834106445,
                lat: 37.335497334999936,
                windspeedlegend: [ 10, 20, 25 ],
                winddirlegend: []
            }]);
            q.end();
        });
    });

    t.test('Stations - End Server', (q) => {
        q.end();
        process.exit();
    });

    t.end();
});
