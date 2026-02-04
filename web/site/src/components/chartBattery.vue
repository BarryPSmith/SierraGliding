<script>
import chartBase from './chartBase.vue';
import zoomPlugin from 'chartjs-plugin-zoom';
import filterPlugin from '../chart-filter.js';
import chartjsAnnotation from 'chartjs-plugin-annotation';

export default {
    extends: chartBase,
    name: 'batteryChart',

    mounted: function() {
        this.base_mounted();
    },

    props: {
        range : {},
        dataSource: {
            default: function() { return  ['batteryData', 'extBattData']; }
        },
        label: { default: 'Battery Level' }
    },

    watch: {
        'range': function() {
            if (this.chart && this.range) {
                for (let i = 0; i < this.dataSetCount; i++)
                {
                    this.chart.options.scales.yAxes[i].ticks.min = this.getRange(i).min;
                    this.chart.options.scales.yAxes[i].ticks.max = this.getRange(i).max;
                }
            }
        },

        'label': function() {
            if (this.chart) {
                this.chart.options.titles.text = this.label;
            }
        }
    },

    methods: {
        ensure_chart: function() {
            if (this.chart)
                return;

            let battElem = this.$refs.chart;

            //Chart.plugins.register(zoomPlugin);
            //Chart.plugins.register(filterPlugin);

            if (battElem) {
                let battOpts = this.commonOptions;
                battOpts.scales.yAxes[0].ticks.beginAtZero = false;
                if (this.range) {
                    battOpts.scales.yAxes[0].ticks.min = this.getRange(0).min;
                    battOpts.scales.yAxes[0].ticks.max = this.getRange(0).max;
                }
                battOpts.plugins.zoom.pan.onPan = this.chart_panning;
                battOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                battOpts.title.text = this.label;

                battOpts.scales.xAxes[0].afterFit = this.xAxis_afterFit;
                battOpts.scales.xAxes[0].afterUpdate = this.xAxis_afterUpdate;

                battOpts.scales.yAxes[1] = JSON.parse(JSON.stringify(battOpts.scales.yAxes[0]));
                battOpts.scales.yAxes[1].type = 'linear';
                battOpts.scales.yAxes[1].id = 'secondaryY';
                battOpts.scales.yAxes[1].ticks.display = false;
                if (this.range)
                {
                    battOpts.scales.yAxes[1].ticks.min = this.getRange(1).min;
                    battOpts.scales.yAxes[1].ticks.max = this.getRange(1).max;
                }
                battOpts.scales.yAxes[1].gridLines = {
                    display: false,
                };

                const ptColor = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ?
                    'cyan' : 'black';
                const ptColor2 = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ?
                    'rgba(255,255,0,0.5)' : 'rgba(0,255,0,0.5)';

                this.chart = new Chart(battElem, {
                    type: 'line',
                    data: {
                        allData: [],
                        datasets: [{
                            pointBackgroundColor: ptColor,
                            pointBorderColor: ptColor,
                            pointRadius: 0,
                            borderColor: ptColor,
                            borderJoinStyle: 'round',
                            fill: false,
                            //data: this.dataManager.batteryData,
                            lineTension: 0
                        }, {
                            pointBackgroundColor: ptColor2,
                            pointBorderColor: ptColor2,
                            pointRadius: 0,
                            borderColor: ptColor2,
                            borderJoinStyle: 'round',
                            fill: false,
                            //data: this.dataManager.batteryData,
                            lineTension: 0,
                            yAxisID: 'secondaryY',
                        }]
                    },
                    options: battOpts,
                    plugins: [zoomPlugin, chartjsAnnotation, filterPlugin],
                });
            }
        },

        getRange(idx)
        {
            if (!this.range || !this.range.length) {
                return this.range;
            }
            return this.range[idx];
        }
    }
}
</script>