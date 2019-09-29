<template>

    <div class='viewport-full relative scroll-hidden'>
        <!-- Map -->
        <div id="map" class='h-full bg-darken10 viewport-twothirds viewport-full-ml absolute top left right bottom'></div>
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
            stations: {
                type: 'FeatureCollection',
                features: []
            },
            error: {
                title: '',
                body: ''
            },
            credentials: {
                map: 'pk.eyJ1IjoiaW5nYWxscyIsImEiOiJjam42YjhlMG4wNTdqM2ttbDg4dmh3YThmIn0.uNAoXsEXts4ljqf2rKWLQg',
                authed: false,
                username: '',
                uid: false
            },
            map: false,
            modal: false
        }
    },
    components: { },
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
    },
    watch: { },
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
        }
    }
}
</script>
