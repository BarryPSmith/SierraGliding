<template>
<div>
    <select v-model="selectedCommand">
        <option>QueryConfig</option>
        <option>QueryStatus</option>
        <option>QuerySignal</option>
        <option>Battery</option>
        <option>Radio</option>
        <option>Relay</option>
        <option>ReportInterval</option>
        <option>Charging</option>
        <option>Weather</option>
        <option>ID</option>
        <option>Restart</option>
        <option>Raw</option>
    </select>
    <div v-if="selectedCommand=='Battery'">
        <div>
            <p>Low Power (mV)</p>
            <input v-model="batteryThreshold" type="number"/>
        </div>
        <div>
            <p>Emergency Power (mV)</p>
            <input v-model="batteryEmergency" type="number"/>
        </div>
    </div>
    <div v-if="selectedCommand=='Radio'">
        <select v-model="radioSubSelect">
            <option>Power</option>
            <option>CMSA_P</option>
            <option>CSMA_T</option>
            <option>Frequency</option>
            <option>Bandwidth</option>
            <option>SpreadingFactor</option>
            <option>OutboundPreamble</option>
            <option>InboundPreamble</option>
            <option>BoostedRx</option>
            <option>CodingRate</option>
            <option>RelayListenPeriod</option>
        </select>
        <input v-model="RadioParameter" type="number"/>
    </div>
    <div v-if="selectedCommand=='Relay'">
        <select v-model="relayAdd">
            <option>Add</option>
            <option>Remove</option>
        </select>
        <select v-model="relayCommand">
            <option>Weather</option>
            <option>Command</option>
        </select>
        <input v-model="relayStationId" type="number">
        <select v-model="relayStationName">
            <option v-for="station in relayStations">
                {{station.id}} {{station.name}}
            </option>
        </select>
    </div>
    <input v-if="selectedCommand=='ReportInterval'" type="number"
            v-model="newInterval"/>
    <div v-if="selectedCommand=='ID'">
        <p>New ID (use 0 for random)</p>
        <input v-model="newId" type="number"/>
    </div>
    <div v-if="selectedCommand=='Weather'">Not implemented!</div>
    <div v-if="selectedCommand=='Charging'">
        <div>
            <p>Charge Voltage (mv)</p>
            <input v-model="chargeVoltage" type="number"/>
        </div>
        <div>
            <p>Freezing Voltage (mv)</p>
            <input v-model="chargeFreezingVoltage" type="number"/>
        </div>
        <div>
            <p>Responsivity</p>
            <input v-model="chargeResponsitivity" type="number"/>
        </div>
        <div>
            <p>Freezing Current (mA)</p>
            <input v-model="chargeFreezingCurrent" type="number"/>
        </div>
    </div>
    <button class="btn" @click="go">Go!</button>
</div>
</template>
<script>

export default {
    name: 'commandIssuer',
    props: {
        authFetch: Function,
        stationId: Number,
        station: Object
    },
    data: function() { return {
        selectedCommand: "",
        batteryThreshold: 3700,
        batteryEmergency: 3500,
        radioSubSelect: 'Power',
        radioParameter: 22,
        relayAdd: 'Add',
        relayCommand: 'Command',
        relayStationId: null,
        relayStationName: null,
        relayStations: [],
        newInterval: 4000,
        newId: 0,
        chargeVoltage: 4150,
        chargeFreezingVoltage: 4000,
        chargeResponsitivity: 40,
        chargeFreezingCurrent: 6
    }},
    watch: {
        'station': updateRelayStations
    },
    computed: {
        
    },
    methods: {
        async go() {
            if (!this.authFetch) {
                return;
            }
            if (!this.selectedCommand) {
                alert('No command selected');
                return;
            }
            try
            {
                const cmd = { commandType: this.selectedCommand }
                switch (this.selectedCommand)
                {
                    case 'QueryConfig':
                    case 'QueryStatus':
                    case 'QuerySignal':
                    case 'Restart':
                        // Simple, no command data included
                        break;
                    case 'Battery':
                        cmd.commandData = {
                            Threshold: this.batteryThreshold,
                            Emergency: this.batteryEmergency
                        };
                        break;
                    case 'Radio':
                        cmd.commandData = {
                            Parameter: this.radioSubSelect,
                            Value: this.radioParameter
                        }
                        break;
                    case 'Relay':
                        cmd.commandData = [{
                            Add: this.relayAdd == "Add",
                            Command: this.relayCommand == "Command",
                            Id: this.relayStationId
                        }];
                        break;
                    case 'ReportInterval':
                        cmd.commandData = {
                            Value: this.newInterval
                        };
                        break;
                    case 'Charging':
                        cmd.commandData = {
                            DesiredVoltage: this.chargeVoltage,
                            ResponseRate: this.chargeResponsitivity,
                            FreezingVoltage: this.chargeFreezingVoltage,
                            FreezingPwm: this.chargeFreezingCurrent
                        }
                        break;
                    case 'ID':
                        cmd.commandData = {
                            Value: this.newId
                        };
                        break;
                }
                const resp = await authFetch('/api/commands', {
                    body: cmd,
                    method: 'POST',
                });
                if (!resp.ok) {
                    throw resp.statusText
                }
                alert('Command posted!');
                selectedCommand = null;
            } catch (err) {
                alert(`unable to post command: ${err}`);
            }
        },


        async updateRelayStations() {
            if (!this.station) {
                this.relayStations = null;
            }
            const resp = await fetch(`/api/stations?groupId=${this.station.groupId}`);
            const respJson = await resp.json();
            this.relayStations = respJson;
        }
    }
}
</script>