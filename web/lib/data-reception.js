import Err from '@openaddresses/batch-error';
import util from 'util';
import sqlite3 from 'sqlite3';
import moment from 'moment';

const packetTimes = new Map();
const minInterval =  60000; // 60 seconds

let dbAll, dbGet, dbRun;

export function setDbFunctions(db)
{
    dbAll = util.promisify(sqlite3.Database.prototype.all).bind(db);
    dbGet = util.promisify(sqlite3.Database.prototype.get).bind(db);
    dbRun = util.promisify(sqlite3.Database.prototype.run).bind(db);
}

export async function postStationData(req, res)
{
    for (const key of ['timestamp', 'wind_speed', 'wind_direction']) {
        if (req.body[key] === undefined) {
            res.status(400).json({
                status: 400,
                error: `${key} key required`
            });
            return false;
        }
    }

    try {
        moment.unix(req.body.timestamp);
    } catch (err) {
        res.status(400).json({
            status: 400,
            error: 'timestamp must be an integer (unix) date'
        });
        return false;
    }

    let uniqueKey = undefined;
    const currentTimestamp = Date.now();
    if (req.body.uniqueID !== undefined)
    {
        uniqueKey = (req.params.id << 8) + req.params.uniqueID;
        if (currentTimestamp - packetTimes.get(uniqueKey) < minInterval)
        {
            res.json('success - duplicate');
            return false;
        }
    }

    

    try {
        const wsOffset = await (dbGet('SELECT Wind_Direction_Offset FROM stations WHERE id = $id'
            , { $id: req.params.id }));
        if (!wsOffset) {
            res.status(404).json({
                status:404,
                error:`Station ${req.params.id} not found.`
            });
            return false;
        }

        const invalidWindspeed = typeof(req.body.wind_gust) == 'number' && req.body.wind_speed > 2 * (req.body.wind_gust + 2);
        const dest = invalidWindspeed ? 'Discarded_Data' : 'Station_Data';

        req.body.wind_direction = ((req.body.wind_direction + wsOffset.Wind_Direction_Offset) % 360 + 360) % 360;
        await dbRun(`
            INSERT INTO ${dest} (
                Station_ID,
                Timestamp,
                Windspeed,
                Wind_Direction,
                Battery_Level,
                Internal_Temp,
                External_Temp,
                Wind_Gust,
                Pwm,
                Current,
                Humidity,
                Light,
                UV,
                External_Battery
            ) VALUES (
                $id,
                $timestamp,
                $windspeed,
                $winddir,
                $battery,
                $internal_temp,
                $external_temp,
                $wind_gust,
                $pwm,
                $current,
                $humidity,
                $light,
                $uv_index,
                $external_batt
            )
        `, {
            $id: req.params.id,
            $timestamp: moment.unix(req.body.timestamp).unix(),
            $windspeed: req.body.wind_speed,
            $winddir: req.body.wind_direction,
            $battery: typeof(req.body.battery) == 'number' ? req.body.battery : null,
            $internal_temp: typeof(req.body.internal_temp) == 'number' ? req.body.internal_temp : null,
            $external_temp: typeof(req.body.external_temp) == 'number' ? req.body.external_temp : null,
            $wind_gust: typeof(req.body.wind_gust) == 'number' ? req.body.wind_gust : req.body.wind_speed,
            $pwm: typeof(req.body.pwm) == 'number' ? req.body.pwm : null,
            $current: typeof(req.body.current) == 'number' ? req.body.current : null,
            $humidity: typeof(req.body.humidity) == 'number' ? req.body.humidity : null,
            $light: typeof(req.body.light) == 'number' ? req.body.light : null,
            $uv_index: typeof(req.body.uv_index) == 'number' ? req.body.uv_index : null,
            $external_batt: typeof(req.body.external_batt) == 'number' ? req.body.external_batt : null
        });

        if (invalidWindspeed) {
            res.status(422).json({
                status: 422,
                error: 'Invalid windspeed stronger than gust'
            });
            return false;
        }

        if (uniqueKey !== undefined)
            packetTimes.set(uniqueKey, currentTimestamp);

        res.json('success');
    } catch (err) {
        Err.respond(err, res);
        return false;
    }
    return true;
}