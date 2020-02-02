<template>
    <div>
        <div class="flex-parent-inline fr px24">
            <button @click='duration =   300' :class='dur300' class="btn btn--pill btn--pill-hl">5 Minutes</button>
            <button @click='duration =  3600' :class='dur3600' class="btn btn--pill btn--pill-hc">1 Hour</button>
            <button @click='duration = 14400' :class='dur14400' class="btn btn--pill btn--pill-hc">4 Hours</button>
            <button @click='duration = 42200' :class='dur42200' class="btn btn--pill btn--pill-hc">12 Hours</button>
            <button @click='duration = 84400' :class='dur84400' class="btn btn--pill btn--pill-hr">24 Hours</button>
        </div>
        <div class='h-full w-full'>
            <div class='align-center border-b border--gray-light h36 my6'>
                <h1 class='txt-h4'>Active Stations!</h1>
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
            chartEnd: new Date(),
            duration: 300
        };
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
    }
}
</script>