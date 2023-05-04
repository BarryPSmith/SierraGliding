import sqlite3 from 'sqlite3';

/**
 * Instantiate a new Sqlite Database, creating schema is needed
 *
 * @param {String} dbpath Path to existing db or db to create
 * @param {Boolean} drop Should a new database be created
 * @param {Function} cb Callback Function (err, db)
 */
export default function database(dbpath, drop, cb) {
    const db = new sqlite3.Database(dbpath);

    db.serialize(() => {
        db.run('PRAGMA foreign_keys = ON');
        db.run('PRAGMA journal_mode = WAL');
        db.run('PRAGMA busy_timeout = 1000');

        db.loadExtension('sqlite3_ext/extension-functions', (err) => {
            if (err) console.error(err);
        });

        if (drop) {
            db.run(`
                DROP TABLE IF EXISTS stations;
                DROP TABLE IF EXISTS station_data;
                DROP TABLE IF EXISTS station_data_100;
                DROP TABLE IF EXISTS station_data_600;
                DROP TABLE IF EXISTS users;
                DROP TABLE IF EXISTS tokens;
                DROP TABLE IF EXISTS station_groups;
            `);
        }

        db.run(`
            CREATE TABLE IF NOT EXISTS users (
                id                  SERIAL,
                username            TEXT PRIMARY KEY,
                password            TEXT NOT NULL
            )
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS tokens (
                uid                 INT NOT NULL REFERENCES users(id),
                token               TEXT PRIMARY KEY
            )
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS station_groups (
                ID INTEGER PRIMARY KEY,
                Name TEXT UNIQUE NOT NULL COLLATE NOCASE
            )
        `);

        db.run(`
            CREATE TABLE IF NOT EXISTS stations (
                ID                      INTEGER PRIMARY KEY,
                Name                    TEXT UNIQUE NOT NULL,
                Lon                     FLOAT NOT NULL,
                Lat                     FLOAT NOT NULL,
                Elevation               FLOAT NOT NULL,
                Wind_Speed_Legend       TEXT NULL,
                Wind_Dir_Legend         TEXT NULL,
                Wind_Direction_Offset   FLOAT NOT NULL DEFAULT 0, -- Used to compensate for misaligned stations. Applied when data is added to the station_data
                Display_Level           INTEGER NOT NULL DEFAULT 1, -- If we want to have some stations that are not usually displayed. 0 = Hidden, 1 = Public, 2 = Private (TODO: Figure out admin stuff)
                Status_Message          TEXT NULL, --If we want to temporarily display a status message when e.g. a station has a hardware fault
                Battery_Range           TEXT NULL,
                Group_ID                INTEGER NULL REFERENCES station_groups ON DELETE SET NULL ON UPDATE CASCADE,
                Missing_Features        TEXT NULL --JSON list of features this station does not report (e.g. ["external_temp", "current"])
            );
        `);

        for (const postFix of ['', '_100', '_600']) {
            db.run(`
                CREATE TABLE IF NOT EXISTS station_data${postFix} (
                    Station_ID          INT NOT NULL REFERENCES Stations(id) ON DELETE CASCADE ON UPDATE CASCADE,
                    Timestamp           INT NOT NULL,
                    Windspeed           FLOAT NOT NULL,
                    Wind_Direction      FLOAT NOT NULL,
                    Battery_Level       Float NULL,
                    Internal_Temp       FLOAT NULL,
                    External_Temp       FLOAT NULL,
                    Wind_Gust           FLOAT NULL,
                    Pwm                 INT NULL,
                    Current             INT NULL,
                    PRIMARY KEY(Station_ID, Timestamp)
                );
            `);
        }

        for (const interval of ['100', '600']) {
            db.run(`CREATE TRIGGER IF NOT EXISTS
                    Trg_Station_Data_${interval}
                    AFTER INSERT ON Station_Data
                    BEGIN
                      INSERT OR REPLACE INTO Station_Data_${interval}
                        (Station_ID, Timestamp, Windspeed,
                        Wind_Direction, Battery_Level,
                        Internal_Temp, External_Temp, Wind_Gust, Pwm, Current)
                      SELECT
                        NEW.Station_ID,
                        CEIL(NEW.Timestamp/${interval}.0) * ${interval} AS Timestamp,
                        AVG(windspeed) AS Windspeed,
                        (DEGREES(ATAN2(AVG(SIN(RADIANS(Wind_Direction))), AVG(COS(RADIANS(Wind_Direction))))) + 360) % 360 AS Wind_Direction,
                        AVG(Battery_Level) AS Battery_Level,
                        AVG(Internal_Temp) AS Internal_Temp,
                        AVG(External_Temp) AS External_Temp,
                        MAX(Windspeed) AS Wind_Gust,
                        AVG(Pwm) AS Pwm,
                        AVG(Current) AS Current
                      FROM
                        Station_Data
                      WHERE
                        (CEIL(NEW.Timestamp/${interval}.0) - 1) * ${interval} < Timestamp
                        AND Timestamp <= CEIL(NEW.Timestamp/${interval}.0) * ${interval}
                        AND Station_ID = NEW.Station_ID;
                    END;
            `);

            db.run(`CREATE TEMP TRIGGER IF NOT EXISTS
                    Trg_Station_Data_Del_${interval}
                    AFTER DELETE ON main.Station_Data
                    BEGIN
                        INSERT OR REPLACE INTO Station_Data_${interval}
                            (Station_ID, Timestamp, Windspeed,
                            Wind_Direction, Battery_Level,
                            Internal_Temp, External_Temp, Wind_Gust, Pwm, Current)
                        SELECT
                            OLD.Station_ID,
                            CEIL(OLD.Timestamp/${interval}.0) * ${interval} AS Timestamp,
                            AVG(windspeed) AS Windspeed,
                            (DEGREES(ATAN2(AVG(SIN(RADIANS(Wind_Direction))), AVG(COS(RADIANS(Wind_Direction))))) + 360) % 360 AS Wind_Direction,
                            AVG(Battery_Level) AS Battery_Level,
                            AVG(Internal_Temp) AS Internal_Temp,
                            AVG(External_Temp) AS External_Temp,
                            MAX(Windspeed) AS Wind_Gust,
                            AVG(Pwm) AS Pwm,
                            AVG(Current) AS Current
                        FROM
                            Station_Data
                        WHERE
                            (CEIL(OLD.Timestamp/${interval}.0) - 1) * ${interval} < Timestamp
                            AND Timestamp <= CEIL(OLD.Timestamp/${interval}.0) * ${interval}
                            AND Station_ID = OLD.Station_ID;

                        DELETE FROM
                            Station_Data_${interval}
                        WHERE
                            Station_ID = OLD.Station_ID
                            AND Timestamp = CEIL(OLD.Timestamp/${interval}.0) * ${interval}
                            AND NOT EXISTS (
                                SELECT 1
                                FROM Station_Data
                                WHERE
                                    (CEIL(OLD.Timestamp/${interval}.0) - 1) * ${interval} < Timestamp
                                    AND Timestamp <= CEIL(OLD.Timestamp/${interval}.0) * ${interval}
                                    AND Station_ID = OLD.Station_ID
                                );
                    END;`
            );
        }

        db.run(`
            CREATE TABLE IF NOT EXISTS discarded_data
            AS SELECT * FROM station_data LIMIT 0;
        `);

        db.get('SELECT sqlite_version() AS ver', (err, res) => {
            if (err) console.error(err);
            else (console.error('Sqlite3 Version: ' + res.ver));
        });

        return cb(null, db);
    });
}
