export default class DataManager {
    constructor(station_id) {
        // Sets that contain the data:
        this.windspeedData= [];
        this.windDirectionData= [];
        this.batteryData= [];
        
        // Resolution properties=
        this.desiredResolution= 2000;
        this.minResolution= 1000;
        this.maxResolution= 4000;
        this.maxDataPoints= 8000;
        
        // Sample properties=
        this.currentSampleInterval= null;
        this.stationDataRate= 4;
        
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
        
    //Ensures that data is available from start until end with an acceptable resolution.
    //cb: callback(bool reloaded) where reloaded indicates all new data was loaded, otherwise data was appended.
    ensure_data(start, end, cb) {
        let actualEnd = end;
        if (end === null) {
            actualEnd = new Date();
        }
        
        const span = (actualEnd - start) / 1000;
        const minInterval = Math.max(span / this.maxResolution, 1);
        const maxInterval = Math.min(span / this.minResolution, this.stationDataRate);
        const expectedTotalSpan = (Math.max(actualEnd, this.get_actual_end()) - Math.min(start, this.currentStart)) / 1000;
        
        if (start > new Date())
            return; //We're going to ignore them if they ask for data in the future.
        
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
            (expectedTotalSpan / this.currentSampleInterval)
            ||
            //We will get a full refresh if the new span completely encloses our current data:
            (start < this.currentStart && this.get_actual_end() < actualEnd);
        
        let thePromise = null;
        if (refreshRequired)
            thePromise = this.load_data(start, end, actualEnd);
        else
            thePromise = this.add_data(start, end, actualEnd);
            
        //We're going to tack on refreshRequired so the caller can choose not to wait on the promise.
        thePromise.refreshRequired = refreshRequired;
            
        if (cb)
            return thePromise.then(
                () => { cb(refreshRequired, null); },
                (err) => { cb(refreshRequired, err); });
        else
            return thePromise.then(() => refreshRequired);
    }
    
    end_is_now() {
        return this.currentEnd === null || isNan(this.currentEnd);
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
        var curEnd = get_actual_end();
        if (start >= this.currentStart && start < curEnd)
            start = curEnd;
        if (actualEnd <= curEnd && actualEnd > this.currentStart)
            actualEnd = this.currentStart;
        
        //Do nothing if we already have all the necessary data
        if (start >= actualEnd)
            return Promise.resolve();
    
        //For now, we're going to cheat and assume that the promise runs to completion.
        this.currentEnd = Math.max(this.currentEnd, end);
        this.currentStart = Math.min(this.currentStart, start);
    
        const promiseToken = this.curPromiseToken;
        
        const promise = this.fetch_station_data(start, actualEnd, this.currentSampleInterval)
            .then(newStationData => {
            if (this.curPromiseToken != promiseToken) {
                return;
            }
            
            if (newStationData.length == 0) {
                return;
            }
            
            //Get our insertion index. We're going to do a proper check in case promises run out of order.
            let i = 0;
            for (; i < this.stationData.length; i++) {
                if (this.stationData[i].timestamp > newStationData[0].timestamp) {
                    break;
                }
            }
            
            this.stationData.splice(i, 0, ...newStationData);
            this.windDirectionData.splice(...newStationData.map(this.get_wind_dir_entry));
            this.windspeedData.splice(i, 0, newStationData.map(this.get_wind_spd_entry));
            this.batteryData.splice(i, 0, ...newStationData.map(this.get_batt_entry));
        });;
        
        return promise;
    }
    
    push_if_appropriate(newDataPoint)
    {
        if (!this.end_is_now())
            return;
        
        this.stationData.push(newDataPoint);
        this.windDirectionData.push(this.get_wind_dir_entry(newDataPoint));
        this.windspeedData.push(this.get_wind_spd_entry(newDataPoint));
        this.batteryData.push(this.get_batt_entry(newDataPoint));
        
        //Trim if necessary.
        for (const cur of [this.stationData, this.windDirectionData, this.windspeedData, this.batteryData]) {
            if (cur.length > this.maxDataPoints) {
                cur.splice(0, 50);
            }
        }       
    }
        
    get_wind_dir_entry(entry) {
        return { 
            x: new Date(entry.timestamp * 1000),
            y: entry.wind_direction
        };
    }
    
    get_wind_spd_entry(entry) {
        return {
            x: new Date(entry.timestamp * 1000),
            y: entry.windspeed
        };
    }
    
    get_batt_entry(entry) {
        return {
            x: new Date(entry.timestamp * 1000),
            y: entry.battery_level
        };
    }
    
    load_data(start, end, actualEnd) {
        //Ensure that any updates that have been requested based on the previous data discard their results:
        this.curPromiseToken = Math.random();
        
        this.currentStart = start;
        this.curEnd = end;
        
        this.currentSampleInterval = Math.max((actualEnd - start) / this.desiredResolution / 1000, 1);
        
        return this.fetch_station_data(start, actualEnd, this.currentSampleInterval)
            .then(newStationData => {
            this.stationData = newStationData;
            this.windDirectionData = newStationData.map(this.get_wind_dir_entry);
            this.windspeedData = newStationData.map(this.get_wind_spd_entry);
            this.batteryData = newStationData.map(this.get_batt_entry);
        });
    }
    
    fetch_station_data(start, end, sample) {
        let url = new URL(this.fetchUrl);

        url.searchParams.append('start', start / 1000);
        url.searchParams.append('end', end / 1000);
        url.searchParams.append('sample', sample);
        
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