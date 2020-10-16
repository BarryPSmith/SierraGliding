<template>
    <div>
        <h1 class="align-center align-mid outlineText"
            :class="text_size"
            :style="{ color: color }">
            {{ curVal | number1 }}
        </h1>

        <h1 class="align-center align-mid outlineText"
            :class="text_size"
            :style="{ color: color }">
            {{ dataManager ? dataManager.unit : '' }}
        </h1>

        <div v-if="detailed"
             class="align-center align-mid outlineText txt-h4 txt-bold mt12"
             :style="{ color: avgColor }">
            Avg: {{ curAvg | number1 }} {{ dataManager && dataManager.unit }}
        </div>
        <div v-if="detailed"
             class="align-center align-mid outlineText txt-h4 txt-bold"
             :style="{ color: gustColor }">
            Gust: {{ curGust | number1 }} {{ dataManager && dataManager.unit }}
        </div>
    </div>
</template>
<script>
export default {
    name: 'curWindSpdIndicator',

    data: function() {
        return {
            curVal: null,
            curGust: null,
            curAvg: null,
            lastTime: null,
        };
    },

    props: { 
        dataManager: undefined, 
        legend: undefined,
        detailed: {
            type: Boolean,
            default: true
        }
    },

    mounted: function() {
        this.on_dataManager_changed();
    },

    computed: {
        color: function() {
            return this.get_color(this.curVal);
        },
        avgColor: function() {
            return this.get_color(this.curAvg);
        },
        gustColor: function() {
            return this.get_color(this.curGust);
        },

        text_size: function() {
            if (this.detailed)
                return "txt-h2";
            else
                return "txt-h4 txt-bold";
        }
    },

    watch: {
        'dataManager': function() { this.on_dataManager_changed(); }
    },

    methods: {
        get_color(value) {
            if (typeof (value) != "number"
                || !this.dataManager
                || !this.legend) {
                return 'Black';
            }
            const factor = this.dataManager.unit == 'mph' ? 1.6 : 1.0;
            for (const entry of this.legend) {
                if (entry.top >= value * factor) {
                    return entry.color;
                }
            }
            return this.legend[this.legend.length - 1].color;
        },
        on_dataManager_changed: function() {
            if (!this.dataManager) {
                return;
            }
            this.dataManager.on('data_updated', this.data_updated);
            this.data_updated();
        },

        data_updated: function() {
            if (!this.dataManager.windspeedData || this.dataManager.windspeedData.length < 1) {
                return;
            }
            if (false) {
                const mostRecent = this.dataManager.windspeedData[this.dataManager.windspeedData.length - 1];
                if (mostRecent && mostRecent.x > this.lastTime /*&&
                    mostRecent.x > new Date() - 60000*/) {
                    this.lastTime = mostRecent.x;
                    this.curVal = mostRecent.y;
                }
            } else {
                const mostRecent = this.dataManager.stationData[this.dataManager.stationData.length - 1];
                if (mostRecent && mostRecent.timestamp > this.lastTime) {
                    this.lastTime = mostRecent.timestamp;
                    const factor = this.dataManager.wsFactor();
                    this.curVal = mostRecent.windspeed / factor;
                    this.curAvg = mostRecent.windspeed_avg / factor;
                    this.curGust = mostRecent.windspeed_max / factor;
                }
            }
        },
    },
}
</script>
