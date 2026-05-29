<template>
    <div>
        <form v-if="!loggedIn" v-on:submit.prevent="login_click" class="align-center">
            <div>
                <p>Username</p>
                <input v-model="username"/>
            </div>
            <div>
                <p>Password</p>
                <input type="password" v-model="password"/>
            </div>
            <button type=submit class="btn">Login</button>
        </form>
        <div v-if="loggedIn">
            <div>
                <p>Station</p>
                <input v-model="selectedStationId"/>
                <select v-model="selectedStationName">
                    <option v-for="station in stations">
                        {{ station.id }} {{ station.name }}
                    </option>
                </select>
            </div>
            <h2>Command History</h2>
            <commandHistory :authFetch="authFetch" :stationId="selectedStationId"/>
            <h2>New Command</h2>
            <CommandIssuer :authFetch="authFetch" :stationId="selectedStationId"
                :station="selectedStation"/>
        </div>
    </div>
</template>
<script>
import commandHistory from './components/command/commandHistory.vue'
import CommandIssuer from './components/command/commandIssuer.vue';
import commandIssuer from './components/command/commandIssuer.vue'

export default {
    name: 'management',
    components: {
        commandHistory,
        commandIssuer
    },
    data: function() {
        return {
            loggedIn: false,
            userId: null,
            username: "",
            password: "",
            stationId: null,
            selectedStationName: null,
            stations: [],
            selectedStation: null,
            selectedStationId: null
        };
    },
    watch: {
        'selectedStation': function() {
            if (this.selectedStation != null) {
                const station = this.stations.find(s =>
                    `${s.id} ${s.name}` == this.selectedStation);
                this.selectedStation = station;
                if (station != null) {
                    this.selectedStationId = station.id;
                }
            }
        }
    },
    methods: {
        async login_click() {
            const unpwd = this.username + ":" + this.password;
            this.token = btoa(unpwd);
            const res = await this.authFetch('/api/auth');
            if (res.ok) {
                const resJson = await res.json()
                this.userId = resJson.userId;
                this.loggedIn = true;
                this.fetchStations();
            } else {
                alert('Unable to log in.');
            }
        },
        authFetch(url, options = {}) {
            const headers = {
                ...options.headers,
                Authorization: `Basic ${this.token}`
            };
            return fetch(url, {...options, headers});
        },
        async fetchStations() {
            const resp = await this.authFetch(`/api/stations?userId=${this.userId}`);
            if (!resp.ok) {
                alert('unable to fetch stations');
                return;
            }
            this.stations = await resp.json();
        }
    }
}
</script>