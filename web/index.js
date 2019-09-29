const express = require('express');
const sqlite3 = require('sqlite3');
const fs = require('fs');
const path = require('path');

const app = express();
const router = new express.Router();

app.disable('x-powered-by');

app.use(express.static(path.resolve(__dirname, 'site/dist')));
app.use('/api/', router);

router.use((req, res, next) => {
    console.error(`[${req.method}] /api${req.url}`);
    return next();
});

app.listen(4000, (err) => {
    if (err) throw err;

    console.error('Server listening http://localhost:4000');
});
