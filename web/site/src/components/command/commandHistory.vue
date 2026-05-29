<template>
    <div>
        <table class="table">
            <tr>
                <th>id</th>
                <th width="110px">Requested</th>
                <th>Type</th>
                <th>Tries</th>
                <th>Result</th>
                <th width="110px">Completed</th>
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
export default {
    name: 'commandHistory',
    props: {
        authFetch: Function,
        stationId: Number
    },
    data: function() { return {
        commands: []
    }},
    watch: {
        'authFetch': function() { this.init(); },
        'stationId': function() { this.init(); }
    },
    methods: {
        async init() {
            if (!this.authFetch || !this.stationId) {
                return;
            }
            const resp = await this.authFetch(`/api/commands?stationId=${this.stationId}`);
            const respJson = await resp.json();
            this.commands = respJson;
        }
    }
}
</script>