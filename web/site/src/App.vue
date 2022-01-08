<template>
    <div class='h-viewport-full foreColor'>
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
        urlBase() {
            return `${window.location.protocol}//${window.location.host}`;
        }
    },
    mounted: function(e) {
        this.fetch_stations();
        this.init_socket();
    },
    watch: {
    },
    methods: {
        async update_title(groupName)
        {
            try {
                // Fetch the properly capitlised title from the server:
                const url = new URL(`${this.urlBase}/api/groups`);
                url.searchParams.append('name', groupName);
                const res = await fetch(url);
                if (!res.ok) return;
                const groups = await res.json();
                const group = groups[0];
                if (group && group.Name) {
                    document.title = `${group.Name} Wind | Sierra Gliding`;
                }
            } catch {}
        },
        fetch_stations: function() {
            const url = new URL(`${this.urlBase}/api/stations`);

            const splitHost = window.location.hostname.split('.');
            let groupName;

            if (window.location.pathname.toLowerCase().indexOf("all") >= 0) {
                url.searchParams.append('all', true);
            } else if (window.location.pathname.length > 1) {
                const gr = window.location.pathname.replace(/^\/+|\/+$/g, '');
                if (gr == 'teapot')
                    alert('teapot!');
                groupName = gr;
            } else if (splitHost.length > 2) {
                groupName = splitHost[0];
            }
            if (groupName) {
                this.update_title(groupName);
                url.searchParams.append('groupName', groupName);
            }

            return fetch(url, {
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
