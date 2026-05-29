import Err from '@openaddresses/batch-error';
import util from 'util';
import sqlite3 from 'sqlite3';
import crypto from 'crypto';

let db;

export function setAuthDatabase(_db) {
    db = _db;
}

export async function auth(req, res, next)
{
    try {
        const dbAll = util.promisify(sqlite3.Database.prototype.all).bind(db);
        const dbGet = util.promisify(sqlite3.Database.prototype.get).bind(db);
        
        const authHeader = req.headers['authorization'];
        const parts = authHeader && authHeader.split(' ');
        const type = parts[0];
        switch (parts[0])
        {
            case 'Bearer':
                const token = parts[1];
                if (token == null) {
                    return res.status(401).json({
                        status: 401,
                        error: "token required"
                    });
                }

                const dbToken = (await dbGet("SELECT * from tokens where token = $token",
                    { $token: token }
                ));
                if (!dbToken) {
                    return res.status(403).json({
                        status: 403,
                        error: "invalid token"
                    });
                }
                req.userId = dbToken.uid;
                next();
                break;
            case 'Basic':
                const base64Data = parts[1];
                const decoded = Buffer.from(base64Data, 'base64').toString('utf8');
                const [username, password] = decoded.split(':');
                if (username == null || password == null) {
                    return res.status(401).json({
                        status: 401,
                        error: "username and password required"
                    });
                }
                const user = await dbGet('SELECT * FROM users where username = $username',
                    { $username: username });
                if (user) {
                    const hash = crypto.createHash('sha256').update(password + user.salt).digest('base64');
                    if (hash == user.password) {
                        req.userId = user.id
                        next();
                        break;
                    }
                }
                res.status(403).json({
                    status: 403,
                    error: 'invalid username or password'
                });
                break;
            default:
                res.status(401).json({
                    status: 401,
                    error: 'authentication required'
                });
                break;
        }
    } catch (err) {
        //Err.respond(err, res);
        next(err);
    }
}

export async function authPostCommand(req, res, next)
{
    if (!checkPermission(req.userId, req.body.stationId, 'post_command')) {
        {
            res.status(403).json({
                status: 403,
                error: 'post command permission does not exist on this station'
            });
        }
    }
    else
        next();
}

export async function authPostData(req, res, next)
{
    if (!checkPermission(db, req.userId, req.params.id, 'post_data')) {
        res.status(403).json({
            status:403,
            error: 'No post permission on this station'
        });
    }
    else
        next();
}

export async function checkGroupPermission(user_id, group_id, permission)
{
    const dbGet = util.promisify(sqlite3.Database.prototype.get).bind(db);
    const granted = await dbGet(`SELECT 1 FROM user_permissions WHERE
        user_id = $user_id AND group_id IS $group_id AND permissions LIKE '%${permission}%'`, {
            $user_id: user_id,
            $group_id: group_id
        });
    return !!granted
}

export async function checkPermission(user_id, station_id, permission)
{
    const dbGet = util.promisify(sqlite3.Database.prototype.get).bind(db);
    const granted = await dbGet(`SELECT 1 FROM user_permissions up
        JOIN stations s ON s.group_id IS up.group_id
        WHERE
        up.user_id = $user_id AND s.id = $station_id AND permissions LIKE '%${permission}%'`, {
            $user_id: user_id,
            $station_id: station_id
        });
    return !!granted
}