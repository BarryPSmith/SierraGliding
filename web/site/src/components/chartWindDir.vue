<script>
import chartBase from './chartBase.vue';
import zoomPlugin from 'chartjs-plugin-zoom';
import filterPlugin from '../chart-filter.js';
import chartjsAnnotation from 'chartjs-plugin-annotation';

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
            this.chart.options.scales.yAxes[0].ticks.min = this.centre - 180;
            //this.update_chart();
            this.set_direction_annotations();
        }
    },

    methods: {
        set_direction_annotations: function () {
            if (!this.chart || !this.legend) {
                return;
            }

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
                let subEntries = null;
                const start = this.map_y(entry.start);
                const end = this.map_y(entry.end);
                if (start > end) {
                    subEntries = [{
                        start: 0,
                        end: end,
                        color: entry.color
                    }, {
                        start: start,
                        end: 360,
                        color: entry.color
                    }];
                }
                else {
                    subEntries = [{
                        start: start,
                        end: end,
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
            this.update_chart();
        },

        set_direction_range: function() {
            if (!this.chart)
                return;
        },

        map_data_point: function(pt) {
            pt.y = this.map_y(pt.y);
            return pt;
        },

        map_y: function(y) {
            if (y < this.chart.options.scales.yAxes[0].ticks.min)
                y += 360;
            if (y > this.chart.options.scales.yAxes[0].ticks.max)
                y -= 360;
            return y;
        },

        ensure_chart: function () {
            if (this.chart)
                return;

            let wdElem = this.$refs.chart;

            //Chart.plugins.register(zoomPlugin);
            //Chart.plugins.register(filterPlugin);

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
                wdOpts.scales.xAxes[0].afterFit = this.xAxis_afterFit;
                wdOpts.scales.xAxes[0].afterUpdate = this.xAxis_afterUpdate;
                /*wdOpts.scales.xAxes[0].beforeBuildTicks = scale => {
                    console.error(scale);
                };*/
                wdOpts.scales.yAxes[0].ticks.callback = value => {
                    const ret = windNames[(value + 360) % 360];
                    if (ret !== undefined)
                        return ret;
                    else
                        return value;
                };
                wdOpts.title.text = 'Wind Direction';
                const ptColor = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ?
                    'cyan' : 'black';
                this.chart = new Chart(wdElem, {
                    type: 'line',
                    data: {
                        allData: [],
                        datasets: [{
                            //label: 'Wind Direction',
                            pointBackgroundColor: ptColor,
                            pointBorderColor: 'transparent',
                            pointBorderWidth: 0,
                            pointRadius: 2,
                            borderColor: 'black',
                            showLine: false,
                            fill: false,
                            //data: this.dataManager.windDirectionData,
                            lineTension: 0
                        }]
                    },
                    options: wdOpts,
                    plugins: [zoomPlugin, chartjsAnnotation, filterPlugin],
                });
            }
        },

        before_update: function() {
            this.set_points();
        },

        set_points: function() {
            if (!this.chart || !this.dataManager || !this.dataManager.currentSampleInterval
                || !this.duration) {
                return;
            }

            const maxPtRadius = 2;
            const minPtRadius = 2;
            
            let pointOpacity = 1;
            
            const wdElem = this.$refs.chart;
            if (!wdElem) {
                return;
            }
            const width = wdElem.clientWidth;
            let desiredPtRadius = width * 
                Math.max(this.dataManager.currentSampleInterval, this.dataManager.stationDataRate) / this.duration;

            if (desiredPtRadius < minPtRadius) {
                pointOpacity = 0.3 + 0.7 * desiredPtRadius / minPtRadius;
                if (pointOpacity > 1)
                    pointOpacity = 1;
                desiredPtRadius = minPtRadius;
            } else if (desiredPtRadius > maxPtRadius) {
                desiredPtRadius = maxPtRadius;
            }

            this.chart.data.datasets[0].pointRadius = desiredPtRadius;

            const ptColor = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ?
                'rgba(0, 255, 255, ' : 'rgba(0, 0, 0, ';

            this.chart.data.datasets[0].pointBackgroundColor = ptColor + pointOpacity + ')';
            this.chart.data.datasets[0].pointBorderColor = ptColor + ' 0.1)';
        }
    }
}
</script>
