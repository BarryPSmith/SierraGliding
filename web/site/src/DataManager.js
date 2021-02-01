import NanoEvents from 'nanoevents'
import bs from 'binary-search' //'../../node_modules/binary-search/index.js';

export default class DataManager {
    constructor(station_id) {
        this.emitter = new NanoEvents();

        // Sets that contain the data:
        this.windspeedData= [];
        this.windspeedMaxData = [];
        this.windspeedMinData = [];
        this.windspeedAvgData = [];
        this.windDirectionData= [];
        this.batteryData= [];
        this.internalTempData = [];
        this.externalTempData = [];
        
        // Resolution properties=
        this.desiredResolution= 2000;
        this.minResolution= 1000;
        this.maxResolution= 4000;
        this.maxDataPoints = 8000;
        this.lastEnsureSpan = 1;

        this.unit = 'mph';
        
        // Sample properties=
        this.currentSampleInterval= null;
        this.stationDataRate= 4;
        this.statLength = 600;
        this.avgDecimation = this.statLength / 20;
        
        // Keep track of what data we currently have.
        // A value of null for currentEnd indicates the ever evolving present.
        this.currentStart= null;
        this.currentEnd= null;
        
        // Raw data=
        this.stationData= [];
        
        this.fetchUrl = `${window.location.protocol}//${window.location.host}/api/station/${station_id}/data`;
        
        // This promiseToken is changed every time a complete reload is called.
        // When that happens; all updates should be no-ops.
        this.curPromiseToken= Math.random() //We could make this a GUID, but it seems overkill.
    }

    on() {
        return this.emitter.on.apply(this.emitter, arguments);
    }

    data_updated() {
        this.emitter.emit('data_updated');
    }
        
    //Ensures that data is available from start until end with an acceptable resolution.
    //cb: callback(bool reloaded, bool anyChange) where reloaded indicates all new data was loaded, otherwise data was appended.
    ensure_data(start, end, cb) {
        let actualEnd = end;
        if (end === null) {
            actualEnd = new Date();
        }
        
        const span = (actualEnd - start) / 1000;
        this.lastEnsureSpan = span;
        const minInterval = Math.max(span / this.maxResolution, 1);
        const maxInterval = Math.max(span / this.minResolution, this.stationDataRate);
        const expectedTotalSpan = (Math.max(actualEnd, this.get_actual_end()) - Math.min(start, this.currentStart)) / 1000;

        const actualDataRate = Math.max(this.currentSampleInterval, this.stationDataRate);
        
        if (start > new Date()) {
            return Promise.resolve({
                reloadedData: false,
                anyChange: false
            }); //We're going to ignore them if they ask for data in the future.
        }
        
        //Figure out if we need to throw away all of our data and refresh
        const refreshRequired = 
            //We will get a full refresh if we've never got data before:
            (this.currentSampleInterval === null || this.currentStart === null) 
            ||
            //We will get a full refresh if there is no overlap
            (actualEnd < this.currentStart - this.currentSampleInterval * 1000) 
            ||
            (this.currentEnd !== null && start > this.currentEnd + this.currentSampleInterval * 1000)
            ||
            //We will also get a full refresh if the resolution wouldn't fit:
            (minInterval > this.currentSampleInterval || maxInterval < this.currentSampleInterval)
            ||
            //We will get a full refresh if this causes our buffer to overflow:
            (expectedTotalSpan / actualDataRate > this.maxDataPoints)
            ||
            //We will get a full refresh if the new span completely encloses our current data:
            (start < this.currentStart && this.get_actual_end() < actualEnd);
        
        let thePromise = null;
        if (refreshRequired) {
            thePromise = this.load_data(start, end, actualEnd);
        } else {
            thePromise = this.add_data(start, end, actualEnd);
        }
            
        //We're going to tack on refreshRequired so the caller can choose not to wait on the promise.
        thePromise.refreshRequired = refreshRequired;

        if (cb) {
            return thePromise.then(
                () => { 
                    cb(refreshRequired, null); 
                },
                (err) => { 
                    cb(refreshRequired, err); 
                });
        } else {
            return thePromise.then((anyChange) => {
                return {
                    reloadedData: refreshRequired,
                    anyChange: anyChange
                };
            });
        }
    }
    
    end_is_now() {
        return this.currentEnd === null;// || Number.isNan(this.currentEnd); //Why does chrome report that Number.isNan is not a function?
    }
    
