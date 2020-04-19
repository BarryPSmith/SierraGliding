<template>
    <!--I don't know what all this div bullshit is about.
        But it seems to be necessary so the chart doesn't grow without bounds.-->
    <canvas ref="chart" />
</template>

<script>
import { EventBus } from '../eventBus.js';

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
                lastRange: new Date(),
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
                            maxRotation: 0,
                            min: this.cur_start(),
                            max: this.cur_end()
                        },
                        time: {
                            minUnit: 'minute'
                        }
                    }]
                },
                legend: {
                    display: false
                },
                title: {
                    display: true
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
            isPanning: false,
            timer: null,
            annotationSets: [],
        };
    },

    props: [ 'duration', 'chartEnd', 'dataManager', 'id' ],

    computed: {
        dataSetCount: function() {
            return (this.dataSource && this.dataSource.length)
                ? 
                this.dataSource.length
                :
                1;
        }
    },

    watch: {
        duration: function() { this.chart_range(); },
        chartEnd: function() {
            if (!this.isPanning) {
                this.chart_range();
            }
        },
        dataManager: function() {
            this.on_dataManager_changed();
        },
        dataSource: function() { this.update_chart(); }
    },

    methods: {
        base_mounted: function() {
            EventBus.$on('socket-message', this.on_socket_message);
            this.ensure_chart(); 
            this.on_dataManager_changed();
            this.chart_range();
            this.setup_timer();
        },

        setup_timer() {
            this.timer = setInterval(() => {
                if (this.chart) {
                    this.chart.options.plugins.zoom.pan.rangeMax.x = new Date();
                }
            }, 1000);
        },

        on_dataManager_changed: function() {
            if (!this.dataManager) {
                return;
            }
            this.dataManager.on('data_updated', this.update_chart);
            this.update_chart();
        },

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
            if (!this.chart)
                return;
            if (this.chart_range_derived)
                this.chart_range_derived();
            this.chart.options.scales.xAxes[0].ticks.min = this.cur_start();
            this.chart.options.scales.xAxes[0].ticks.max = this.cur_end();
            
            if (this.before_update)
                this.before_update();
            this.chart.update();
            this.lastRange = new Date();
        },

        chart_panning: function ({ chart }) {
            this.set_isPanning(true);
            const chartMax = chart.options.scales.xAxes[0].ticks.max;
            if (new Date() - chartMax < 10 * 1000)
                this.set_chartEnd(null);
            else
                this.set_chartEnd(chartMax);
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

            for (const idx in this.annotationSets) {
                this.annotationSets[idx][0].x = minX;
                this.annotationSets[idx][1].x = maxX;
            }
        },

        update_chart: function()
        {
            if (this.dataManager && this.chart)
            {
                if (this.update_chart_derived)
                    this.update_chart_derived();
                
                if (Array.isArray(this.dataSource)) {
                    for (const idx in this.dataSource) {
                        this.set_data(idx, this.dataSource[idx]);
                    }
                } else {
                    this.set_data(0, this.dataSource);
                }
                this.update_annotation_range();
                if (this.before_update)
                    this.before_update();
                this.chart.update();
            }
        },

        set_data: function(idx, ds)
        {
            if (this.map_data_point)
                this.chart.data.allData[idx] = this.dataManager[ds].map(this.map_data_point);
            else
                this.chart.data.allData[idx] = this.dataManager[ds];
        },

        on_socket_message: function(msg) {
            const idMatch = this.id == msg.id;
            const canRange = !this.chartEnd && !this.isPanning;
            const timeMatch = (+new Date() - this.lastRange) > 10000;
            if (canRange && (idMatch || timeMatch))
                this.chart_range();
        }
    }
}
</script>