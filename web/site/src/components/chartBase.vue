<script>
export default {
    name: 'chartBase',

    data: function() {
        return {
            commonOptions: {
                animation: {
                    duration: 0,
                    easing: 'linear'
                },
                hover: {
                    animationDuration: 0
                },
                responsive: true,
                responsiveAnimationDuration: 0,
                maintainAspectRatio: false,
                scales: {
                    yAxes: [{
                        ticks: {
                            min: 0
                        },
                        position: 'right'
                    }],
                    xAxes: [{
                        type: 'time',
                        offset: false,
                        ticks: {
                            maxRotation: 0
                        },
                        time: {
                            minUnit: 'minute',
                            min: this.cur_start(),
                            max: this.cur_end()
                        }
                    }]
                },
                annotation: {
                        annotations: [],
                        drawTime: 'beforeDatasetsDraw'
                },
                        
                plugins: {
                    zoom: {
                        pan: {
                            enabled: true,
                            mode: 'x',
                            rangeMax: {
                                x: new Date()
                            },
                        }
                    }
                }
            },
            isPanning: false
        };
    },

    props: [ 'duration', 'chartEnd', 'dataManager' ],

    watch: {
        duration: function() { this.chart_range(); },
        chartEnd: function() {
            if (!this.isPanning) {
                this.chart_range();
            }
        },
        dataManager: function() {
            this.dataManager.on('data_updated', this.update_chart);
            this.update_chart();
        }
    },

    methods: {
        set_chartEnd: function(newVal) {
            if (this.chartEnd === newVal) {
                return;
            }
            this.chartEnd = newVal;
            this.$emit('update:chartEnd', this.chartEnd);
        },

        set_isPanning: function(newVal) {
            if (this.isPanning === newVal) {
                return;
            }
            this.isPanning = newVal;
            this.$emit('update:isPanning', this.isPanning);
        },

        cur_end: function () {
            if (this.chartEnd) {
                return this.chartEnd;
            } else {
                return new Date();
            }
        },

        cur_start: function () {
            return this.cur_end() - this.duration * 1000;
        },

        chart_range: function() {
            if (this.chart_range_derived)
                this.chart_range_derived();
            this.chart.options.scales.xAxes[0].time.min = this.cur_start();
            this.chart.options.scales.xAxes[0].time.max = this.cur_end();
            this.chart.update();
        },

        chart_panning: function ({ chart }) {
            this.set_isPanning(true);
            const chartMax = chart.options.scales.xAxes[0].ticks.max;
            if (new Date() - chartMax < 10 * 1000)
                this.set_chartEnd(null);
            else
                this.set_chartEnd(chartMax);
            /*this.dataManager.ensure_data(this.cur_start(), this.cur_end())
                .then(obj => {
                    if (obj.anyChange) {
                        this.set_windspeed_range();
                        this.update_annotation_ranges();
                        this.station_data_update(obj.reloadedData);
                    }
            });
            this.chart_range(chart);*/
        },
        
        chart_panComplete: function ({ chart }) {
            this.set_isPanning(false);
        },

        update_annotation_range()
        {
            if (this.dataManager.stationData.length == 0) {
                return;
            }

            const minX = this.dataManager.stationData[0].timestamp * 1000;
            const maxX = this.dataManager.stationData[this.dataManager.stationData.length - 1].timestamp * 1000;

            for (const idx in this.chart.options.annotation.annotations) {
                this.chart.options.annotation.annotations[idx].xMin = minX;
                this.chart.options.annotation.annotations[idx].xMax = maxX;
            }
        },

        update_chart()
        {
            if (this.dataManager && this.chart)
            {
                if (this.update_chart_derived)
                    this.update_chart_derived();
                this.chart.data.allData[0] = this.dataManager[this.dataSource];
                this.update_annotation_range();
                this.chart.update();
            }
        }
    }
}
</script>