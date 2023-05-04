#!/usr/bin/env node

import sqlite3 from 'sqlite3';
import prompt from 'prompt';
import bcrypt from 'bcrypt';
import path from 'path';
import minimist from 'minimist';

const args = minimist(process.argv, {
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
    console.log('  Usage ./user.js [create|delete|update]');
    console.log('');
    console.log('  SubCommand:');
    console.log('');
    console.log('   create              Create a new user');
    console.log('   delete              Delete an existing user');
    console.log('   update              Update the password');
    console.log('');
    console.log('  Options:');
    console.log('  --db <DB.sqlite>     [required] Backend Database to Create or Use');
    console.log('  --help               [optional] Print this message to stdout');
    console.log('');
    process.exit();
}

return database();

function database() {
    const db = new sqlite3.Database(path.resolve(__dirname, args.db));

    db.serialize(() => {
        prompt.message = '$';
        prompt.start({
            stdout: process.stderr
        });

        if (process.argv[2] === 'create') {
            prompt.get([{
                name: 'username',
                message: 'Username',
                type: 'string',
                required: true
            },{
                name: 'password',
                message: 'Password',
                hidden: true,
                replace: '*',
                required: true,
                type: 'string'
            }], (err, res) => {
                if (err) throw err;


                hash(res.password, (err, password) => {
                    db.run(`
                        INSERT INTO users (
                            username,
                            password
                        ) VALUES (
                            $username,
                            $password
                        )
                    `, {
                        '$username': res.username,
                        '$password': password
                    }, (err) => {
                        if (err) throw err;
                    });
                });
            });
        } else if (process.argv[2] === 'delete') {
            prompt.get([{
                name: 'username',
                message: 'Username',
                type: 'string',
                required: true
            }], (err, res) => {
                if (err) throw err;

                db.run(`
                     DELETE FROM users
                        WHERE
                            username = $username
                `, {
                    '$username': res.username
                }, (err) => {
                    if (err) throw err;
                });
            });
        } else if (process.argv[2] === 'update') {
            prompt.get([{
                name: 'username',
                message: 'Username',
                type: 'string',
                required: true
            },{
                name: 'password',
                message: 'Password',
                hidden: true,
                replace: '*',
                required: true,
                type: 'string'
            }], (err, res) => {
                if (err) throw err;

                hash(res.password, (err, password) => {
                    db.run(`
                         UPDATE users
                            SET
                                password = $password
                            WHERE
                                username = $username
                    `, {
                        '$username': res.username,
                        '$password': password
                    }, (err) => {
                        if (err) throw err;
                    });
                });
            });

        } else {
            return help();
        }
    });
}

function hash(password, cb) {
    bcrypt.genSalt(10, (err, salt) => {
        if (err) return cb(err);

        bcrypt.hash(password, salt, cb);
    });
}
