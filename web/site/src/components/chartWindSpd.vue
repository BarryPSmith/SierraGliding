<template>
    <!--I don't know what all this div bullshit is about.
        But it seems to be necessary so the chart doesn't grow without bounds.-->
    <div>
        <div display:block style="height:300px; max-height: 300px">
            <canvas ref="chart" />
        </div>
    </div>
</template>

<script>
import chartBase from './chartBase.vue';
import bs from '../../node_modules/binary-search/index.js';

export default {
    extends: chartBase,
    name: 'chartWindSpd',
    props: ['legend'],

    mounted: function() {
        this.ensure_chart();
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
                this.chart.data.datasets[0].label = 'Wind Speed (' + this.dataManager.unit + ')';
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
            let last = {};
            this.chart.options.annotation.annotations = [];
            for (const entry of this.legend) {
                last = {
                    type: 'box',
                    xScaleID: 'x-axis-0',
                    yScaleID: 'y-axis-0',
                    yMin: lastVal / factor,
                    yMax: entry.top / factor,
                    //xMax: maxX,
                    //xMin: minX,
                    backgroundColor: entry.color,
                    borderColor: 'rgba(0,0,0,0)',
                };
                this.chart.options.annotation.annotations.push(last);
                lastVal = entry.top;
            }
            //duplicate last and have it go to the moon:
            //The code below causes our Y-axis to go to 100 as soon as this is shown. We need to take manual control of the Y-axis if we want this to work.
            last = JSON.parse(JSON.stringify(last));
            last.yMin = last.yMax;
            last.yMax = null;
            this.chart.options.annotation.annotations.push(last);
        },

        ensure_chart: function () {
            if (this.chart)
                return;

            let wsElem = this.$refs.chart;

            var annotationPlugin = require('chartjs-plugin-annotation');
            var zoomPlugin = require('chartjs-plugin-zoom');
            var filterPlugin = require('../chart-filter.js');
            Chart.plugins.register(annotationPlugin);
            Chart.plugins.register(zoomPlugin);
            Chart.plugins.register(filterPlugin);


            if (wsElem) {
                let wsOpts = this.commonOptions;

                //Callbacks don't survive stringify/parse
                wsOpts.plugins.zoom.pan.onPan = this.chart_panning;
                wsOpts.plugins.zoom.pan.onPanComplete = this.chart_panComplete;
                wsOpts.scales.yAxes[0].ticks.stepSize = 5;
                const wsData = {
                    allData: [],
                    datasets: [{
                        label: 'Wind Speed',
                        pointBackgroundColor: 'black',
                        pointBorderColor: 'black',
                        pointRadius: 0,
                        borderColor: 'black',
                        borderJoinStyle: 'round',
                        borderWidth: 0.2,
                        fill: 'end',
                        backgroundColor: 'rgba(255, 255, 255, 0.7)',
                        //data: this.dataManager.windspeedData,
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