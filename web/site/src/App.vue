<template>
    <div class='viewport-full'>
        <div class='absolute right top z1 h36 round'>
            <button @click='mode = "list"' class='bg-white my3 mx3 btn btn--stroke btn--gray round fr h36'><svg class='icon'><use href='#icon-menu'/></svg></button>
            <button @click='mode = "map"' v-if='map' class='bg-white my3 mx3 btn btn--stroke btn--gray round fr h36'><svg class='icon'><use href='#icon-map'/></svg></button>
        </div>

        <div v-bind:class='viewport' class='relative scroll-hidden'>
            <!-- Map -->

            <div id="map" :class='mapmode' class='h-full w-full bg-darken10'></div>

            <template v-if='mode === "list"'>
                <div class='h-full w-full'>
                    <div class='align-center border-b border--gray-light h36 my6'>
                        <h1 class='txt-h4'>Active Stations</h1>
                    </div>

                    <template v-for='station_feat in stations.features'>
                        <div @click='station.id = station_feat.id' class='cursor-pointer py12 px12 col col--12 clearfix bg-gray-light-on-hover'>
                            <span class='fl txt-h3'  v-text='station_feat.properties.name'></span>
                        </div>
                    </template>
                </div>
            </template>
        </div>

        <template v-if='station.id'>
            <div class='viewport-half relative scroll-hidden headerGrid'>
                <div class='py12 px12 clearfix border-t border--gray-light'>
                    <h1 class='txt-h2 fl' v-text='station.name'></h1>

                    <button @click='station.id = false' class='btn btn--stroke btn--gray round fr h36'><svg class='icon'><use href='#icon-close'/></svg></button>

                    <div v-if='!station.error' class="flex-parent-inline fr px24">
                        <button @click='dataType = "wind"' :class='modeWind' class="btn btn--pill btn--pill-hl">Wind</button>
                        <button @click='dataType = "battery"' :class='modeBattery' class="btn btn--pill btn--pill-hr">Battery</button>
                    
                        <button @click='duration =   300' :class='dur300' class="btn btn--pill btn--pill-hl">5 Minutes</button>
                        <button @click='duration =  3600' :class='dur3600' class="btn btn--pill btn--pill-hc">1 Hour</button>
                        <button @click='duration = 14400' :class='dur14400' class="btn btn--pill btn--pill-hc">4 Hours</button>
                        <button @click='duration = 42200' :class='dur42200' class="btn btn--pill btn--pill-hc">12 Hours</button>
                        <button @click='duration = 84400' :class='dur84400' class="btn btn--pill btn--pill-hr">24 Hours</button>
                    </div>
                </div>

                <template v-if='station.error'>
                    <div class='w-full align-center'>
                        <span v-text='station.error' class='txt-h3 color-red'></span>
                    </div>
                </template>
                <template v-else-if='station.loading'>
                    <div class='w-full h-full loading'></div>
                </template>

                <div v-if='dataType === "wind"' class='splitGrid'>
                    <div>
                        <canvas id="windspeed"></canvas>
                    </div>
                    <div>
                        <canvas id="wind_direction"></canvas>
                    </div>
                </div>
                
                <div v-if='dataType === "battery"'>
                    <canvas id="battery"></canvas>
                </div>
            </div>
        </template>
    </div>
</template>

<script>
// === Components ===
import Err from './components/Error.vue';

