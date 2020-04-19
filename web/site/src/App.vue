<template>
    <div class='viewport-full'>
        <stationList :stations="stations"/>
    </div>
</template>

<script>
// === Components ===
import stationList from './components/stationList.vue';
import { EventBus } from './eventBus.js';

export default {
    name: 'app',
    data: function() {
        return {
            ws: null,
            stations: null,
            timer: null,
        }
    },
    components: { stationList },
    computed: {
    },
    mounted: function(e) {
        this.fetch_stations();
        this.init_socket();
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

        init_socket: function() {
            let protocol = 'ws';
            if (window.location.protocol=='https:')
                protocol = 'wss';
            this.ws = new WebSocket(`${protocol}://${window.location.hostname}:${window.location.port}`);
            this.ws.onmessage = this.socket_onMessage;
            this.ws.onclose = this.init_socket;
        },

        socket_onMessage: function(msg) {
            const msgData = JSON.parse(msg.data);
            // Sometimes if we leave the site open in the background all day we come back to find it frozen.
            // It seems it's in a rush to catch up a bunch of old socket messages.
            // So..
            // Only accept things from the past 30 seconds:
            if (msgData.timestamp > (+new Date() / 1000 - 30))
                EventBus.$emit('socket-message', msgData);
        }
    }
}
</script>
