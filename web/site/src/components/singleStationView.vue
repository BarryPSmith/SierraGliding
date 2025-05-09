<template>
    <div class="border-b border--gray-light">
        <!--err v-if="showInfo" :title="station.name" :body="station.info"
             @close="showInfo=false"/-->
        <div v-on:click="title_click"
             class="rightGridSingle cursor-pointer">
            <div class='flex flex--center-cross' style="grid-column:1">
                <h2 class="ml12 txt-h4" v-text="station.name" />
                <button v-if="station.info" @click="info_click"
                        title="Site Info"
                        class="ml6">
                    <svg class="icon"><use href="#icon-info"/></svg>
                </button>
            </div>
            <div v-if="collapsed" class="flex flex--center-cross mr12"
                style="grid-column:2">
                <div style="height: 40px;">
                    <curWindDirIndicator :dataManager="dataManager"
                                         :legend="windDirLegend"
                                         :detailLevel="0"
                                         :strokeWidth="3"
                                         style="height:40px"/>
                </div>
                <div>
                    <curWindSpdIndicator v-bind:dataManager="dataManager"
                                         :legend="station.Wind_Speed_Legend"
                                         :detailLevel="0"
                                         style="width:70px"/>
                </div>
            </div>
        </div>
        <div v-if="showInfo" v-html="stationInfo"
            class="ml12 mr12"/>
        <div v-if="showInfo" class="ml12 mr12">
            <p><span v-if="!compact">Switch to map view (top left <svg class='icon inline-block'>
                    <use href='#icon-map'/>
                </svg>) for approach visualisation,</span> or <a class="link" target="_blank" href="https://caltopo.com/m/HPL7G0S">open approach map in CalTopo</a></p>
        </div>
        <div v-if="!collapsed" class="rightGridDouble">
            <!-- Wind Speed Chart -->
            <div style="grid-row:1; max-height: 300px"
                :style="{ height: chartHeight + 'px' }"
                display:block>
                <chartWindSpd v-bind:dataManager="dataManager"
                            v-bind:duration="duration"
                            v-bind:chartEnd.sync="chartEnd"
                            v-bind:legend="station.Wind_Speed_Legend" 
                            :station="station"/>
            </div>
            <!-- Wind Speed Numbers -->
            <div style="grid-row:1; grid-column:2; display:flex;
                    justify-content:center; align-items:center">
                <curWindSpdIndicator v-bind:dataManager="dataManager"
                                    :legend="station.Wind_Speed_Legend"
                                    :detailLevel="compact ? 1 : 2"/>
            </div>
            <!-- Wind Direction Chart -->
            <div style="grid-row: 2; max-height: 300px"
                :style="{ height: chartHeight + 'px' }"
                display:block>
                <chartWindDir v-bind:dataManager="dataManager"
                            v-bind:duration="duration"
                            v-bind:chartEnd.sync="chartEnd"
                            v-bind:legend="windDirLegend"
                            v-bind:centre="windDirCentre"
                            :station="station"/>
            </div>
            <!-- Wind Direction Diagram -->
            <div style="grid-row:2; grid-column:2; display:flex;
                    justify-content:center; align-items:center">
                <curWindDirIndicator :style="dirIndicatorStyle"
                                    v-bind:dataManager="dataManager"
                                    :legend="windDirLegend"
                                    :detailLevel="compact ? 1 : 2"/>
            </div>
        </div>
        <div v-if="!collapsed">
            <p v-if="!detailed" v-on:click="detailed_click"
            class="cursor-pointer txt-xs">[[ Show Details ]]</p>
            <p v-if="detailed" v-on:click="detailed_click"
            class="cursor-pointer txt-xs">[[ Hide Details ]]</p>
            <div v-if="detailed" :style="batteryMargin">
                <chartBattery :dataManager="dataManager"
                            :duration="duration"
                            :chartEnd.sync="chartEnd"
                            :range="station.Battery_Range" 
                            :station="station"/>
            </div>
            <div v-if="detailed && etAvailable" 
                    :style="tempStyle"
                v-on:click="temp_click">
                <chartBattery :dataManager="dataManager"
                            :duration="duration"
                            :chartEnd.sync="chartEnd"
                            :range="{min:-15, max:45}"
                            :dataSource="'externalTempData'"
                            :label="'External Temperature'"
                            :station="station"/>
            </div>
            <div v-if="detailed && itAvailable" :style="batteryMargin">
                <chartBattery :dataManager="dataManager"
                            :duration="duration"
                            :chartEnd.sync="chartEnd"
                            :range="{min:-15, max:45}"
                            :dataSource="'internalTempData'"
                            :label="'Case Temperature'"
                            :station="station"
                            />
            </div>
            <div v-if="detailed && currentAvailable" :style="batteryMargin">
                <chartBattery :dataManager="dataManager"
                            :duration="duration"
                            :chartEnd.sync="chartEnd"
                            :range="{min:0, max:100}"
                            :dataSource="['currentData', 'pwmData']"
                            :label="'Charge Current'"
                            :station="station"/>
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
import err from './Error.vue';
import { EventBus } from '../eventBus.js';
import Error from './Error.vue';

