import Err from '@openaddresses/batch-error';
import util from 'util';
import sqlite3 from 'sqlite3';


export default async function auth(db, req, res, next)
{
    try {
        const dbAll = util.promisify(sqlite3.Database.prototype.all).bind(db);

        const authHeader = req.headers['authorization'];
        const token = authHeader && authHeader.split(' ')[1];

        if (token == null) {
            return res.status(401).json({
                status: 401,
                error: "token required"
            });
        }

        const tokens = (await dbAll("SELECT token from tokens")).map(a => a["token"]);
        if (!tokens.includes(token)) {
            return res.status(403).json({
                status: 403,
                error: "invalid token"
            });
        }

        next();
    } catch (err) {
        //Err.respond(err, res);
        next(err);
    }
}