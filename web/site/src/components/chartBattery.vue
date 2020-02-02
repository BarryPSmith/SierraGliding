
<script>
import chartBase from './chartBase.vue';

export default {
    extends: chartBase,
    name: 'batteryChart',

    methods: {
        ensure_chart: function() {
            let battElem = document.getElementById('battery');
            if (battElem) {
                let battOpts = commonOptions;
                battOpts.scales.yAxes[0].ticks.beginAtZero = false;
                battOpts.scales.yAxes[0].ticks.min = this.station.batteryRange.min;
                battOpts.scales.yAxes[0].ticks.max = this.station.batteryRange.max;
                battOpts.plugins.zoom.pan.onPan = this.chart_panning;
                battOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                if (!this.charts.battery) {
                    this.charts.battery = new Chart(battElem, {
                        type: 'line',
                        data: {
                            datasets: [{
                                label: 'Battery Level',
                                pointBackgroundColor: 'black',
                                pointBorderColor: 'black',
                                pointRadius: 0,
                                borderColor: 'black',
                                borderJoinStyle: 'round',
                                fill: false,
                                data: this.dataManager.batteryData,
                                lineTension: 0
                            }]
                        },
                        options: battOpts
                    });
                } else {
                    this.chart.options.scales.yAxes[0].ticks.min = this.station.batteryRange.min;
                    this.chart.options.scales.yAxes[0].ticks.max = this.station.batteryRange.max;
                    this.chart.data.datasets[0].data = this.dataManager.batteryData;
                }
            }
        }
    }
}
</script>