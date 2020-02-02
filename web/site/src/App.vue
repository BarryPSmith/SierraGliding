<template>
    <div class='viewport-full'>
        <stationList :stations="stations"/>
    </div>
</template>

<script>
// === Components ===
import DataManager from './DataManager.js';
import stationList from './components/stationList.vue';

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
            station: {
                id: false,
                error: false,
                loading: true,
                name: '',
                lon: 0,
                lat: 0,
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
    components: { stationList },
    computed: {
    },
    mounted: function(e) {
        this.fetch_stations();
    },
    watch: {
    },
    methods: {
        fetch_stations: function() {
            return fetch(`${window.location.protocol}//${window.location.host}/api/stations`, {
                method: 'GET',
                credentials: 'same-origin'
            }).then((response) => {
                const ret = response.json();
                return ret;
            }).then((stations) => {
                this.stations = stations;
            });
        },
    }
}
</script>
