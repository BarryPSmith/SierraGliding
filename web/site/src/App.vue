<template>
    <div class='viewport-full'>
        <div v-bind:class='viewport' class='relative scroll-hidden'>
            <!-- Map -->
            <div id="map" class='h-full w-full bg-darken10'></div>
        </div>

        <template v-if='station.id'>
            <div class='viewport-half relative scroll-hidden'>
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

                <div style="position:absolute; top:80px; right:10px; left:10px; bottom: 10px">
                    <div style="height:-webkit-fill-available;">
                        <canvas id="windspeed"></canvas> <!--height="400"  class='w-full h-full' -->
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
                windspeed: false,
                windspeedData: false
            },
            modal: false,
            timer: false
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
            if (ev.data == this.station.id) {
                if (Array.isArray(this.charts.windspeedData)) {
                    this.fetch_station_latest(this.station.id, dataPoint => {
                        /*console.error(dataPoint);
                        console.error(dataPoint.timestamp);
                        console.error(dataPoint.timestamp * 1000);
                        console.error(new Date(dataPoint.timestamp * 1000));
                        console.error(new Date(dataPoint.timestamp));*/
                        this.charts.windspeedData.push({ x: new Date(dataPoint[0].timestamp * 1000), y: dataPoint[0].windspeed });
                        //console.error(this.charts.windspeedData.length);
                        //console.error(this.charts.windspeed.data.datasets[0].data);
                        //console.error(this.charts.windspeedData);
                        //TODO: Trim the windspeed data array when it gets too long.
                    });
                }
            }
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
                
                this.charts.windspeedData = this.station.data.map(entry => {
                    return {
                        x: new Date(entry.timestamp * 1000),
                        y: entry.windspeed
                }});
                
                if (!this.Timer) {
                    this.Timer = setInterval(() => {
                        if (this.charts.windspeed) {
                            this.charts.windspeed.options.scales.xAxes[0].time.min = new Date(new Date() - 180000);
                            this.charts.windspeed.options.scales.xAxes[0].time.max = new Date();
                            this.charts.windspeed.update();
                        }
                    }, 1000);
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
                            data: this.charts.windspeedData
                        }]
                    },
                    options: {
                        animation: { 
                            duration: 0,
                            easing: "linear"
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
                                    min: new Date(new Date() - 180000),
                                    max: new Date()
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

            let stationFetch = fetch(`${window.location.protocol}//${window.location.host}/api/station/${station_id}`, {
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
            })
            
            let url = new URL(`${window.location.protocol}//${window.location.host}/api/station/${station_id}/data`);
            
            url.searchParams.append('start', new Date(new Date() - 180000));
            url.searchParams.append('end', new Date());
            
            let dataFetch = fetch(url, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                return response.json();
            }).then(stationData => {
                this.station.data = stationData;
            });
            
            Promise.all([stationFetch, dataFetch])
                .then(() => {
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
