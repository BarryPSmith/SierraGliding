<template>
    <div class="h-viewport-1/2 overflow-scroll" ref="historyDiv">
        <table class="table">
            <tr>
                <th class="sticky bg-white">id</th>
                <th class="sticky bg-white" width="110px">Requested</th>
                <th class="sticky bg-white">Type</th>
                <th class="sticky bg-white">Tries</th>
                <th class="sticky bg-white">Result</th>
                <th class="sticky bg-white" width="110px">Completed</th>
            </tr>
            <tr v-for="command in commands">
                <td>{{command.ID}}</td>
                <td>{{new Date(command.request_time * 1000).toDatetimeLocal2()}}</td>
                <td>{{command.command_type}}</td>
                <td>{{command.attempts}}</td>
                <td class="pre">{{command.response}}</td>
                <td>{{command.response_time ? new Date(command.response_time * 1000).toDatetimeLocal2() : ''}}</td>
            </tr>
        </table>
    </div>
</template>
<script>
import Vue from 'vue';

export default {
    name: 'commandHistory',
    props: {
        authFetch: Function,
        stationId: Number
    },
    data: function() { return {
        commands: [],
        commandDict: new Map(),
    }},
    watch: {
        'authFetch': function() { this.init(); },
        'stationId': function() { this.init(); }
    },
    mounted() {
        this.init_socket();
    },
    methods: {
        async init() {
            if (!this.authFetch || !this.stationId) {
                return;
            }
            const resp = await this.authFetch(`/api/commands?stationId=${this.stationId}`);
            if (!resp.ok)
                alert('unable to fetch commands.');
            const respJson = await resp.json();
            this.commands = respJson;
            this.commandDict = new Map(this.commands.map(c => [c.ID, c]));
            this.scroll_to_end();
        },
        init_socket() {
            let protocol = 'ws';
            if (window.location.protocol=='https:')
                protocol = 'wss';
            this.ws = new WebSocket(`${protocol}://${window.location.hostname}:${window.location.port}`);
            this.ws.onmessage = this.socket_onMessage;
            this.ws.onclose = this.init_socket;
        },
        scroll_to_end() {
            this.$nextTick(() => {
                const elem = this.$refs["historyDiv"];
                elem.scrollTo({
                    top: elem.scrollHeight,
                    behavior: 'smooth'
                });
            });
        },
        async socket_onMessage(msg) {
            if (!this.authFetch) {
                return;
            }
            const msgData = JSON.parse(msg.data);
            if (msgData.type != 'command') {
                return;
            }
            switch (msgData.op)
            {
                case 'Add':
                    {
                    const resp = await this.authFetch(`/api/commands?commandId=${msgData.commandId}`);
                    if (!resp.ok)
                        return;
                    const respJson = await resp.json();
                    const cmd = respJson[0];
                    this.commands.push(cmd)
                    this.commandDict.set(cmd.ID, cmd);
                    }
                    break;
                case 'Remove':
                    const index = this.commands.findIndex(cmd => cmd.ID == msgData.commandId);
                    if (index >= 0)
                        this.commands.splice(index, 1);
                    break;
                case 'Attempt':
                case 'Respond':
                    {
                    const resp2 = await this.authFetch(`/api/commands?commandId=${msgData.commandId}`);
                    if (!resp2.ok)
                        return;
                    const respJson2 = await resp2.json();
                    var newCmd = respJson2[0];
                    const cmd = this.commandDict.get(newCmd.ID)
                    if (cmd == null) {
                        return;
                    }
                    Vue.set(cmd, 'attempts', newCmd.attempts);
                    Vue.set(cmd, 'response', newCmd.response);
                    Vue.set(cmd, 'response_time', newCmd.response_time);
                    }
                    break;
            }
            this.scroll_to_end();
        }
    }
}
</script>