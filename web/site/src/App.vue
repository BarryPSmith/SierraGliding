<template>
    <div class='viewport-full'>
        <div v-bind:class='viewport' class='relative scroll-hidden'>
            <!-- Map -->
            <div id="map" class='h-full w-full bg-darken10'></div>
        </div>

        <template v-if='station.id'>
            <div class='py12 px12 clearfix'>
                <h1 class='txt-h2 fl' v-text='station.name'></h1>

                <button @click='station.id = false' class='btn btn--stroke--gray round fr'><svg class='icon'><use href='#icon-close'/></svg></button>

                <div class="flex-parent-inline fr px24">
                    <button class="btn btn--pill btn--pill-hl">1 Hour</button>
                    <button class="btn btn--pill btn--pill-hc">4 Hours</button>
                    <button class="btn btn--pill btn--pill-hc">12 Hours</button>
                    <button class="btn btn--pill btn--pill-hr">24 Hours</button>
                </div>
            </div>

            <div class='viewport-half relative scroll-hidden'>
                <canvas id="windspeed" class='w-full' height="400"></canvas>
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
            station: {
                id: false,
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
                windspeed: false
            },
            modal: false
        }
    },
    components: { },
    computed: {
        viewport: function() {
            return {
                'viewport-half': !!this.station.id,
                'viewport-full': !this.station.id
            };
        }
    },
    mounted: function(e) {
        mapboxgl.accessToken = this.credentials.map;

        this.ws = new WebSocket(`ws://${window.location.hostname}:40510`);
        this.ws.onopen = () => {
            console.error('connected');
        };
        this.ws.onmessage = (ev) => {
            console.log(ev);
        };

        this.fetch_stations();

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
    watch: {
        'station.id': function() {
            this.fetch_station(this.station.id, () => {
                this.map.setCenter([this.station.lon, this.station.lat]);

                this.charts.windspeed = new Chart(document.getElementById('windspeed'), {
                    type: 'line',
                    data: {
                        datasets: [{
                            label: 'WindSpeed',
                            pointBackgroundColor: 'black',
                            pointBorderColor: 'black',
                            borderColor: 'black',
                            fill: false,
                            data: [{
                                y: 5,
                                x: new Date('2019-09-30T02:38:46Z')
                            },{
                                y: 10,
                                x: new Date('2019-09-30T02:38:49Z')
                            },{
                                y: 20,
                                x: new Date('2019-09-30T02:38:52Z')
                            },{
                                y: 24,
                                x: new Date('2019-09-30T02:38:55Z')
                            },{
                                y: 23,
                                x: new Date('2019-09-30T02:38:58Z')
                            },{
                                y: 12,
                                x: new Date('2019-09-30T02:39:01')
                            }]
                        }]
                    },
                    options: {
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
                                    min: new Date('2019-09-30T02:38:46Z'),
                                    max: new Date('2019-09-30T02:39:04Z')
                                }
                            }]
                        },
                        annotation: {
                            annotations: []
                        }
                    }
                });

                if (this.station.legend.windspeed && this.station.legend.windspeed.length === 3) {
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
                }
            });
        }
    },
    methods: {
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

            fetch(`${window.location.protocol}//${window.location.host}/api/station/${station_id}`, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                return response.json();
            }).then((station) => {
                this.station.name = station.name;
                this.station.legend.windspeed = station.windspeedlegend;
                this.station.legend.winddir = station.winddirlegend;
                this.station.lon = station.lon;
                this.station.lat = station.lat;

                return cb();
            });
        }
    }
}
</script>
