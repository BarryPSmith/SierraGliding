<script>
import chartBase from './chartBase.vue';
import bs from '../../node_modules/binary-search/index.js';
import zoomPlugin from 'chartjs-plugin-zoom';
import filterPlugin from '../chart-filter.js';

export default {
    extends: chartBase,
    name: 'chartWindSpd',
    props: ['legend'],

    mounted: function() {
        this.base_mounted();
        this.set_speed_annotations();
        this.set_windspeed_range();
    },

    data: function() {
        return {
            dataSource: 'windspeedData'
        };
    },

    watch: {
        'legend': function() {
            this.set_speed_annotations();
            this.set_windspeed_range();
        },

        'dataManager': function() {
            if (this.chart)
            {
                this.chart.data.datasets[0].label = 'Wind Speed (' + this.dataManager.unit + ')';
                this.set_speed_annotations();
            }
        }
    },

    methods: {
        set_windspeed_range: function () {
            if (!this.dataManager || !this.chart || !this.legend)
                return;

            const dataset = this.dataManager.windspeedData;
            const chart = this.chart;
            const start = this.cur_start();
            const end = this.cur_end();
            const minMax = this.legend[this.legend.length - 1].top / (this.dataManager.unit == 'mph' ? 1.6 : 1.0);
            let startIdx = bs(dataset, start, (element, needle) => element.x - needle);
            let endIdx = bs(dataset, end, (element, needle) => element.x - needle);
            if (startIdx < 0)
                startIdx = ~startIdx;
            if (endIdx < 0)
                endIdx = ~endIdx;

            let max = minMax
            for (let i = startIdx; i < endIdx && i < dataset.length; i++) {
                if (dataset[i].y > max)
                    max = dataset[i].y;
            }
            max = 5 * Math.ceil(max / 5);
            chart.options.scales.yAxes[0].ticks.max = max;
        },

        chart_range_derived: function() { this.set_windspeed_range(); },
        update_chart_derived: function() { this.set_windspeed_range(); },

        set_speed_annotations: function () {
            if (!this.dataManager || !this.chart)
                return;

            let lastVal = 0;
            const factor = this.dataManager.unit == 'mph' ? 1.6 : 1.0;
            let last = null;

            this.chart.data.allData.splice(1, this.annotationSets.length);
            this.chart.data.datasets.splice(1, this.annotationSets.length);
            this.annotationSets = [];

            var descriptors = [];

            var isFirst = true;
            for (const entry of this.legend) {
                last = entry;
                descriptors.push({
                    borderWidth: 0,
                    pointRadius: 0,
                    fill: isFirst ? 'origin' : '-1',
                    backgroundColor: entry.color,
                    borderColor: 'rgba(0,0,0,0)',
                });
                this.annotationSets.push([
                    {
                        x: 0,
                        y: entry.top / factor
                    }, {
                        x: 0,
                        y: entry.top / factor
                    }
                ]);
                isFirst = false;
            }
            if (last != null) {
                descriptors.push({
                    borderWidth: 0,
                    pointRadius: 0,
                    fill: 'end',
                    backgroundColor: last.color,
                    borderColor: 'rgba(0,0,0,0)'
                });
                this.annotationSets.push([
                    {
                        x: 0,
                        y: last.top / factor
                    }, {
                        x: 0,
                        y: last.top / factor
                    }
                ]);
            }

            this.chart.data.allData.splice(1, 0, ...this.annotationSets);
            this.chart.data.datasets.splice(1, 0, ...descriptors);
        },

        ensure_chart: function () {
            if (this.chart)
                return;

            let wsElem = this.$refs.chart;

            Chart.plugins.register(zoomPlugin);
            Chart.plugins.register(filterPlugin);


            if (wsElem) {
                let wsOpts = this.commonOptions;

                //Callbacks don't survive stringify/parse
                wsOpts.plugins.zoom.pan.onPan = this.chart_panning;
                wsOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                wsOpts.scales.yAxes[0].ticks.stepSize = 5;
                wsOpts.title.text = 'Wind Speed';
                const wsData = {
                    allData: [],
                    datasets: [{
                        pointBackgroundColor: 'black',
                        pointBorderColor: 'black',
                        pointRadius: 0,
                        borderColor: 'black',
                        borderJoinStyle: 'round',
                        borderWidth: 0.2,
                        fill: 'end',
                        backgroundColor: 'rgba(255, 255, 255, 0.7)',
                        lineTension: 0
                    }]
                };
                
                this.chart = new Chart(wsElem, {
                    type: 'line',
                    data: wsData,
                    options: wsOpts
                });
            }
        }
    }
}
</script>