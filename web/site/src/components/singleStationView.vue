<template>
    <div class="border-b border--gray-light">
        <div v-on:click="title_click"
             class="rightGridSingle cursor-pointer">
            <div class='flex-parent flex-parent--center-cross' style="grid-column:1">
                <h2 class="ml12 flex-child txt-h4" v-text="station.name" />
            </div>
            <div v-if="collapsed" class="flex-parent flex-parent--center-cross mr12"
                style="grid-column:2">
                <div class='flex-child' style="height: 40px;">
                    <curWindDirIndicator :dataManager="dataManager"
                                         :legend="station.Wind_Dir_Legend"
                                         :detailed="false"
                                         :strokeWidth="3"/>
                </div>
                <div class='flex-child'>
                    <curWindSpdIndicator v-bind:dataManager="dataManager"
                                         :legend="station.Wind_Speed_Legend"
                                         :detailed="false"
                                         style="width:50px"/>
                </div>
            </div>
        </div>
        <div v-if="!collapsed" class="rightGridDouble">
            <div style="grid-row:1; height:300px; max-height: 300px"
                 display:block>
                <chartWindSpd v-bind:dataManager="dataManager"
                              v-bind:duration="duration"
                              v-bind:chartEnd.sync="chartEnd"
                              v-bind:legend="station.Wind_Speed_Legend" />
            </div>
            <div style="grid-row:1; grid-column:2; display:flex;
                    justify-content:center; align-items:center">
                <curWindSpdIndicator v-bind:dataManager="dataManager"
                                     :legend="station.Wind_Speed_Legend"/>
            </div>
            <div style="grid-row: 2; height:300px; max-height: 300px"
                 display:block>
                <chartWindDir v-bind:dataManager="dataManager"
                              v-bind:duration="duration"
                              v-bind:chartEnd.sync="chartEnd"
                              v-bind:legend="windDirLegend"
                              v-bind:centre="windDirCentre"/>
            </div>
            <div style="grid-row:2; grid-column:2; display:flex;
                    justify-content:center; align-items:center">
                <curWindDirIndicator style="width:150px" v-bind:dataManager="dataManager"
                                     :legend="windDirLegend"/>
            </div>
        </div>
        <div v-if="!collapsed">
            <p v-if="!detailed" v-on:click="detailed_click"
               class="cursor-pointer txt-xs">[[ Show Details ]]</p>
            <p v-if="detailed" v-on:click="detailed_click"
               class="cursor-pointer txt-xs">[[ Hide Details ]]</p>
            <div v-if="detailed">
                <chartBattery :dataManager="dataManager"
                              :duration="duration"
                              :chartEnd.sync="chartEnd"
                              :range="station.Battery_Range" />
            </div>
            <div v-if="detailed">
                <chartBattery :dataManager="dataManager"
                              :duration="duration"
                              :chartEnd.sync="chartEnd"
                              :range="{min:-15, max:45}"
                              :dataSource="'externalTempData'"
                              :label="'External Temperature'"/>
            </div>
            <div v-if="detailed">
                <chartBattery :dataManager="dataManager"
                              :duration="duration"
                              :chartEnd.sync="chartEnd"
                              :range="{min:-15, max:45}"
                              :dataSource="'internalTempData'"
                              :label="'Case Temperature'"/>
            </div>
        </div>
    </div>
</template>

<script>
import chartBattery from './chartBattery.vue';
import chartWindDir from './chartWindDir.vue';
import chartWindSpd from './chartWindSpd.vue';
import curWindSpdIndicator from './curWindSpdIndicator.vue';
import curWindDirIndicator from './curWindDirIndicator.vue';
import DataManager from '../DataManager';
import { EventBus } from '../eventBus.js';

export default {
    name: 'singleStationView',
    //render: h => h(App),
    props: [ 'station', 'dataType', 'duration', 'chartEnd', ],
    data: function() {
        return {
            legend: {
                winddir: []
                },
            timer: false,
            dataManager: null,
            collapsed: true,
            detailed: false,
        };
    },

    mounted: function() {
        if (this.station && window.location.hash == '#' + this.station.id)
            this.collapsed = false;
        this.init_dataManager();
        EventBus.$on('socket-message', this.on_socket_message);
    },

    watch: {
        'station': function() {
            this.init_dataManager();
            if (this.station && window.location.hash == '#' + this.station.id)
                this.collapsed = false;
        },

        'chartEnd': function() {
            this.$emit('update:chartEnd', this.chartEnd);
            if (this.dataManager && !this.collapsed)
                this.dataManager.ensure_data(this.cur_start(), this.chartEnd);
        },

        'duration': function() {
            if (this.dataManager && !this.collapsed)
                this.dataManager.ensure_data(this.cur_start(), this.chartEnd);
        },

        'collapsed': function() {
            if (this.dataManager && !this.collapsed)
                this.dataManager.ensure_data(this.cur_start(), this.chartEnd);
        },
    },

    components: {
        chartWindDir,
        chartWindSpd,
        chartBattery,
        curWindSpdIndicator,
        curWindDirIndicator,
    },

    computed: {
        windDirLegend: function() {
            if (this.station) {
                const ret = this.station.Wind_Dir_Legend;
                if (ret && ret.legend)
                    return ret.legend;
                else
                    return ret;
            } else {
                return null;
            }
        },

        windDirCentre: function() {
            if (this.station &&
                this.station.Wind_Dir_Legend &&
                typeof(this.station.Wind_Dir_Legend.centre) == "number") {
                return this.station.Wind_Dir_Legend.centre;
            }
            else
                return 180;
        },
    },

    methods: {
        title_click: function() {
            this.collapsed = !this.collapsed;
            if (!this.collapsed && this.station) {
                window.location.hash = this.station.id;
            }
        },

        detailed_click: function() {
            this.detailed = !this.detailed;
        },

        init_dataManager() {
            if (this.station && this.station.id) {
                this.dataManager = new DataManager(this.station.id);
                this.dataManager.ensure_data(this.cur_start(), this.chartEnd);
            }
        },

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
        },

        on_socket_message: function(msg) {
            if (msg.id != this.station.id) {
                return;
            }
            this.dataManager.push_if_appropriate(msg);
        }
    }
}
</script>
