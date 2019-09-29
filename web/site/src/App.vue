<template>
    <div class='viewport-full'>
        <div v-bind:class='viewport' class='relative scroll-hidden'>
            <!-- Map -->
            <div id="map" class='h-full bg-darken10 viewport-twothirds viewport-full-ml absolute top left right bottom'></div>
        </div>

        <template v-if='station.id'>
            <div class='viewport-half relative scroll-hidden'>
                HELLO
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
            auth: false,
            station: {
                id: false,
                name: '',
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
            return this.fetch_station(this.station.id);
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
        fetch_station: function(station_id) {
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
            });
        }
    }
}
</script>