export default {
    name: 'app',
    data: function() {
        return {
            ws: false,
            mode: 'map',
            auth: false,
            duration: 300,
            dataType: 'wind',
            station: {
                id: false,
                error: false,
                loading: true,
                name: '',
                lon: 0,
                lat: 0,
                legend: {
                    windspeed: [],
                    winddir: []
                }
            },
            stations: {
                type: 'FeatureCollection',
                features: []
            },
            error: {
                title: '',
                body: ''
            },
            credentials: {
                map: 'pk.eyJ1IjoiaW5nYWxscyIsImEiOiJjam42YjhlMG4wNTdqM2ttbDg4dmh3YThmIn0.uNAoXsEXts4ljqf2rKWLQg'
            },
            map: false,
            charts: {
                windspeed: false,
                windspeedData: false,
                wind_direction: false,
                windDirectionData: false,
                battery: false,
                batteryData: false
            },
            modal: false,
            timer: false
        }
    },
    components: { },
    computed: {
        mapmode: function() {
            return {
                none: this.mode === 'list'
            };
        },
        loading: function() {
            return {
                loading: this.station.loading,
                none: this.station.error
            }
        },
        modeWind: function() {
            return {
                'btn--stroke': this.dataType === "wind"
            }
        },
        modeBattery: function() {
            return {
                'btn--stroke': this.dataType === "battery"
            }
        },
        dur300: function() {
            return {
                'btn--stroke': this.duration === 300
            }
        },
        dur3600: function() {
            return {
                'btn--stroke': this.duration === 3600
            }
        },
        dur14400: function() {
            return {
                'btn--stroke': this.duration === 14400
            }
        },
        dur42200: function() {
            return {
                'btn--stroke': this.duration === 42200
            }
        },
        dur84400: function() {
            return {
                'btn--stroke': this.duration === 84400
            }
        },
        viewport: function() {
            return {
                'viewport-half': !!this.station.id,
                'viewport-full': !this.station.id
            };
        }
    },
    mounted: function(e) {
        if (!this.timer) {
            this.timer = setInterval(() => {
                this.chart_range();
            }, 1000);
        }

        if (window.location.hash) {
            const station = parseInt(window.location.hash.replace(/^#/, ''));

            if (isNaN(station)) {
                window.location.hash = '';
            } else {
                this.station.id = station;
            }
        }

        this.ws = new WebSocket(`ws://${window.location.hostname}:${window.location.port}`);

        this.ws.onmessage = (ev) => {
            if (!ev.data || !ev.data.length) return;

            let data = {}
            try {
                data = JSON.parse(ev.data);
            } catch (err) {
                console.error(err);
            }

            if (!data.id || data.id != this.station.id) return;

            if (Array.isArray(this.charts.windspeedData)) {
                this.charts.windspeedData.push({
                    x: new Date(data.timestamp * 1000),
                    y: data.wind_speed
                });
                for (const idx in this.charts.windspeed.options.annotation.annotations) {
                    this.charts.windspeed.options.annotation.annotations[idx].xMax = data.timestamp * 1000;
                }
            }
            if (Array.isArray(this.charts.windDirectionData)) {
                this.charts.windDirectionData.push({
                    x: new Date(data.timestamp * 1000),
                    y: data.wind_direction
                });
                for (const idx in this.charts.wind_direction.options.annotation.annotations) {
                    this.charts.wind_direction.options.annotation.annotations[idx].xMax = data.timestamp * 1000;
                }
            }
            if (Array.isArray(this.charts.batteryData)) {
                this.charts.batteryData.push({
                    x: new Date(data.timestamp * 1000),
                    y: data.battery
                });
            }

            //TODO: Trim the data arrays when it gets too long.

        };

        this.fetch_stations();
        this.map_init();
    },
    watch: {
        'duration': function() {
            this.station_update();
        },
        'station.id': function() {
            if (!this.station.id) window.location.hash = '';

            this.station_update();
        },
        'dataType': function() {
            this.station_update();
        }
    },
    methods: {
        chart_range: function() {
            for (const chart of [
                this.charts.windspeed,
                this.charts.wind_direction,
                this.charts.battery
            ]) {
                if (chart) {
                    chart.options.scales.xAxes[0].time.min = new Date(+new Date() - (this.duration * 1000));
                    chart.options.scales.xAxes[0].time.max = new Date();
                    chart.update();
                }
            }
        },
        station_update: function() {
            this.station.loading = true;
            
            this.fetch_station(this.station.id, () => {
                if (this.map) {
                    this.map.setCenter([this.station.lon, this.station.lat]);
                }

                window.location.hash = this.station.id;

                this.charts.windspeedData = this.station.data.map(entry => {
                    return {
                        x: new Date(entry.timestamp * 1000),
                        y: entry.windspeed
                    }
                });

                this.charts.windDirectionData = this.station.data.map(entry => {
                    return {
                        x: new Date(entry.timestamp * 1000),
                        y: entry.wind_direction
                    }
                });

                this.charts.batteryData = this.station.data.map(entry => {
                    return {
                        x: new Date(entry.timestamp * 1000),
                        y: entry.battery_level
                    }
                });

                const commonOptions = {
                    animation: {
                        duration: 0,
                        easing: 'linear'
                    },
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        yAxes: [{
                            ticks: {
                                beginAtZero: true
                            }
                        }],
                        xAxes: [{
                            type: 'time',
                            offset: false,
                            //bounds: 'data',
                            time: {
                                minUnit: 'minute',
                                //unitStepSize: 1,
                                min: new Date(+new Date() - (this.duration * 1000)),
                                max: new Date()
                            }
                        }]
                    },
                    annotation: {
                        annotations: [],
                        drawTime: 'beforeDatasetsDraw'
                    }
                }

                let wsElem = document.getElementById('windspeed');
                if (wsElem) {
                    this.charts.windspeed = new Chart(wsElem, {
                        type: 'line',
                        data: {
                            datasets: [{
                                label: 'WindSpeed',
                                pointBackgroundColor: 'black',
                                pointBorderColor: 'black',
                                pointRadius: 0,
                                borderColor: 'black',
                                borderJoinStyle: 'round',
                                borderWidth: 0.2,
                                fill: 'end',
                                backgroundColor: 'rgba(255, 255, 255, 0.7)',
                                data: this.charts.windspeedData,
                                lineTension: 0
                            }]
                        },
                        options: JSON.parse(JSON.stringify(commonOptions))
                    });
                }
                
                let wdElem = document.getElementById('wind_direction');
                if (wdElem) {
                    let wdOpts = JSON.parse(JSON.stringify(commonOptions));
                    wdOpts.scales.yAxes[0].ticks.max = 360;
                    wdOpts.scales.yAxes[0].ticks.stepSize = 45;
                    const windNames = 
                    {
                        0: 'N',
                        45: 'NE',
                        90: 'E',
                        135: 'SE',
                        180: 'S',
                        225: 'SW',
                        270: 'W',
                        315: 'NW',
                        360: 'N'
                    }
                    wdOpts.scales.yAxes[0].ticks.callback = value => {
                        if (windNames[value] !== undefined)
                            return windNames[value];
                        else
                            return value;
                    }
                    
                    this.charts.wind_direction = new Chart(wdElem, {
                        type: 'line',
                        data: {
                            datasets: [{
                                label: 'Wind Direction',
                                pointBackgroundColor: 'black',
                                pointBorderColor: 'black',
                                pointRadius: 2  ,
                                borderColor: 'black',
                                showLine: false,
                                fill: false,
                                data: this.charts.windDirectionData,
                                lineTension: 0
                            }]
                        },
                        options: wdOpts
                    });
                }
                
                let battElem = document.getElementById('battery');
                if (battElem) {
                    let battOpts = JSON.parse(JSON.stringify(commonOptions));
                    battOpts.scales.yAxes[0].ticks.beginAtZero = false;
                    battOpts.scales.yAxes[0].ticks.min = 10;
                    battOpts.scales.yAxes[0].ticks.max = 15;
                    this.charts.battery = new Chart(battElem, {
                        type: 'line',
                        data: {
                            datasets: [{
                                label: 'Battery Level',
                                pointBackgroundColor: 'black',
                                pointBorderColor: 'black',
                                pointRadius: 0,
                                borderColor: 'black',
                                fill: false,
                                data: this.charts.batteryData,
                                lineTension: 0
                            }]
                        },
                        options: battOpts
                    });
                }

                this.station.loading = false;

                this.set_speed_annotations();
                this.set_direction_annotations();
            });
        },
        map_init: function() {
            try {
                mapboxgl.accessToken = this.credentials.map;

                this.map = new mapboxgl.Map({
                    hash: false,
                    container: 'map',
                    attributionControl: false,
                    style: 'mapbox://styles/mapbox/satellite-streets-v11',
                    center: [-118.41064453125, 37.3461426132468],
                    zoom: 11
                }).addControl(new mapboxgl.AttributionControl({
                    compact: true
                }));
            } catch (err) {
                // Mapbox GL was not able to be created, show list by default
                this.mode = 'list';
            }

            if (!this.map) return;

            this.map.on('style.load', () => {
                this.map.addLayer({
                    id: 'stations',
                    type: 'circle',
                    source: {
                        type: 'geojson',
                        data: this.stations
                    },
                    paint: {
                        'circle-color': '#51bbd6',
                        'circle-radius': 10
                    }
                });

                this.map.fitBounds(this.stations.bbox);
            });

            this.map.on('click', (e) => {
                // set bbox as 5px reactangle area around clicked point
                const bbox = [[e.point.x - 5, e.point.y - 5], [e.point.x + 5, e.point.y + 5]];
                const feats = this.map.queryRenderedFeatures(bbox, {
                    layers: ['stations']
                });

                if (!feats.length) return;

                this.station.id = feats[0].id;
            });
        },
        set_direction_annotations: function() {
            if (!this.station.legend.winddir) {
                return;
            }
            
            let minX = this.charts.wind_direction.options.scales.xAxes[0].time.min;
            let maxX = this.charts.wind_direction.options.scales.xAxes[0].time.max;
            if (Array.isArray(this.charts.windDirectionData) && this.charts.windDirectionData.length > 0)
            {
                minX = this.charts.windDirectionData[0].x;
                maxX = this.charts.windDirectionData[this.charts.windDirectionData.length - 1].x;
            }
                      
            for (const entry of this.station.legend.winddir) {
                if (entry.start === undefined 
                    || entry.end === undefined
                    || entry.color === undefined) {
                    continue;
                }
                let subEntries = [entry];
                if (entry.start > entry.end) {
                    subEntries = [ {
                        start: 0,
                        end: entry.end,
                        color: entry.color
                    }, {
                        start: entry.start,
                        end: 360,
                        color: entry.color
                    }];
                }
                for (const subEntry of subEntries) {
                    this.charts.wind_direction.options.annotation.annotations.push({
                        type: 'box',
                        xScaleID: 'x-axis-0',
                        yScaleID: 'y-axis-0',
                        yMin: subEntry.start,
                        yMax: subEntry.end,
                        xMin: minX,
                        xMax: maxX,
                        backgroundColor: subEntry.color,
                        borderColor: 'rgba(0,0,0,0)',
                    });
                }
            }
        },
        set_speed_annotations: function() {
            let lastVal = 0;
            const Xs = this.charts.windspeedData.map(wsd => {
                return +wsd.x;
            });
            const maxX = Math.max(...Xs);
            const minX = Math.min(...Xs);
            let last = {};
            for (const entry of this.station.legend.windspeed)
            {
                last = {  
                    type: 'box',
                    xScaleID: 'x-axis-0',
                    yScaleID: 'y-axis-0',
                    yMin: lastVal,
                    yMax: entry.top,
                    xMax: maxX,
                    xMin: minX,
                    backgroundColor: entry.color,
                    borderColor: 'rgba(0,0,0,0)',
                };
                this.charts.windspeed.options.annotation.annotations.push(last);
                lastVal = entry.top;
            }
            //duplicate last and have it go to the moon:
            //The code below causes our Y-axis to go to 100 as soon as this is shown. We need to take manual control of the Y-axis if we want this to work.
            /*last = JSON.parse(JSON.stringify(last));
            last.yMin = last.yMax;
            last.yMax = null;
            this.charts.windspeed.options.annotation.annotations.push(last);*/
        },
        fetch_stations: function() {
            fetch(`${window.location.protocol}//${window.location.host}/api/stations`, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                return response.json();
            }).then((stations) => {
                this.stations = stations;
            });
        },
        fetch_station: function(station_id, cb) {
            this.station.error = '';

            if (!station_id) {
                this.station.error = 'No Station ID Specified';
                return;
            }
            
            let stationFetch = fetch(`${window.location.protocol}//${window.location.host}/api/station/${station_id}`, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                if (response.ok) {
                    return response.json();
                } else if (response.status === 404) {
                    throw new Error('Station Not Found');
                } else {
                    throw new Error('Could Not Retreive Station');
                }
            }).then((station) => {
                this.station.name = station.name;
                this.station.lon = station.lon;
                this.station.lat = station.lat;

                if (!!station.windspeedlegend) {
                    this.station.legend.windspeed.splice(0, this.station.legend.windspeed.length);
                    station.windspeedlegend.forEach((legend) => {
                        this.station.legend.windspeed.push(legend)
                    });
                }
                
                if (!!station.winddirlegend) {
                    this.station.legend.winddir.splice(0, this.station.legend.winddir.length);

                    station.winddirlegend.forEach((legend) => {
                        this.station.legend.winddir.push(legend);
                    });
                }
            }).catch((err) => {
                this.station.error = err.message;
            });

            let url = new URL(`${window.location.protocol}//${window.location.host}/api/station/${station_id}/data`);

            let current = +new Date() / 1000; // Unix time (seconds)

            // current (seconds) - ( 60 (seconds) * 60 (minutes) )
            url.searchParams.append('start', Math.floor(current - this.duration));
            url.searchParams.append('end', Math.floor(current));
            
            //Sample rate. Let's aim for 2000 points.
            let desiredSample = this.duration / 2000;
            if (desiredSample < 1)
                desiredSample = 1;
            url.searchParams.append('sample', desiredSample);

            let dataFetch = fetch(url, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                if (response.ok) {
                    return response.json();
                } else if (response.status === 404) {
                    throw new Error('Station Not Found');
                } else {
                    throw new Error('Could Not Retreive Station');
                }
            }).then(stationData => {
                this.station.data = stationData;
            }).catch((err) => {
                this.station.error = err.message;
            });

            Promise.all([stationFetch, dataFetch]).then(() => {
                return cb();
            });
        },
        fetch_station_latest: function(station_id, cb) {
            if (!station_id) return;

            fetch(`${window.location.protocol}//${window.location.host}/api/station/${station_id}/data/latest`, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                return response.json();
            }).then((dataPoint) => {
                return cb(dataPoint)
            })
        }
    }
}
</script>
