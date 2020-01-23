<template>
    <div something or the other>
        <div v-if='!station.error' class="flex-parent-inline fr px24">

            <button @click='duration =   300' :class='dur300' class="btn btn--pill btn--pill-hl">5 Minutes</button>
            <button @click='duration =  3600' :class='dur3600' class="btn btn--pill btn--pill-hc">1 Hour</button>
            <button @click='duration = 14400' :class='dur14400' class="btn btn--pill btn--pill-hc">4 Hours</button>
            <button @click='duration = 42200' :class='dur42200' class="btn btn--pill btn--pill-hc">12 Hours</button>
            <button @click='duration = 84400' :class='dur84400' class="btn btn--pill btn--pill-hr">24 Hours</button>
        </div>

        <div v-show='dataType === "wind"' class='splitGrid'>
            <div>
                <canvas id="windspeed"></canvas>
            </div>
            <div>
                <canvas id="wind_direction"></canvas>
            </div>
        </div>

        <div v-show='dataType === "battery"'>
            <canvas id="battery"></canvas>
        </div>
    </div>
</template>

<script>
export default {
    name: 'stationCharts',
    render: h => h(App),
    methods: {},
    props: ['dataManager', 'station'],
    data: function() {
        return {
            ws: false,
            duration: 300,
            availableDurations: [
                { title: "5 Minutes", duration: 300 },
                { title: "1 Hour", duration: 3600 },
                { title: "4 Hours", duration: 14400 },
                { title: "12 Hours", duration: 42200 },
                { title: "24 Hours", duration: 84400 }
            ],
            isPanning: false,
            dataType: 'wind',
            charts: {
                windspeed: false,
                wind_direction: false,
                battery: false,
            },
            timer: false
        }
    },
    watch: {
        'duration': function () {
            const dmPromise = this.dataManager.ensure_data(
                this.cur_start(), this.chartEnd
            );
            this.chart_range();
            this.station.loading = true;
            dmPromise.then(obj => {
                this.update_annotation_ranges();
                if (obj.anyChange) {
                    this.set_windspeed_range();
                    this.station_data_update(obj.reloadedData);
                }
                this.station.loading = false;
                this.station_data_update();
            });
        },
        'dataType': function() {
            this.station_update(true);
            this.chart_range();
        }
    },
    mounted: function(e) {
        if (!this.timer) {
            this.timer = setInterval(() => {
                for (const chart of [
                    this.charts.windspeed,
                    this.charts.wind_direction,
                    this.charts.battery
                ]) {
                    if (chart) {
                        chart.options.plugins.zoom.pan.rangeMax.x = new Date();
                    }
                }
                /*if (!this.chartEnd && !this.isPanning) {
                    this.chart_range();
                }*/
            }, 1000);
        }


    },
    methods: {
        set_windspeed_range: function () {
            const dataset = this.dataManager.windspeedData;
            const chart = this.charts.windspeed;
            const start = this.cur_start();
            const end = this.cur_end();
            const minMax = this.station.legend.windspeed[this.station.legend.windspeed.length - 1].top / (this.dataManager.unit == 'mph' ? 1.6 : 1.0);
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

        cur_end: function () {
            if (this.chartEnd)
                return this.chartEnd;
            else
                return new Date();
        },

        cur_start: function () {
            return this.cur_end() - this.duration * 1000;
        },

        chart_range: function (chartToExclude) {
            const charts = this.dataType == 'wind' ?
                [
                this.charts.windspeed,
                this.charts.wind_direction,
                ] : [
                this.charts.battery
                ];

            if (this.dataType == 'wind')
                this.set_windspeed_range(); 
            for (const chart of charts) {
                if (chart && chart != chartToExclude) {
                    chart.options.scales.xAxes[0].time.min = this.cur_start();
                    chart.options.scales.xAxes[0].time.max = this.cur_end();
                    chart.update();
                }
            }               
        },

        chart_panning: function ({ chart }) {
            this.isPanning = true;
            const chartMax = chart.options.scales.xAxes[0].ticks.max;
            if (new Date() - chartMax < 10 * 1000)
                this.chartEnd = null;
            else
                this.chartEnd = chartMax;
            this.dataManager.ensure_data(this.cur_start(), this.cur_end())
                .then(obj => {
                    if (obj.anyChange) {
                        this.set_windspeed_range();
                        this.update_annotation_ranges();
                        this.station_data_update(obj.reloadedData);
                    }
            });
            this.chart_range(chart);
        },

        chart_panComplete: function ({ chart }) {
            this.isPanning = false;
        },

        set_direction_annotations: function() {
            if (!this.station.legend.winddir) {
                return;
            }

            this.charts.wind_direction.options.annotation.annotations = [];
                      
            for (const entry of this.station.legend.winddir) {
                if (entry.start === undefined 
                    || entry.end === undefined
                    || entry.color === undefined) {
                    continue;
                }
                let subEntries = [entry];
                if (entry.start > entry.end) {
                    subEntries = [ {
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
                    this.charts.wind_direction.options.annotation.annotations.push({
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

        set_speed_annotations: function() {
            let lastVal = 0;
            const factor = this.dataManager.unit == 'mph' ? 1.6 : 1.0;
            let last = {};
            this.charts.windspeed.options.annotation.annotations = [];
            for (const entry of this.station.legend.windspeed)
            {
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
                this.charts.windspeed.options.annotation.annotations.push(last);
                lastVal = entry.top;
            }
            //duplicate last and have it go to the moon:
            //The code below causes our Y-axis to go to 100 as soon as this is shown. We need to take manual control of the Y-axis if we want this to work.
            last = JSON.parse(JSON.stringify(last));
            last.yMin = last.yMax;
            last.yMax = null;
            this.charts.windspeed.options.annotation.annotations.push(last);
            //this.charts.windspeed.options.scales.yAxes[0].max = last.yMin;
        },

        update_annotation_ranges: function () {
            if (this.dataManager.stationData.length == 0)
                return;

            const minX = this.dataManager.stationData[0].timestamp * 1000;
            const maxX = this.dataManager.stationData[this.dataManager.stationData.length - 1].timestamp * 1000;

            if (this.charts.windspeed) {
                for (const idx in this.charts.windspeed.options.annotation.annotations) {
                    this.charts.windspeed.options.annotation.annotations[idx].xMin = minX;
                    this.charts.windspeed.options.annotation.annotations[idx].xMax = maxX;
                }
            }
            if (this.charts.wind_direction) {
                for (const idx in this.charts.wind_direction.options.annotation.annotations) {
                    this.charts.wind_direction.options.annotation.annotations[idx].xMin = minX;
                    this.charts.wind_direction.options.annotation.annotations[idx].xMax = maxX;
                }
            }
        },
    }
}
</script>