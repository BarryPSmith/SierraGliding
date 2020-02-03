<template>
    <h1 class="align-center align-mid outlineText"
        :class="text_size"
        :style="{ color: color }">{{ curVal | number1 }} {{ dataManager.unit }}</h1>
</template>
<script>
export default {
    name: 'curWindSpdIndicator',

    data: function() {
        return {
            curVal: null,
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
            if (typeof(this.curVal) != "number"
                || !this.dataManager
                || !this.legend) {
                return 'Black';
            }
            const factor = this.dataManager.unit == 'mph' ? 1.6 : 1.0;
            for (const entry of this.legend) {
                if (entry.top >= this.curVal * factor ) {
                    return entry.color;
                }
            }
            return this.legend[this.legend.length - 1].color;
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
            const mostRecent = this.dataManager.windspeedData[this.dataManager.windspeedData.length - 1];
            if (mostRecent && mostRecent.x > this.lastTime /*&&
                mostRecent.x > new Date() - 60000*/) {
                this.lastTime = mostRecent.x;
                this.curVal = mostRecent.y;
            }
        },
    },
}
</script>