import util from 'util';
import sqlite3 from 'sqlite3';

let dbAll, dbGet, dbRun;

export function setCmdDbFunctions(db)
{
    dbAll = util.promisify(sqlite3.Database.prototype.all).bind(db);
    dbGet = util.promisify(sqlite3.Database.prototype.get).bind(db);
    dbRun = util.promisify(sqlite3.Database.prototype.run).bind(db);
}

export async function addCommand(req, res)
{
    if (req.body.commandType === undefined) {
        res.status(400).json({
            status: 400,
            error: `$commandType key required`
        });
        return null;
    }

    const requestTime = Math.floor(Date.now() / 1000);

    const rowId = await dbGet(`INSERT INTO commands (station_id, command_type, command_data, request_time, attempts)
        VALUES
        ($id, $command_type, $command_data, $request_time);
        
        SELECT last_insert_rowid()`,
        {
            $id: req.params.id,
            $command_type: req.body.commandType,
            $command_data: req.body.commandData,
            $request_time: requestTime
        });
    res.json({
        result: 'success',
        commandId: rowId
    });
    return rowId;
}

export async function getCommands(req, res)
{
    let query = 'SELECT * from commands WHERE TRUE ';
    let params = {};
    if (req.params.id !== undefined) {
        query += ' AND station_id = $station_id';
        params['$station_id'] = req.params.id;
    }
    if (req.query.start !== undefined) {
        query += ' AND request_time >= $request_time';
        params['$request_time'] = req.query.start;
    }
    if (req.query.unhandled !== undefined) {
        query += ' AND response_time IS NULL';
    }
    if (req.query.commandId !== undefined) {
        query += ' AND id = $command_id';
        params['$command_id'] = req.query.commandId;
    }

    const resp_data = await dbAll(query, params);

    return res.json(resp_data);
}

export async function patchCommand(req, res)
{
    let ret = false;
    for (const key of ['id', 'commandId']) {
        if (isNaN(parseInt(req.params[key]))) {
            res.status(400).json({
                status: 400,
                error: `${key} url param must be number`
            });
            return false;
        } else {
            req.params[key] = parseInt(req.params[key]);
        }
    }
    if (!!req.body.attempt) {
        await dbRun(`UPDATE commands SET attempts = attemps + 1 
            WHERE id = $command_id`,
            { $command_id: req.params.commandId });
        ret = true;
    }
    if (req.body.response !== undefined) {
        const responseTime  = Math.floor(Date.now() / 1000);
        await dbRun(`UPDATE commands SET 
            response_time = $response_time,
            response = $response
            WHERE
            id = $command_id`,
            {
                $response_time: responseTime,
                $response: req.body.response,
            });
        ret = true;
    }
    return ret;
}

export async function deleteCommand(req, res)
{
    for (const key of ['id', 'commandId']) {
        if (isNaN(parseInt(req.params[key]))) {
            res.status(400).json({
                status: 400,
                error: `${key} url param must be number`
            });
            return false;
        } else {
            req.params[key] = parseInt(req.params[key]);
        }
    }
    await dbRun('DELETE FROM commands WHERE id = $command_id',
        { $command_id: req.params.commandId });
    return true;
}