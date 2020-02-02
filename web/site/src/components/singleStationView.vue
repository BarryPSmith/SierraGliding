<template>
    <div>
        <div>
            <h1 class="txt-h2" v-text="station.name" />
        </div>
        <div>
            <chartWindDir v-bind:dataManager="dataManager"
                          v-bind:duration="duration"
                          v-bind:chartEnd.sync="chartEnd"
                          v-bind:legend="station.Wind_Dir_Legend" />
        </div>
        <div>
            <chartWindSpd v-bind:dataManager="dataManager"
                          v-bind:duration="duration"
                          v-bind:chartEnd.sync="chartEnd"
                          v-bind:legend="station.Wind_Speed_Legend" />
        </div>
        <!--
    <chartBattery v-bind:dataManager="dataManager"
                  v-bind:duration="duration"
                  v-bind:chartEnd="chartEnd"/>
        -->
    </div>
</template>

<script>
import chartBattery from './chartBattery.vue';
import chartWindDir from './chartWindDir.vue';
import chartWindSpd from './chartWindSpd.vue';
import DataManager from '../DataManager';

export default {
    name: 'singleStationView',
    //render: h => h(App),
    props: [ 'station', 'dataType', 'duration', 'chartEnd' ],
    data: function() {
        return {
            legend: {
                winddir: []
                },
            timer: false,
            dataManager: null,
        };
    },

    watch: {
        'station': function() {
            if (this.station && this.station.id) {
                this.dataManager = new DataManager(this.station.id);
                this.dataManager.ensure_data(this.cur_start(), this.cur_end());
            }
        },

        'chartEnd': function() {
            this.$emit('update:chartEnd', this.chartEnd);
            if (this.dataManager)
                this.dataManager.ensure_data(this.cur_start(), this.cur_end());
        },

        'duration': function() {
            if (this.dataManager)
                this.dataManager.ensure_data(this.cur_start(), this.cur_end());
        }
    },

    components: {
        chartWindDir,
        chartWindSpd,
        chartBattery
    },

    methods: {
        cur_end: function () {
            if (this.chartEnd) {
                return this.chartEnd;
            } else {
                return new Date();
            }
        },

        cur_start: function () {
            return this.cur_end() - this.duration * 1000;
        },


        update_range: function() {
            this.dataManager.ensureData(this.cur_start(), this.cur_end());
        }
    }
}
</script>