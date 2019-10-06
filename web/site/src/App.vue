<template>
    <div class='viewport-full'>
        <div v-bind:class='viewport' class='relative scroll-hidden'>
            <!-- Map -->
            <template v-if='map'>
                <div id="map" class='h-full w-full bg-darken10'></div>
            </template>
            <template v-else>
                <div class='h-full w-full'>
                    <template v-for='station_feat in stations.features'>
                        <div @click='station.id = station_feat.id' class='cursor-pointer py12 px12 col col--12'>
                            <span class='txt-h3' v-text='station_feat.properties.name'></span>
                        </div>
                    </template>
                </div>
            </template>
        </div>

        <template v-if='station.id'>
            <div class='viewport-half relative scroll-hidden'>
                <div class='py12 px12 clearfix'>
                    <h1 class='txt-h2 fl' v-text='station.name'></h1>

                    <button @click='station.id = false' class='btn btn--stroke btn--gray round fr h36'><svg class='icon'><use href='#icon-close'/></svg></button>

                    <div class="flex-parent-inline fr px24">
                        <button @click='duration =  1' :class='dur1' class="btn btn--pill btn--pill-hl">1 Hour</button>
                        <button @click='duration =  4' :class='dur4' class="btn btn--pill btn--pill-hc">4 Hours</button>
                        <button @click='duration = 12' :class='dur12' class="btn btn--pill btn--pill-hc">12 Hours</button>
                        <button @click='duration = 24' :class='dur24' class="btn btn--pill btn--pill-hr">24 Hours</button>
                    </div>
                </div>

                <template v-if='station.loading'>
                    <div class='w-full h-full loading'></div>
                </template>

                <div :class='{ none: loading }'class='grid grid--gut12'>
                    <div class='col col--12'>
                        <canvas id="windspeed"></canvas>
                    </div>
                    <div class='col col--12'>
                        <canvas id="wind_direction"></canvas>
                    </div>

                    <div class='col col--12'>
                        <canvas id="battery"></canvas>
                    </div>
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
            auth: false,
            duration: 1,
            station: {
                id: false,
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
        dur1: function() {
            return {
                'btn--stroke': this.duration === 1
            }
        },
        dur4: function() {
            return {
                'btn--stroke': this.duration === 4
            }
        },
        dur12: function() {
            return {
                'btn--stroke': this.duration === 12
            }
        },
        dur24: function() {
            return {
                'btn--stroke': this.duration === 24
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
            }
            if (Array.isArray(this.charts.windDirectionData)) {
                this.charts.windDirectionData.push({
                    x: new Date(data.timestamp * 1000),
                    y: data.wind_direction
                });
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
                    chart.options.scales.xAxes[0].time.min = new Date(+new Date() - (this.duration * 60 * 60 * 1000));
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
                            bounds: 'data',
                            time: {
                                min: new Date(+new Date() - (this.duration * 60 * 60 * 1000)),
                                max: new Date()
                            }
                        }]
                    },
                    annotation: {
                        annotations: []
                    }
                }

                this.charts.windspeed = new Chart(document.getElementById('windspeed'), {
                    type: 'line',
                    data: {
                        datasets: [{
                            label: 'WindSpeed',
                            pointBackgroundColor: 'black',
                            pointBorderColor: 'black',
                            borderColor: 'black',
                            fill: false,
                            data: this.charts.windspeedData,
                            lineTension: 0
                        }]
                    },
                    options: JSON.parse(JSON.stringify(commonOptions))
                });

                this.charts.wind_direction = new Chart(document.getElementById('wind_direction'), {
                    type: 'line',
                    data: {
                        datasets: [{
                            label: 'Wind Direction',
                            pointBackgroundColor: 'black',
                            pointBorderColor: 'black',
                            borderColor: 'black',
                            fill: false,
                            data: this.charts.windDirectionData,
                            lineTension: 0
                        }]
                    },
                    options: JSON.parse(JSON.stringify(commonOptions))
                });

                this.charts.battery = new Chart(document.getElementById('battery'), {
                    type: 'line',
                    data: {
                        datasets: [{
                            label: 'Battery Level',
                            pointBackgroundColor: 'black',
                            pointBorderColor: 'black',
                            borderColor: 'black',
                            fill: false,
                            data: this.charts.batteryData,
                            lineTension: 0
                        }]
                    },
                    options: JSON.parse(JSON.stringify(commonOptions))
                });

                this.station.loading = false;

                this.set_speed_annotations();
            });
        },
        map_init: function() {
            try {
                mapboxgl.accessToken = this.credentials.map;

                this.map = new mapboxgl.Map({
                    hash: true,
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
        set_speed_annotations: function() {
            if (
                !this.station.legend.windspeed
                || this.station.legend.windspeed.length !== 3
            ) return;

            this.charts.windspeed.options.annotation.annotations.push({
                id: 'blue',
                type: 'box',
                xScaleID: 'x-axis-0',
                yScaleID: 'y-axis-0',
                yMin: 0,
                yMax: this.station.legend.windspeed[0],
                backgroundColor: 'rgba(0,0,255,0.4)',
                borderColor: 'rgba(0,0,255,0.4)',
            });

            this.charts.windspeed.options.annotation.annotations.push({
                id: 'green',
                type: 'box',
                xScaleID: 'x-axis-0',
                yScaleID: 'y-axis-0',
                yMin: this.station.legend.windspeed[0],
                yMax: this.station.legend.windspeed[1],
                backgroundColor: 'rgba(0,255,0,0.4)',
                borderColor: 'rgba(0,255,0,0.4)',
            });

            this.charts.windspeed.options.annotation.annotations.push({
                id: 'yellow',
                type: 'box',
                xScaleID: 'x-axis-0',
                yScaleID: 'y-axis-0',
                yMin: this.station.legend.windspeed[1],
                yMax: this.station.legend.windspeed[2],
                backgroundColor: 'rgba(255,255,0,0.4)',
                borderColor: 'rgba(255,255,0,0.4)',
            });

            this.charts.windspeed.options.annotation.annotations.push({
                id: 'red',
                type: 'box',
                xScaleID: 'x-axis-0',
                yScaleID: 'y-axis-0',
                yMin: this.station.legend.windspeed[2],
                yMax: 30,
                backgroundColor: 'rgba(255,0,0,0.4)',
                borderColor: 'rgba(255,0,0,0.4)',
            });
        },
        fetch_stations: function() {
            fetch(`${window.location.protocol}//${window.location.host}/api/stations`, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                return response.json();
            }).then((stations) => {
                this.stations = {
                    type: 'FeatureCollection',
                    features: stations.map((station) => {
                        return {
                            id: station.id,
                            type: 'Feature',
                            properties: {
                                name: station.name
                            },
                            geometry: {
                                type: 'Point',
                                coordinates: [
                                    station.lon,
                                    station.lat
                                ]
                            }
                        }
                    })
                }
            });
        },
        fetch_station: function(station_id, cb) {
            if (!station_id) return;

            let stationFetch = fetch(`${window.location.protocol}//${window.location.host}/api/station/${station_id}`, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                return response.json();
            }).then((station) => {
                this.station.name = station.name;
                this.station.lon = station.lon;
                this.station.lat = station.lat;

                this.station.legend.windspeed.splice(0, this.station.legend.windspeed.length);
                station.windspeedlegend.forEach((legend) => {
                    this.station.legend.windspeed.push(legend)
                });

                this.station.legend.winddir.splice(0, this.station.legend.winddir.length);

                station.winddirlegend.forEach((legend) => {
                    this.station.legend.winddir.push(legend);
                });
            });

            let url = new URL(`${window.location.protocol}//${window.location.host}/api/station/${station_id}/data`);

            let current = +new Date() / 1000; // Unix time (seconds)

            // current (seconds) - ( 60 (seconds) * 60 (minutes) )
            url.searchParams.append('start', String(Math.floor(current - (60 * 60))));
            url.searchParams.append('end', String(Math.floor(current)));

            let dataFetch = fetch(url, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                return response.json();
            }).then(stationData => {
                this.station.data = stationData;
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
