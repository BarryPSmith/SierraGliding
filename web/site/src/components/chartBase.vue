<template>
    <!--I don't know what all this div bullshit is about.
        But it seems to be necessary so the chart doesn't grow without bounds.-->
    <div style="height: 100%;" ref="chartContainer">
        <canvas ref="chart" class="pany" />
    </div>
</template>

<style scoped>
    .pany {
        touch-action: pan-y !important;
    }
</style>

<script>
import { EventBus } from '../eventBus.js';
import SunCalc from 'suncalc';

export default {
    name: 'chartBase',

    data: function() {
        const lineColor = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ?
            'rgb(205, 205, 205, 0.1)' : 'rgb(0, 0, 0, 0.1)';
        return {
            lastRange: new Date(),
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
                        position: 'right',
                        gridLines: {
                            color: lineColor
                        },
                    }],
                    xAxes: [{
                        id: 'x',
                        type: 'time',
                        offset: false,
                        ticks: {
                            maxRotation: 0,
                            min: this.cur_start(),
                            max: this.cur_end(),
                            autoSkip: false
                        },
                        time: {
                            minUnit: 'minute',
                            displayFormats: {
                                hour: 'MMM D hA'
                            },
                        gridLines: {
                            color: lineColor
                            },
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
                },
                annotation: {
                    drawTime: 'afterDatasetsDraw',
                },
            },
            isPanning: false,
            timer: null,
            annotationSets: [],
            chartHeight: null
        };
    },

    props: [ 'duration', 'chartEnd', 'dataManager', 'station'],

    computed: {
        dataSetCount: function() {
            return (this.dataSource && this.dataSource.length)
                ? 
                this.dataSource.length
                :
                1;
        },
        id() {
            return this.station.id;
        },
        lat() {
            return this.station.lat;
        },
        lon() {
            return this.station.lon;
        },
        elevation() {
            return this.station.elevation || 2000;
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
                if (this.$refs.chartContainer && this.chart) {
                    const height = this.$refs.chartContainer.clientHeight;
                    const container = this.$refs.chartContainer;
                    const chartHeight = this.chartHeight;
                    if (this.chartHeight != height)
                        this.chart.resize();
                    this.chartHeight = height;
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

            this.do_daylight_annotations();
            if (this.before_update)
                this.before_update();
            this.chart.update();
            this.lastRange = new Date();
        },

        filter_x_ticks: function(scale, ticks) {
            const hours = 3600;
            const days = 24 * hours;
            const timeSteps = [
                {
                    min: 0,
                    unit: false,
                    stepSize: 1,
                    max: 12 * hours
                },
                {
                    unit: 'hour',
                    stepSize: 1,
                    max: 1 * days
                },
                {
                    unit: 'hour',
                    stepSize: 3,
                    max: 2 * days
                },
                {
                    unit: 'hour',
                    stepSize: 6,
                    max: 4 * days
                },
                {
                    unit: 'hour',
                    stepSize: 12,
                    max: 7 * days
                },
                {
                    unit: 'day',
                    stepSize: 1,
                    max: 31 * days
                },
                {
                    unit: false,
                    stepSize: 1,
                    max: Infinity
                }
            ]

            for (let i = 0; i <= timeSteps.length; i++) {
                const step = timeSteps[i];
                if (this.duration < step.max) {
                    this.chart.options.scales.xAxes[0].time.unit = step.unit;
                    this.chart.options.scales.xAxes[0].time.stepSize = step.stepSize;
                    break;
                }
            }
        },

        chart_panning: function ({ chart }) {
            this.set_isPanning(true);
            const chartMax = chart.options.scales.xAxes[0].ticks.max;
            if (new Date() - chartMax < 10 * 1000)
                this.set_chartEnd(null);
            else
                this.set_chartEnd(chartMax);
            this.do_daylight_annotations();
        },
        
        chart_panComplete: function ({ chart }) {
            this.set_isPanning(false);
            if (this.before_update)
                this.before_update();
            // We need an update here because chart_panning fires after the paint,
            // and we need to ensure that the proper daylight annotations are drawn.
            this.chart.update();
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

        do_daylight_annotations()
        {
            const start = this.cur_start();
            const end = this.cur_end();
            const annotations = [];
            let lastSunset = start;
            const oneDay = 86400000;
            if (end - start < oneDay * 100) {
                for (let day = start; day < +end + 2 * oneDay; day += oneDay) {
                    const times = SunCalc.getTimes(day, this.lat, this.lon, this.elevation);
                    if (times.sunrise > start) {
                        annotations.push({
                            type: 'box',
                            xMin: lastSunset,
                            xMax: times.sunrise,
                            xScaleID: 'x',
                            borderWidth: 0,
                            borderColor: 'transparent',
                            backgroundColor: 'rgba(0,0,0, 0.1)'
                        });
                    }
                    lastSunset = times.sunset;
                }
            }
            this.chart.options.annotation.annotations = annotations;
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
        },

        // ============ Functions to get decent datetime behaviour from chartjs
        // These are highly dependent on the internals of the chartjs library
        // But they work in the version we build against
        // and it's easier than trying to actually fix it in that library.
        xAxis_afterFit: function(scale) {
            scale.originalTicks = scale._ticks;
            const increase = scale.margins.left + scale.margins.right - 6;
            scale.margins.right = scale.margins.left = 3;
            scale.paddingRight = scale.paddingLeft = 3;
            scale.width += increase;
            scale._length += increase;
        },

        xAxis_afterUpdate: function(scale) {
            const sizes = scale._getLabelSizes();
            const widest = sizes.widest;
            const amtPerPx = (scale.max - scale.min) / scale._length;
            const amtPerLAbel = amtPerPx * widest.width;
            const hr = 3600 * 1000;
            const allowedSpacings = [
                //1, 5, 30 seconds
                1000, 5000, 30000,
                //1, 2, 3, 5 minutes
                60000, 120000, 180000, 300000,
                //10, 20, 30 minutes
                600000, 1200000, 1800000,
                //1, 3, 6, 12, 24 hours
                hr, 3 * hr, 6 * hr, 12 * hr, 24 * hr
            ]
            if (amtPerLAbel > allowedSpacings[allowedSpacings.length - 1]) {
                return;
            }
            let filterAmt = null;
            for (let i = 0; i < allowedSpacings.length; i++) {
                if (allowedSpacings[i] >= amtPerLAbel) {
                    filterAmt = allowedSpacings[i];
                    break;
                }
            }
            if (filterAmt) {
                for (let i = 0; i < scale.originalTicks.length; i++) {
                    let tick = scale.originalTicks[i];
                    if ((tick.value - new Date().getTimezoneOffset() * 60000) % filterAmt == 0) {
                        tick._index = i;
                    }
                }
                scale._ticksToDraw = scale.originalTicks.filter(t => {
                    return (t.value - new Date().getTimezoneOffset() * 60000) % filterAmt == 0;
                });
                scale.ticks = scale._convertTicksToLabels(scale._ticksToDraw);
            }
        }
    }
}
</script>