export default {
    name: 'singleStationView',
    //render: h => h(App),
    props:
    { 
        'station': Object, 
        'duration': Number,
        'chartEnd': Number, 
        collapsedProp: {
            type: Boolean,
            default: true
        },
        compact: {
            type: Boolean,
            default: false,
        },
    },
    data: function() {
        return {
            legend: {
                winddir: []
                },
            timer: false,
            detailed: false,
            tempLarge: false,
            showInfo: false
        };
    },

    mounted: function() {
        if (this.station && (this.hash_present() || (window.location.hash == '#' + this.station.id)))
            this.collapsed = false;
        this.init_dataManager();
    },

    watch: {
        'station': function() {
            this.init_dataManager();
            if (this.station && (this.hash_present() || (window.location.hash == '#' + this.station.id)))
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
        err
    },

    computed: {
        dirIndicatorStyle() {
            return this.compact ?
             { /*height: '40px'*/ } :
             { width: '150px' };
        },
        batteryMargin() {
            return this.compact ?
                { 'margin-right': '40px' }
                :
                { 'margin-right': '150px' }
        },
        tempStyle() {
            return {
                height: this.tempHeight,
                'margin-right': this.compact ? '40px' : '150px',
            };
        },
        chartHeight() {
            return this.compact ? 120 : 300;
        },
        collapsed: {
            get() { return this.collapsedProp; },
            set(value) {
                this.collapsedProp = value;
                this.$emit('update:collapsed', value);
            }
        },
        dataManager() {
            return this.station && this.station.dataManager;
        },
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

        tempHeight: function() {
            if (this.tempLarge)
                return "500px";
            else
                return "150px";
        },

        currentAvailable() {
            return this.station &&
                   (!this.station.Missing_Features ||
                    !this.station.Missing_Features.includes('Current'));
        },

        itAvailable() {
            return this.station &&
                    (!this.station.Missing_Features ||
                     !this.station.Missing_Features.includes('Internal_Temperature'));
        },

        etAvailable() {
            return this.station &&
                   (!this.station.Missing_Features ||
                    !this.station.Missing_Features.includes('External_Temperature'));
        },
        stationInfo() {
            let info = this.station.info;
            if (!info) {
                return info;
            }
            info = info.replaceAll('<a', '<a class="link" taget="_blank"');
            info = info.replaceAll(/\-?\d+\.\d+,\s\-?\d+\.\d+/g,
                match => `${match} <button onClick="copyButtonClicked(this, '${match}')" class="px3">&#x2398;</button>`);
                // &#x1f5d0;
                // 
            return info
        }
    },

    methods: {
        title_click: function() {
            this.collapsed = !this.collapsed;
            if (this.collapsed) {
                this.showInfo = false;
            }
            this.set_hash();
        },
        info_click(ev) {
            ev.stopPropagation();
            if (this.station.infoIsLink) {
                window.open(this.station.info, '_blank');
            } else {
                this.showInfo = !this.showInfo;
            }
        },

        hash_present() {
            if (!this.station)
                return false;
            const hash = window.location.hash.slice(1)
            const searchParams = new URLSearchParams(hash);
            for (const [key, value] of searchParams.entries()) {
                if (key == 'station' && value == this.station.id)
                    return true;
            }
            return false;
        },
        set_hash() {
            if (!this.station)
                return;
            const hash = window.location.hash.slice(1)
            const searchParams = new URLSearchParams(hash);
            const otherStations = [];
            for (const [key, value] of searchParams.entries()) {
                if (key == 'station' && value != this.station.id) {
                    otherStations.push(value);
                }
            }
            if (this.collapsed) {
                searchParams.delete('station');
            } else {
                searchParams.set('station', this.station.id);
            }
            for (const val of otherStations) {
                searchParams.append('station', val);
            }
            window.location.hash = searchParams;
        },

        detailed_click: function() {
            this.detailed = !this.detailed;
        },
        switch_to_maps() {
            this.$emit("switch_to_maps");
        },
        temp_click: function() {
            this.tempLarge = !this.tempLarge;
        },

        init_dataManager() {
            if (this.dataManager) {
                if (!this.collapsed) {
                    //Load all necessary data
                    this.dataManager.ensure_data(this.cur_start(), this.chartEnd);
                }
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

        /*on_socket_message: function(msg) {
            if (msg.id != this.station.id) {
                return;
            }
            this.dataManager.push_if_appropriate(msg);
        }*/
    }
}
</script>
