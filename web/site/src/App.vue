<template>
    <div class='h-full foreColor'>
        <div class='absolute left top z1 h30 round'>
            <button @click='mode = "list"' 
                    class='bg-white my6 mx6 btn btn--s btn--stroke btn--gray round fr h30'
                    style="font-size:18px"
                    v-if="mode != 'list'"
                    title="List Display">
                <svg class='icon' title="List Display"><use href='#icon-menu'/></svg>
            </button>
            <button @click='mode = "map"' 
                    class='bg-white my6 mx6 btn btn--s btn--stroke btn--gray round fr h30'
                    style="font-size:18px"
                    title="Map Display"
                    v-else>
                <svg class='icon'>
                    <use href='#icon-map'/>
                </svg>
            </button>
        </div>
        <stationList v-if="mode=='list'" :stations="stations"/>
        <sgMap v-else :stations="stations"
               :stationDict="stationDict"
               :mapGeometry="mapGeometry"/>
    </div>
</template>

<script>
// === Components ===
import stationList from './components/stationList.vue';
import sgMap from './components/map.vue';
import { EventBus } from './eventBus.js';
import DataManager from './DataManager';

export default {
    name: 'app',
    data: function() {
        return {
            ws: null,
            stations: null,
            timer: null,
            stationDict: null,
            mode: null,
            mapGeometry: null,
        }
    },
    components: { stationList, sgMap },
    computed: {
        urlBase() {
            return `${window.location.protocol}//${window.location.host}`;
        }
    },
    mounted: function(e) {
        this.mode = window.localStorage.getItem('mode') == 'map' ? 'map' : 'list';
        this.fetch_stations();
        this.fetch_mapGeometry();
        this.init_socket();
    },
    watch: {
        mode(oldValue, newValue) {
            if (newValue == null) return;
            window.localStorage.setItem('mode', this.mode);
        }
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
        async fetch_mapGeometry() {
            const url = new URL(`${this.urlBase}/api/mapGeometry`);

            const splitHost = window.location.hostname.split('.');
            let groupName;
            if (window.location.pathname.length > 1) {
                groupName = window.location.pathname.replace(/^\/+|\/+$/g, '');
            } else if (splitHost.length > 2) {
                groupName = splitHost[0];
            }
            if (groupName) {
                url.searchParams.append('groupName', groupName);
            }
            const response = await fetch(url, {
                method: 'GET',
                credentials: 'same-origin'
            });
            const geometry = await response.json();
            this.mapGeometry = geometry;
        },
        async fetch_stations() {
            const url = new URL(`${this.urlBase}/api/stations`);

            const splitHost = window.location.hostname.split('.');
            let groupName;

            if (window.location.pathname.toLowerCase().indexOf("all") >= 0) {
                url.searchParams.append('all', true);
            } else if (window.location.pathname.length > 1) {
                groupName = window.location.pathname.replace(/^\/+|\/+$/g, '');
            } else if (splitHost.length > 2) {
                groupName = splitHost[0];
            }
            if (groupName) {
                this.update_title(groupName);
                url.searchParams.append('groupName', groupName);
            }

            const response = await fetch(url, {
                method: 'GET',
                credentials: 'same-origin'
            });
            const stations = await response.json();
            for (const station of stations)
            {
                station.dataManager = new DataManager(station.id);
                station.dataManager.ensure_data(+new Date() - 60000, null);
            }
            this.stations = stations;
            this.stationDict = Object.fromEntries(stations.map(s => [s.id, s]));
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
            // Only accept things from the past 60 seconds:
            if (msgData.timestamp > (+new Date() / 1000 - 60)) {
                EventBus.$emit('socket-message', msgData);
                const dataManager = this.stationDict &&
                    this.stationDict[msgData.id] &&
                    this.stationDict[msgData.id].dataManager;
                if (dataManager) {
                    dataManager.push_if_appropriate(msgData);
                }
            }
        }
    }
}
</script>