    get_actual_end() {
        if (this.end_is_now()) {
            return new Date();
        }
        else {
            return this.currentEnd;
        }
    }
    
    add_data(start, end, actualEnd) {
        //Don't fetch data we already have
        var span = actualEnd - start;
        var curEnd = this.get_actual_end();
        if (start >= this.currentStart && start < curEnd)
            start = curEnd;
        if (actualEnd <= curEnd && actualEnd > this.currentStart)
            actualEnd = this.currentStart;
        
        //Do nothing if we already have all the necessary data
        if (start >= actualEnd)
            return Promise.resolve(false);

        //We're going to buffer ahead. The amount we buffer will be 33% of the window we're asked to ensure.
        //We also add in an extra five seconds for the hell of it.
        //This should stop us from sending a million requests to the server as the user pans a graph.
        const endDiff = actualEnd - curEnd;
        const startDiff = this.currentStart - start;

        if (endDiff > 0)
            actualEnd = +actualEnd + span / 3 + 5000;
        if (startDiff > 0)
            start = +start - span / 3 + 5000;
    
        //For now, we're going to cheat and assume that the promise runs to completion.
        if (end === null || this.currentEnd === null) {
            this.currentEnd = null;
        } else {
            this.currentEnd = Math.max(this.currentEnd, actualEnd);
        }
        this.currentStart = Math.min(this.currentStart, start);
    
        const promiseToken = this.curPromiseToken;
        
        const promise = this.fetch_station_data(start, actualEnd, this.currentSampleInterval)
            .then(newStationData => {
                if (this.curPromiseToken != promiseToken) {
                    return false;
                }
            
                if (newStationData.length == 0) {
                    return false;
                }
            
                //Get our insertion index. We're going to do a proper check in case promises run out of order.
                let i = 0;
                for (; i < this.stationData.length; i++) {
                    if (this.stationData[i].timestamp > newStationData[0].timestamp) {
                        break;
                    }
                }
            
                this.stationData.splice(i, 0, ...newStationData);
                this.windDirectionData.splice(i, 0, ...newStationData.map(this.get_wind_dir_entry));
                this.windspeedData.splice(i, 0, ...newStationData.map(this.get_wind_spd_entry, this));
                this.windspeedAvgData.splice(i, 0, ...this.get_decimated_wind_spd_avg_entries(newStationData));
                this.windspeedMinData.splice(i, 0, ...this.get_distinct_ws_entries(newStationData, 'windspeed_min'));
                this.windspeedMaxData.splice(i, 0, ...this.get_distinct_ws_entries(newStationData, 'windspeed_max'));
                this.batteryData.splice(i, 0, ...this.get_batt_entries(...newStationData));
                this.internalTempData.splice(i, 0, ...this.get_internalTemp_entries(...newStationData));
                this.externalTempData.splice(i, 0, ...this.get_externalTemp_entries(...newStationData));

                this.data_updated();

                return true;
            });;
        
        return promise;
    }
    
    push_if_appropriate(newDataPoint)
    {
        if (newDataPoint.op === undefined || newDataPoint.op == 'Add')
            this.add_point(newDataPoint);
        else if (newDataPoint.op == 'Remove')
            this.remove_point(newDataPoint);
    }

    remove_point(dataPoint)
    {
        //Technically we should check if the point falls in our timerange. But this is easier.
        if (!this.end_is_now())
            return;

        for (let i = this.stationData.length - 1; i >= 0; i--) {
            if (this.stationData[i].timestamp < dataPoint.timestamp)
                break;
            if (this.stationData[i].timestamp == dataPoint.timestamp)
                this.stationData.splice(i, 1);
        }
        const jsTimestamp = dataPoint.timestamp * 1000;
        for (const arr of this.allDatasets()) {
            for (let i = arr.length - 1; i >= 0; i--) {
                if (+arr[i].x < jsTimestamp)
                    break;
                if (+arr[i].x == jsTimestamp) {
                    arr.splice(i, 1);
                }
            }
        }
    }

    allDatasets() {
        return [this.windDirectionData, this.windspeedData, 
                            this.windspeedAvgData, this.windspeedMinData, this.windspeedMaxData,
                            this.batteryData,
                           this.internalTempData, this.externalTempData];
    }

