const test = require('tape');
const srv = require('../index');
const request = require('request');

test('Stations', (t) => {
    t.test('Stations - Server', (q) => {
        srv.database('/tmp/data.sqlite', (err, db) => {
            q.error(err);

            srv.main(db, () => {
                q.end();
            });
        })
    });

    t.test('Stations - Empty Stations', (q) => {
        request('http://localhost:4000/api/stations', (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), []);
            q.end()
        });
    });

    t.test('Stations - Create Station', (q) => {
        request({
            url: 'http://localhost:4000/api/stations',
            method: 'POST',
            body: JSON.stringify({
            })
        }, (err, res) => {
            q.error(err);
            q.deepEquals(JSON.parse(res.body), []);
            q.end()
        });
    });

    t.end();
});
