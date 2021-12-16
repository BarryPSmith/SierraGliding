/**
 * @class
 * @param {Number}          status  HTTP Status Code
 * @param {Error|String}    err     Original Error or Human Readable message if no error is present
 * @param {String}          human   Human Readable Error Message
 *
 * @prop {Number}           status  HTTP Status Code
 * @prop {Error}            err     Original Error Object
 * @prop {String}           human   Human readable string
 */
class HTTPError {
    constructor(status, err, human) {
        console.error(err);

        this.status = status;

        if (err instanceof Error) {
            this.err = err;
        }

        if (typeof err === 'string') {
            this.human = err;
        } else if (!human) {
            this.human = 'Unknown Error';
        } else {
            this.human = err;
        }
    }

    respond(res) {
        res.status(this.status).json({
            status: this.status,
            message: this.human
        });
    }
}

/**
 * Standard error logging and internal error response
 *
 * @param {Object} err HTTPError or Error like object
 * @param {Object} req Express Request Object
 * @param {Object} res Express Result Object
 */
function HTTPErrorMiddleware(err, req, res) {
    if (err instanceof HTTPError) return err.respond(res);

    if (err.code === 'SQLITE_CONSTRAINT') {
        res.status(422).json({
            status: 422,
            error: err.message
        });
    } else {
        res.status(500).json({
            status: 500,
            error: 'Internal Server Error'
        });
    }
}

module.exports = {
    HTTPError,
    HTTPErrorMiddleware
};

