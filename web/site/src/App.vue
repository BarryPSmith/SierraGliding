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
            <div class='relative scroll-hidden headerGrid'
                :class='graphViewport'>
                <div class='py12 px12 clearfix border-t border--gray-light'>
                    <h1 class='txt-h2 fl' v-text='station.name'></h1>

                    <button @click='station.id = false' class='btn btn--stroke btn--gray round fr h36'><svg class='icon'><use href='#icon-close'/></svg></button>
                    <button @click='fullSize = !fullSize' class='btn btn--stroke btn--gray round fr h36'><svg class='icon'><use href='#icon-viewport'/></svg></button>

                    <div v-if='!station.error' class="flex-parent-inline fr px24">
                        <button @click='dataType = "wind"' :class='modeWind' class="btn btn--pill btn--pill-hl">Wind</button>
                        <button @click='dataType = "battery"' :class='modeBattery' class="btn btn--pill btn--pill-hr">Battery</button>
                    </div>
                    
                    <div v-if='!station.error' class="flex-parent-inline fr px24">
                    
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

                <div v-show='dataType === "wind"' class='splitGrid'>
                    <div>
                        <canvas id="windspeed"></canvas>
                    </div>
                    <div>
                        <canvas id="wind_direction"></canvas>
                    </div>
                </div>
                
                <div v-show='dataType === "battery"'>
                    <canvas id="battery"></canvas>
                </div>
            </div>
        </template>
    </div>
</template>

<script>
// === Components ===
import Err from './components/Error.vue';
import DataManager from './DataManager.js';
import bs from '../node_modules/binary-search/index.js';

