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
            default: 'batteryData'
        },
        label: { default: 'Battery Level' }
    },

    watch: {
        'range': function() {
            if (this.chart && this.range) {
                this.chart.options.scales.yAxes[0].ticks.min = this.range.min;
                this.chart.options.scales.yAxes[0].ticks.max = this.range.max;
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
                    battOpts.scales.yAxes[0].ticks.min = this.range.min;
                    battOpts.scales.yAxes[0].ticks.max = this.range.max;
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
                battOpts.scales.yAxes[1].ticks.max = 256;
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
        }
    }
}
</script>