<template>
    <div something or the other>
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
    props: ['station'],
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
            legend: {
                windspeed: [],
                winddir: []
            },
            timer: false,
            dataManager: null,
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
                }
            }               
        },

        on_socketMessage: function() {
            this.update_annotation_ranges();
            if (!this.chartEnd && !this.isPanning) {
                this.chart_range();
            } else {
                this.station_data_update();
            }
        },

        station_data_update: function (refreshRequired) {
            if (refreshRequired) {
                if (this.charts.windspeed && this.dataType == 'wind') {
                    this.charts.windspeed.data.datasets[0].data = this.dataManager.windspeedData;
                }
                if (this.charts.wind_direction && this.dataType == 'wind') {
                    this.charts.wind_direction.data.datasets[0].data = this.dataManager.windDirectionData;
                }
                if (this.charts.battery && this.dataType == 'battery') {
                    this.charts.battery.data.datasets[0].data = this.dataManager.batteryData;
                }
            }
            
            if (this.charts.windspeed && this.dataType == 'wind') {
                this.charts.windspeed.update();
            }
            if (this.charts.wind_direction && this.dataType == 'wind') {
                this.charts.wind_direction.update();
            }
            if (this.charts.battery && this.dataType == 'battery') {
                this.charts.battery.update();
            }
        },
    }
}
</script>