export default {
    name: 'app',
    data: function() {
        return {
            ws: false,
            mode: 'map',
            auth: false,
            duration: 300,
            chartEnd: null,
            isPanning: false,
            dataType: 'wind',
            fullSize: false,
            dataManager: null,
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
                wind_direction: false,
                battery: false,
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
        graphViewport: function() {
            return {
                'viewport-half': !this.fullSize,
                'viewport-almost': !!this.fullSize
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
                'viewport-slight': (!!this.station.id && !!this.fullSize),
                'viewport-half': (!!this.station.id && !this.fullSize),
                'viewport-full': !this.station.id
            };
        }
    },
    mounted: function(e) {
        if (!this.timer) {
            this.timer = setInterval(() => {
                for (const chart of [
                    this.charts.windspeed,
                    this.charts.wind_direction,
                    this.charts.battery
                ]) {
                    if (chart) {
                        chart.options.plugins.zoom.pan.rangeMax.x = new Date();
                    }
                }
                /*if (!this.chartEnd && !this.isPanning) {
                    this.chart_range();
                }*/
            }, 1000);
        }

        var annotationPlugin = require('../node_modules/chartjs-plugin-annotation/chartjs-plugin-annotation.js');
        Chart.plugins.register(annotationPlugin);
        
        var h = Math.max(document.documentElement.clientHeight, window.innerHeight || 0);
        if (h < 600)
            this.fullSize = true;

        if (window.location.hash) {
            const station = parseInt(window.location.hash.replace(/^#/, ''));

            if (isNaN(station)) {
                window.location.hash = '';
            } else {
                this.station.id = station;
            }
        }

        let protocol = 'ws';
        if (window.location.protocol=='https:')
            protocol = 'wss';
        this.ws = new WebSocket(`${protocol}://${window.location.hostname}:${window.location.port}`);

        this.ws.onmessage = this.socket_onMessage;

        this.fetch_stations(() => { 
            //If there's only one station (which there will be until we build some more), then select that:
            if (this.stations.features.length == 1) {
                this.station.id = this.stations.features[0].id;
            }
        });
        this.map_init();
    },
    watch: {
        'duration': function () {
            const dmPromise = this.dataManager.ensure_data(
                this.cur_start(), this.chartEnd
            );
            this.chart_range();
            this.station.loading = true;
            dmPromise.then(obj => {
                this.update_annotation_ranges();
                if (obj.anyChange) {
                    this.set_windspeed_range();
                    this.station_data_update(obj.reloadedData);
                }
                this.station.loading = false;
                this.station_data_update();
            });
        },
        'station.id': function() {
            if (!this.station.id) {
                this.charts.windspeed = false;
                this.charts.wind_direction = false;
                this.charts.battery = false;
                window.location.hash = '';
            }
            else
            {
                this.dataManager = new DataManager(this.station.id);
            }
            
            this.station_update();
        },
        'dataType': function() {
            this.station_update(true);
            this.chart_range();
        }
    },
    methods: {
        socket_onMessage: function (ev) {
            if (!ev.data || !ev.data.length) return;

            let data = {}
            try {
                data = JSON.parse(ev.data);

                if (!data.id || data.id != this.station.id) return;
                
                this.dataManager.push_if_appropriate(data);
                this.update_annotation_ranges();
                if (!this.chartEnd && !this.isPanning) {
                    this.chart_range();
                } else {
                    this.station_data_update(false);
                }
            } catch (err) {
                console.error(err);
            }
        },

        set_windspeed_range: function () {
            const dataset = this.dataManager.windspeedData;
            const chart = this.charts.windspeed;
            const start = this.cur_start();
            const end = this.cur_end();
            const minMax = this.station.legend.windspeed[this.station.legend.windspeed.length - 1].top / (this.dataManager.unit == 'mph' ? 1.6 : 1.0);
            let startIdx = bs(dataset, start, (element, needle) => element.x - needle);
            let endIdx = bs(dataset, end, (element, needle) => element.x - needle);
            if (startIdx < 0)
                startIdx = ~startIdx;
            if (endIdx < 0)
                endIdx = ~endIdx;

            let max = minMax
            for (let i = startIdx; i < endIdx && i < dataset.length; i++) {
                if (dataset[i].y > max)
                    max = dataset[i].y;
            }
            max = 5 * Math.ceil(max / 5);
            chart.options.scales.yAxes[0].ticks.max = max;
        },

        cur_end: function () {
            if (this.chartEnd)
                return this.chartEnd;
            else
                return new Date();
        },

        cur_start: function () {
            return this.cur_end() - this.duration * 1000;
        },

        chart_range: function (chartToExclude) {
            const charts = this.dataType == 'wind' ?
                [
                this.charts.windspeed,
                this.charts.wind_direction,
                ] : [
                this.charts.battery
                ];

            if (this.dataType == 'wind')
                this.set_windspeed_range(); 
            for (const chart of charts) {
                if (chart && chart != chartToExclude) {
                    chart.options.scales.xAxes[0].time.min = this.cur_start();
                    chart.options.scales.xAxes[0].time.max = this.cur_end();
                    chart.update();
                }
            }               
        },

        station_data_update: function (refreshRequired) {
            if (refreshRequired) {
                if (this.charts.windspeed && this.dataType == 'wind') {
                    this.charts.windspeed.data.datasets[0].data = this.dataManager.windspeedData;
                }
                if (this.charts.wind_direction && this.dataType == 'wind') {
                    this.charts.wind_direction.data.datasets[0].data = this.dataManager.windDirectionData;
                }
                if (this.charts.battery && this.dataType == 'battery') {
                    this.charts.battery.data.datasets[0].data = this.dataManager.batteryData;
                }
            }
            
            if (this.charts.windspeed && this.dataType == 'wind') {
                this.charts.windspeed.update();
            }
            if (this.charts.wind_direction && this.dataType == 'wind') {
                this.charts.wind_direction.update();
            }
            if (this.charts.battery && this.dataType == 'battery') {
                this.charts.battery.update();
            }
        },

        station_update: function() {
            this.station.loading = true;

            const dmPromise = this.dataManager.ensure_data(
                this.cur_start(), this.chartEnd
            );
            
            this.fetch_station(this.station.id, () => {
                if (this.map) {
                    this.map.setCenter([this.station.lon, this.station.lat]);
                }

                window.location.hash = this.station.id;
                
                dmPromise.then(() => {
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
                                    //beginAtZero: true
                                    min: 0
                                },
                                position: 'right'
                            }],
                            xAxes: [{
                                type: 'time',
                                offset: false,
                                ticks: {
                                    maxRotation: 0
                                },
                                time: {
                                    minUnit: 'minute',
                                    min: this.cur_start(),
                                    max: this.cur_end()
                                }
                            }]
                        },
                        annotation: {
                                annotations: [],
                                drawTime: 'beforeDatasetsDraw'
                        },
                        
                        plugins: {
                            zoom: {
                                pan: {
                                    enabled: true,
                                    mode: 'x',
                                    rangeMax: {
                                        x: new Date()
                                    },
                                }
                            }
                        }
                    }

                    let wsElem = document.getElementById('windspeed');
                    if (wsElem && (this.dataType == 'wind')) {
                        let wsOpts = JSON.parse(JSON.stringify(commonOptions));
                        //Callbacks don't survive stringify/parse
                        wsOpts.plugins.zoom.pan.onPan = this.chart_panning;
                        wsOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                        wsOpts.scales.yAxes[0].ticks.stepSize = 5;
                        const wsData = {
                            datasets: [{
                                label: 'WindSpeed (' + this.dataManager.unit + ')',
                                pointBackgroundColor: 'black',
                                pointBorderColor: 'black',
                                pointRadius: 0,
                                borderColor: 'black',
                                borderJoinStyle: 'round',
                                borderWidth: 0.2,
                                fill: 'end',
                                backgroundColor: 'rgba(255, 255, 255, 0.7)',
                                data: this.dataManager.windspeedData,
                                lineTension: 0
                            }]
                        };
                        if (!this.charts.windspeed) {
                            this.charts.windspeed = new Chart(wsElem, {
                                type: 'line',
                                data: wsData,
                                options: wsOpts
                                });
                            }
                        else {
                            this.charts.windspeed.data.datasets[0].data = this.dataManager.windspeedData;
                        }
                    }
                    
                    let wdElem = document.getElementById('wind_direction');
                    if (wdElem && (this.dataType == 'wind')) {
                        let wdOpts = JSON.parse(JSON.stringify(commonOptions));
                        wdOpts.plugins.zoom.pan.onPan = this.chart_panning;
                        wdOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                        wdOpts.scales.yAxes[0].ticks.max = 360;
                        wdOpts.scales.yAxes[0].ticks.stepSize = 45;
                        const windNames =  {
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

                        if (!this.charts.wind_direction) {
                            this.charts.wind_direction = new Chart(wdElem, {
                                type: 'line',
                                data: {
                                    datasets: [{
                                        label: 'Wind Direction',
                                        pointBackgroundColor: 'black',
                                        pointBorderColor: 'black',
                                        pointRadius: 2,
                                        borderColor: 'black',
                                        showLine: false,
                                        fill: false,
                                        data: this.dataManager.windDirectionData,
                                        lineTension: 0
                                    }]
                                },
                                options: wdOpts
                            });
                        } else {
                            this.charts.wind_direction.data.datasets[0].data = this.dataManager.windDirectionData;
                        }
                    }
                    
                    let battElem = document.getElementById('battery');
                    if (battElem && this.dataType == 'battery') {
                        let battOpts = JSON.parse(JSON.stringify(commonOptions));
                        battOpts.scales.yAxes[0].ticks.beginAtZero = false;
                        battOpts.scales.yAxes[0].ticks.min = this.station.batteryRange.min;
                        battOpts.scales.yAxes[0].ticks.max = this.station.batteryRange.max;
                        battOpts.plugins.zoom.pan.onPan = this.chart_panning;
                        battOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                        if (!this.charts.battery) {
                            this.charts.battery = new Chart(battElem, {
                                type: 'line',
                                data: {
                                    datasets: [{
                                        label: 'Battery Level',
                                        pointBackgroundColor: 'black',
                                        pointBorderColor: 'black',
                                        pointRadius: 0,
                                        borderColor: 'black',
                                        borderJoinStyle: 'round',
                                        fill: false,
                                        data: this.dataManager.batteryData,
                                        lineTension: 0
                                    }]
                                },
                                options: battOpts
                            });
                        } else {
                            this.charts.battery.options.scales.yAxes[0].ticks.min = this.station.batteryRange.min;
                            this.charts.battery.options.scales.yAxes[0].ticks.max = this.station.batteryRange.max;
                            this.charts.battery.data.datasets[0].data = this.dataManager.batteryData;
                        }
                    }

                    this.station.loading = false;
                    if (this.dataType == 'wind') {
                        this.set_speed_annotations();
                        this.set_direction_annotations();
                        this.update_annotation_ranges();
                        this.set_windspeed_range();
                    }
                    //Ensure update() is called on all of our charts:
                    this.station_data_update(false);
                });
            });
        },

        chart_panning: function ({ chart }) {
            this.isPanning = true;
            const chartMax = chart.options.scales.xAxes[0].ticks.max;
            if (new Date() - chartMax < 10 * 1000)
                this.chartEnd = null;
            else
                this.chartEnd = chartMax;
            this.dataManager.ensure_data(this.cur_start(), this.cur_end())
                .then(obj => {
                    if (obj.anyChange) {
                        this.set_windspeed_range();
                        this.update_annotation_ranges();
                        this.station_data_update(obj.reloadedData);
                    }
            });
            this.chart_range(chart);
        },

        chart_panComplete: function ({ chart }) {
            this.isPanning = false;
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

            this.charts.wind_direction.options.annotation.annotations = [];
                      
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
                        //xMin: minX,
                        //xMax: maxX,
                        backgroundColor: subEntry.color,
                        borderColor: 'rgba(0,0,0,0)',
                    });
                }
            }
        },
        set_speed_annotations: function() {
            let lastVal = 0;
            const factor = this.dataManager.unit == 'mph' ? 1.6 : 1.0;
            let last = {};
            this.charts.windspeed.options.annotation.annotations = [];
            for (const entry of this.station.legend.windspeed)
            {
                last = {  
                    type: 'box',
                    xScaleID: 'x-axis-0',
                    yScaleID: 'y-axis-0',
                    yMin: lastVal / factor,
                    yMax: entry.top / factor,
                    //xMax: maxX,
                    //xMin: minX,
                    backgroundColor: entry.color,
                    borderColor: 'rgba(0,0,0,0)',
                };
                this.charts.windspeed.options.annotation.annotations.push(last);
                lastVal = entry.top;
            }
            //duplicate last and have it go to the moon:
            //The code below causes our Y-axis to go to 100 as soon as this is shown. We need to take manual control of the Y-axis if we want this to work.
            last = JSON.parse(JSON.stringify(last));
            last.yMin = last.yMax;
            last.yMax = null;
            this.charts.windspeed.options.annotation.annotations.push(last);
            //this.charts.windspeed.options.scales.yAxes[0].max = last.yMin;
        },
        update_annotation_ranges: function () {
            if (this.dataManager.stationData.length == 0)
                return;

            const minX = this.dataManager.stationData[0].timestamp * 1000;
            const maxX = this.dataManager.stationData[this.dataManager.stationData.length - 1].timestamp * 1000;

            if (this.charts.windspeed) {
                for (const idx in this.charts.windspeed.options.annotation.annotations) {
                    this.charts.windspeed.options.annotation.annotations[idx].xMin = minX;
                    this.charts.windspeed.options.annotation.annotations[idx].xMax = maxX;
                }
            }
            if (this.charts.wind_direction) {
                for (const idx in this.charts.wind_direction.options.annotation.annotations) {
                    this.charts.wind_direction.options.annotation.annotations[idx].xMin = minX;
                    this.charts.wind_direction.options.annotation.annotations[idx].xMax = maxX;
                }
            }
        },

        fetch_stations: function(cb) {
            fetch(`${window.location.protocol}//${window.location.host}/api/stations`, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                return response.json();
            }).then((stations) => {
                this.stations = stations;
                
                if (cb) {
                    return cb();
                }
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
				this.station.error = station.statusMessage;

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

                this.station.batteryRange = station.batteryRange;
            }).catch((err) => {
                this.station.error = err.message;
            });

            Promise.all([stationFetch ]).then(() => {
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
