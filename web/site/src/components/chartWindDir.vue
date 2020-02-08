<script>
import chartBase from './chartBase.vue';

export default {
    extends: chartBase,
    name: 'chartWindDir',

    props: [ 'legend' ],

    mounted: function() { 
        this.base_mounted();
        this.set_direction_annotations();
    },


    data: function() {
        return {
            dataSource: 'windDirectionData'
            };
    },

    watch: {
        'legend': function() { 
            this.set_direction_annotations(); 
        }
    },

    methods: {
        set_direction_annotations: function () {
            if (!this.chart || !this.legend) {
                return;
            }

            this.chart.options.annotation.annotations = [];

            for (const entry of this.legend) {
                if (entry.start === undefined
                    || entry.end === undefined
                    || entry.color === undefined) {
                    continue;
                }
                let subEntries = [entry];
                if (entry.start > entry.end) {
                    subEntries = [{
                        start: 0,
                        end: entry.end,
                        color: entry.color
                    }, {
                        start: entry.start,
                        end: 360,
                        color: entry.color
                    }];
                }
                for (const subEntry of subEntries) {
                    this.chart.options.annotation.annotations.push({
                        type: 'box',
                        xScaleID: 'x-axis-0',
                        yScaleID: 'y-axis-0',
                        yMin: subEntry.start,
                        yMax: subEntry.end,
                        //xMin: minX,
                        //xMax: maxX,
                        backgroundColor: subEntry.color,
                        borderColor: 'rgba(0,0,0,0)',
                    });
                }
            }
        },

        ensure_chart: function () {
            if (this.chart)
                return;

            let wdElem = this.$refs.chart;

            var annotationPlugin = require('chartjs-plugin-annotation');
            var zoomPlugin = require('chartjs-plugin-zoom');
            var filterPlugin = require('../chart-filter.js');
            Chart.plugins.register(annotationPlugin);
            Chart.plugins.register(zoomPlugin);
            Chart.plugins.register(filterPlugin);

            if (wdElem) {
                let wdOpts = this.commonOptions;
                wdOpts.plugins.zoom.pan.onPan = this.chart_panning;
                wdOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                wdOpts.scales.yAxes[0].ticks.max = 360;
                wdOpts.scales.yAxes[0].ticks.stepSize = 45;
                const windNames = {
                    0: 'N',
                    45: 'NE',
                    90: 'E',
                    135: 'SE',
                    180: 'S',
                    225: 'SW',
                    270: 'W',
                    315: 'NW',
                    360: 'N'
                }
                wdOpts.scales.yAxes[0].ticks.callback = value => {
                    if (windNames[value] !== undefined)
                        return windNames[value];
                    else
                        return value;
                }
                this.chart = new Chart(wdElem, {
                    type: 'line',
                    data: {
                        allData: [],
                        datasets: [{
                            label: 'Wind Direction',
                            pointBackgroundColor: 'black',
                            pointBorderColor: 'black',
                            pointRadius: 2,
                            borderColor: 'black',
                            showLine: false,
                            fill: false,
                            //data: this.dataManager.windDirectionData,
                            lineTension: 0
                        }]
                    },
                    options: wdOpts
                });
            }
        }
    }
}
</script>