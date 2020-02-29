<script>
import chartBase from './chartBase.vue';
import zoomPlugin from 'chartjs-plugin-zoom';
import filterPlugin from '../chart-filter.js';

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

            Chart.plugins.register(zoomPlugin);
            Chart.plugins.register(filterPlugin);

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
                this.chart = new Chart(battElem, {
                    type: 'line',
                    data: {
                        allData: [],
                        datasets: [{
                            pointBackgroundColor: 'black',
                            pointBorderColor: 'black',
                            pointRadius: 0,
                            borderColor: 'black',
                            borderJoinStyle: 'round',
                            fill: false,
                            //data: this.dataManager.batteryData,
                            lineTension: 0
                        }]
                    },
                    options: battOpts
                });
            }
        }
    }
}
</script>