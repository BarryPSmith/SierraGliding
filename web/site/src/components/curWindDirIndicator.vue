<template>
    <div style='height: 100%'>
        <svg viewBox="0 0 100 100"
             :style="image_style">
            <legendArc v-for="entry in legend"
                       :min="entry.start" :max="entry.end" :fill="entry.color">
            </legendArc>
            <marker orient="auto"
                    id="Arrow1Mstart"
                    style="overflow:visible">
                <path id="path4698"
                      d="M 0,0 5,-5 -12.5,0 5,5 Z"
                      style="fill:#000000;fill-opacity:1;stroke:#000000;"
                      transform="scale(0.3)" />
            </marker>
            <marker orient="auto"
                    id="Tail"
                    style="overflow:visible">
                <g id="g4740"
                   transform="scale(-0.6)"
                   style="stroke:#000000">
                    <path id="path4728"
                          d="M -3.8048674,-3.9585227 0.54352094,0"
                          style="stroke:#000000;stroke-width:0.8" />
                    <path id="path4730"
                          d="M -1.2866832,-3.9585227 3.0617053,0"
                          style="stroke:#000000;stroke-width:0.8" />
                    <path id="path4732"
                          d="M 1.3053582,-3.9585227 5.6537466,0"
                          style="stroke:#000000;stroke-width:0.8" />
                    <path id="path4734"
                          d="M -3.8048674,4.1775838 0.54352094,0.21974226"
                          style="stroke:#000000;stroke-width:0.8" />
                    <path id="path4736"
                          d="M -1.2866832,4.1775838 3.0617053,0.21974226"
                          style="stroke:#000000;stroke-width:0.8" />
                    <path id="path4738"
                          d="M 1.3053582,4.1775838 5.6537466,0.21974226"
                          style="stroke:#000000;stroke-width:0.8" />
                </g>
            </marker>
            <path d="M 50,10 V 60"
                  style="stroke:#000000;marker-start:url(#Arrow1Mstart);marker-end:url(#Tail)"
                  :stroke-width="strokeWidth"
                  :transform="'rotate(' + curVal + ', 50, 50)'" />
            <path d="M 50,10 V 60"
                  style="stroke:#000000;marker-start:url(#Arrow1Mstart);marker-end:url(#Tail)"
                  opacity="0.3"
                  :stroke-width="strokeWidth"
                  :transform="'rotate(' + curAvg + ', 50, 50)'" />
        </svg>
        <h1 v-if="detailed" class="txt-h2 align-center"> {{ curVal | number0 }} &deg; </h1>
        <p v-if="detailed" class="align-center">Avg: {{ curAvg | number0 }} &deg</p>
        <p v-if="detailed">Arrow points in the direction the wind is coming from.</p>
    </div>
</template>
<script>
import legendArc from './legendArc.vue';

export default {
    name: 'curWindDirIndicator',

    data: function() {
        return {
            curVal: 0,
            curAvg: 0,
            lastTime: null,
        };
    },

    props: {
        dataManager: undefined,
        legend: undefined,
        detailed: {
            type: Boolean,
            default: true
        },
        strokeWidth: {
            type: Number,
            default: 1.1
        }
    },

    mounted: function() {
        this.on_dataManager_changed();
    },

    watch: {
        'dataManager': function() { this.on_dataManager_changed(); },
    },

    computed: {
        image_style: function() {
            if (!this.detailed) return 'height: 100%;';
            else return '';
        }
    },

    components: {
        legendArc
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
            if (!this.dataManager.windDirectionData || this.dataManager.windDirectionData.length < 1) {
                return;
            }
            if (false) {
                const mostRecent = this.dataManager.windDirectionData[this.dataManager.windDirectionData.length - 1];
                if (mostRecent && mostRecent.x > this.lastTime /*&&
                    mostRecent.x > new Date() - 60000*/) {
                    this.lastTime = mostRecent.x;
                    this.curVal = mostRecent.y;
                }
            } else {
                const mostRecent = this.dataManager.stationData[this.dataManager.stationData.length - 1];
                if (mostRecent && mostRecent.timestamp > this.lastTime) {
                    this.lastTime = mostRecent.timestamp;
                    this.curVal = mostRecent.wind_direction;
                    this.curAvg = mostRecent.wind_direction_avg;
                }
            }
        },
    },
}
</script>
