<script>
import chartBase from './chartBase.vue';
import zoomPlugin from 'chartjs-plugin-zoom';
import filterPlugin from '../chart-filter.js';

export default {
    extends: chartBase,
    name: 'chartWindDir',

    props: { 
        legend: {},
        centre: {
            default: 180
        }
    },

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
        },
        'centre': function() {
            this.chart.options.scales.yAxes[0].ticks.max = this.centre + 180;
            this.chart.options.scales.yAxes[0].ticks.max = this.centre + 180;
            this.update_chart();
        }
    },

    methods: {
        set_direction_annotations: function () {
            if (!this.chart || !this.legend) {
                return;
            }

            //Clear the existing annotation sets:
            //this.chart.options.annotation.annotations = [];
            this.chart.data.allData.splice(1, this.annotationSets.length);
            this.chart.data.datasets.splice(1, this.annotationSets.length);
            this.annotationSets = [];

            var descriptors = [];
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
                    descriptors.push({
                        borderWidth: 0,
                        pointRadius: 0,
                        fill: false
                    });
                    descriptors.push({
                        borderWidth: 0,
                        pointRadius: 0,
                        fill: '-1',
                        backgroundColor: subEntry.color
                    });
                    this.annotationSets.push(
                        [
                            {
                                x: 0,
                                y: subEntry.start
                            }, {
                                x: 0,
                                y: subEntry.start
                            }
                        ], [
                            {
                                x: 0,
                                y: subEntry.end
                            }, {
                                x: 0,
                                y: subEntry.end
                            }
                        ]
                    );
                }
            }

            this.chart.data.allData.splice(1, 0, ...this.annotationSets);
            this.chart.data.datasets.splice(1, 0, ...descriptors);
        },

        set_direction_range: function() {
            if (!this.chart)
                return;
        },

        map_data_point: function(pt) {
            if (pt.y < this.chart.options.scales.yAxes[0].ticks.min)
                pt.y += 360;
            if (pt.y > this.chart.options.scales.yAxes[0].ticks.max)
                pt.y -= 360;
            return pt;
        },

        ensure_chart: function () {
            if (this.chart)
                return;

            let wdElem = this.$refs.chart;

            Chart.plugins.register(zoomPlugin);
            Chart.plugins.register(filterPlugin);

            if (wdElem) {
                let wdOpts = this.commonOptions;
                wdOpts.plugins.zoom.pan.onPan = this.chart_panning;
                wdOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                wdOpts.scales.yAxes[0].ticks.max = this.centre + 180;
                wdOpts.scales.yAxes[0].ticks.min = this.centre - 180;
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
                wdOpts.scales.yAxes[0].afterBuildTicks = scale => {
                    let ticks = [];
                    for (let deg = Math.ceil(scale.min / 45) * 45;
                        deg <= Math.floor(scale.max / 45) * 45;
                        deg += 45) {
                        ticks.push(deg);
                    }
                    scale.ticks = ticks;
                };
                wdOpts.scales.yAxes[0].ticks.callback = value => {
                    const ret = windNames[(value + 360) % 360];
                    if (ret !== undefined)
                        return ret;
                    else
                        return value;
                };
                wdOpts.title.text = 'Wind Direction';
                this.chart = new Chart(wdElem, {
                    type: 'line',
                    data: {
                        allData: [],
                        datasets: [{
                            //label: 'Wind Direction',
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