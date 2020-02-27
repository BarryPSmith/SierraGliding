<template>
    <div>
        <select class="fr px24" v-model="duration">
            <option value="300">5 Minutes</option>
            <option value="900">15 Minutes</option>
            <option value="3600">1 Hour</option>
            <option value="14400">4 Hours</option>
            <option value="43200">12 Hours</option>
            <option value="86400">24 Hours</option>
            <option value="604800">1 Week</option>
            <option value="2592000">1 Month</option>
        </select>
        <div class='h-full w-full'>
            <div class='align-center border-b border--gray h36 my6'>
                <h1 class='txt-h4'>Active Stations</h1>
            </div>
            <singleStationView v-for='cstation in stations'
                               :station="cstation"
                               :duration="duration"
                               v-bind:chartEnd.sync="chartEnd" />
        </div>
    </div>
</template>
<script>
import singleStationView from './singleStationView.vue'

export default {
    name: 'stationList',
    components: { singleStationView },
    props: [ 'stations' ],
    data: function() { 
        return {
            chartEnd: null,
            duration: 900
        };
    },
    mounted: function () {
    },
    watch: {
        'chartEnd': function() { 
            this.$emit('update:chartEnd', this.chartEnd);
        }
    },
    computed: {
        dur300: function() {
            return {
                'btn--stroke': this.duration === 300
            }
        },
        dur3600: function() {
            return {
                'btn--stroke': this.duration === 3600
            }
        },
        dur14400: function() {
            return {
                'btn--stroke': this.duration === 14400
            }
        },
        dur42200: function() {
            return {
                'btn--stroke': this.duration === 42200
            }
        },
        dur84400: function() {
            return {
                'btn--stroke': this.duration === 84400
            }
        }
    },

    methods: {

    }
}
</script>