    findPrevIdx(arr, comp)
    {
        let idx = arr.length - 1;
        while (idx > 0 && comp(arr[idx]))
            idx--;
        if (idx < 0)
            idx = 0;
        return idx;
    }

    spliceBefore(arr, comp, data)
    {
        const idx = this.findPrevIdx(arr, comp) + 1;
        arr.splice(idx, 0, data);
    }

    add_point(newDataPoint) {
        if (!this.end_is_now())
            return;
        
        let idx = this.stationData.length - 1;
        while (idx > 0 && this.stationData[idx].timestamp > newDataPoint.timestamp)
            idx--;
        if (idx < 0)
            idx = 0;
        this.spliceBefore(this.stationData, d => d.timestamp > newDataPoint.timestamp, newDataPoint);
        const jsTimestamp = newDataPoint.timestamp * 1000;
        this.spliceBefore(this.windDirectionData, pt => pt.x > jsTimestamp, this.get_wind_dir_entry(newDataPoint));
        this.spliceBefore(this.windspeedData, pt => pt.x > jsTimestamp, this.get_wind_spd_entry(newDataPoint));
        this.spliceBefore(this.batteryData, pt => pt.x > jsTimestamp, this.get_batt_entries(newDataPoint));
        this.spliceBefore(this.internalTempData, pt => pt.x > jsTimestamp, this.get_internalTemp_entries(newDataPoint));
        this.spliceBefore(this.externalTempData, pt => pt.x > jsTimestamp, this.get_externalTemp_entries(newDataPoint));
        /*this.stationData.splice(idx, 0, newDataPoint);
        //this.stationData.push(newDataPoint);
        this.windDirectionData.push(this.get_wind_dir_entry(newDataPoint));
        this.windspeedData.push(this.get_wind_spd_entry(newDataPoint));
        this.batteryData.push(...this.get_batt_entries(newDataPoint));
        this.internalTempData.push(...this.get_internalTemp_entries(newDataPoint));
        this.externalTempData.push(...this.get_externalTemp_entries(newDataPoint));*/

        const wsAvgEntry = this.get_entry(newDataPoint, 'windspeed_avg', this.wsFactor());
        if (this.windspeedAvgData.length >= 2 
            && this.windspeedAvgData[this.windspeedAvgData.length - 2].x + this.avgDecimation < wsAvgEntry.x) {
            this.windspeedAvgData.pop();
        }
        this.windspeedAvgData.push(wsAvgEntry);

        const wsMaxEntry = this.get_entry(newDataPoint, 'windspeed_max', this.wsFactor());
        if (this.windspeedMaxData.length >= 2
            && this.windspeedMaxData[this.windspeedMaxData.length - 2].y == this.windspeedMaxData[this.windspeedMaxData.length - 1].y) {
            this.windspeedMaxData.pop();
        }
        this.windspeedMaxData.push(wsMaxEntry);

        const wsMinEntry = this.get_entry(newDataPoint, 'windspeed_min', this.wsFactor());
        if (this.windspeedMinData.length >= 2
            && this.windspeedMinData[this.windspeedMinData.length - 2].y == this.windspeedMinData[this.windspeedMinData.length - 1].y) {
            this.windspeedMinData.pop();
        }
        this.windspeedMinData.push(wsMinEntry);
        
        //Trim if necessary.
        const viewStart = this.get_actual_end() - this.lastEnsureSpan * 1000;

        for (const cur of [this.stationData, ...this.allDatasets()]) {
            const startIdx = bs(cur, viewStart, (element, needle) => (element.x === undefined ? element.timestamp * 1000 : element.x) - needle);
            if (startIdx < 0)
                startIdx = ~startIdx;
            // If we've got so much data that we're overloading the chart,
            // replace our data with resampled data from the server.
            if (cur.length - startIdx > this.maxResolution)
                this.load_data(viewStart, this.currentEnd, this.get_actual_end());
            else {
                const toRemove = cur.length - this.maxDataPoints;
                if (toRemove > 0) {
                    cur.splice(0, toRemove + 50);
                }
            }
        }
        this.data_updated();
    }
        
    get_wind_dir_entry(entry) {
        return { 
            x: new Date(entry.timestamp * 1000),
            y: entry.wind_direction
        };
    }

    wsFactor() { 
        return this.unit == 'mph' ? 1.6 : 1.0; 
    } 
    
