#!/usr/bin/env node

'use strict';

const sqlite3 = require('sqlite3');
const bcrypt = require('bcrypt');
const args = require('minimist')(process.argv, {
    string: ['db'],
    boolean: ['help']
});

if (!args.db || args.help) {
    return help();
}

/**
 * Print server CLI help documentation to STDOUT
 */
function help() {
    console.log('');
    console.log('  Initial User Management');
    console.log('');
    console.log('  Usage ./user.js create');
    console.log('');
    console.log('  Options:');
    console.log('  --db <DB.sqlite>     [required] Backend Database to Create or Use');
    console.log('  --help               [optional] Print this message to stdout');
    console.log('');
    process.exit();
}

function database(dbpath, drop, cb) {
    const db = new sqlite3.Database(dbpath);

    db.serialize(() => {
        
    });
}