    get_wind_spd_entry(entry) {
        return {
            x: new Date(entry.timestamp * 1000),
            y: entry.windspeed / this.wsFactor()
        };
    }

    get_decimated_wind_spd_avg_entries(entries) {
        if (!entries.length) {
            return [];
        }
        let last = entries[0];
        let ret = [this.get_entry(entries[0], 'windspeed_avg', this.wsFactor())];
        for (let i = 1; i < entries.length; i++) {
            const curEntry = entries[i];
            if (i == entries.length - 1 ||
                curEntry.timestamp > last.timestamp + this.avgDecimation) {
                ret.push(this.get_entry(curEntry, 'windspeed_avg', this.wsFactor()));
                last = curEntry;
            }
        }
        return ret;
    }

    get_distinct_ws_entries(entries, identifier) {
        if (!entries.length) {
            return [];
        }
        let last = entries[0];
        let ret = [this.get_entry(entries[0], identifier, this.wsFactor())];
        for (let i = 1; i < entries.length; i++) {
            if (i == entries.length - 1 ||
                entries[i][identifier] != last[identifier]) {
                ret.push(this.get_entry(entries[i], identifier, this.wsFactor()));
                last = entries[i];
            }
        }
        return ret;
    }

    get_entry(entry, identifier, factor = 1) {
        return {
            x: new Date(entry.timestamp * 1000),
            y: entry[identifier] / factor
        };
    }
    
    get_batt_entries() {
        return Array.from(arguments)
            .filter(entry => typeof(entry.battery_level) == 'number')
            .map(entry => { 
                return {
                    x: new Date(entry.timestamp * 1000),
                    y: entry.battery_level
                };
            });
    }
    
    get_internalTemp_entries() {
        return Array.from(arguments)
            .filter(entry => typeof(entry.internal_temp) == 'number')
            .map(entry => { return {
                x: new Date(entry.timestamp * 1000),
                y: entry.internal_temp,
            }});
    }

    get_externalTemp_entries() {
        return Array.from(arguments)
            .filter(entry => typeof(entry.internal_temp) == 'number')
            .map(entry => { return {
                x: new Date(entry.timestamp * 1000),
                y: entry.external_temp,
            }});
    }
    
    load_data(start, end, actualEnd) {
        //Ensure that any updates that have been requested based on the previous data discard their results:
        this.curPromiseToken = Math.random();
        
        this.currentStart = start;
        this.currentEnd = end;
        
        this.currentSampleInterval = Math.max((actualEnd - start) / this.desiredResolution / 1000, 1);

        const promiseToken = this.curPromiseToken;
        
        return this.fetch_station_data(start, actualEnd, this.currentSampleInterval)
            .then(newStationData => {
                if (promiseToken == this.curPromiseToken) {
                    this.stationData = newStationData;
                    this.windDirectionData = newStationData.map(this.get_wind_dir_entry);
                    this.windspeedData = newStationData.map(this.get_wind_spd_entry, this);
                    this.windspeedMaxData = this.get_distinct_ws_entries(newStationData, 'windspeed_max');
                    this.windspeedMinData = this.get_distinct_ws_entries(newStationData, 'windspeed_min');
                    this.windspeedAvgData = this.get_decimated_wind_spd_avg_entries(newStationData);
                    this.batteryData = this.get_batt_entries(...newStationData);
                    this.internalTempData = this.get_internalTemp_entries(...newStationData);
                    this.externalTempData = this.get_externalTemp_entries(...newStationData);

                    this.data_updated();

                    return true;
                } else {
                    return false;
                }

        });
    }
    
    fetch_station_data(start, end, sample) {
        const url = new URL(this.fetchUrl);

        url.searchParams.append('start', start / 1000);
        url.searchParams.append('end', end / 1000);
        url.searchParams.append('sample', sample);

        if (Number.isNaN(end / 1000) || Number.isNaN(start / 1000) || Number.isNaN(start))
            throw new Error('Invalid parameter pass to fetch_station_data');
        
        const dataFetch = fetch(url, {
            method: 'GET',
            credentials: 'same-origin'
        }).then((response) => {
            if (response.ok) {
                return response.json();
            } else if (response.status === 404) {
                throw new Error('Station Not Found');
            } else {
                throw new Error('Could Not Retreive Station Data');
            }
        })
        
        return dataFetch;
    }
}