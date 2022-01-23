<template>
    <div>
        <span class="fr px12">
            <select v-model="duration">
                <option value="300">5 Minutes</option>
                <option value="900">15 Minutes</option>
                <option value="3600">1 Hour</option>
                <option value="14400">4 Hours</option>
                <option value="43200">12 Hours</option>
                <option value="86400">24 Hours</option>
                <option value="604800">1 Week</option>
                <option value="2592000">1 Month</option>
            </select>
            <button class="btn btn--s ml6" @click="jumping=true" v-if="!jumping">Jump</button>
            <input v-if="jumping" class="input--s mt3 ml6" type="datetime-local" v-model="endDate" />
        </span>
        <div class='h-full w-full'>
            <div class='align-center border-b border--gray h36 my6'>
                <h1 class='txt-h4'>Stations</h1>
            </div>
            <singleStationView v-for='cstation in stations'
                               :key="cstation.id"
                               :station="cstation"
                               :duration="duration"
                               v-bind:chartEnd.sync="chartEnd" />
        </div>
    </div>
</template>
<script>
import singleStationView from './singleStationView.vue';
import debounce from 'debounce';

export default {
    name: 'stationList',
    components: { singleStationView },
    props: [ 'stations' ],
    data: function() { 
        return {
            chartEnd: null,
            duration: 900,
            jumping: false,
        };
    },
    created() {
        this.setHash = debounce(this.setHash, 200);
    },
    mounted: function () {
        this.loadHash();
    },
    watch: {
        'chartEnd': function() { 
            this.$emit('update:chartEnd', this.chartEnd);
            this.setHash();
        },
        duration() {
            this.setHash();
        }
    },
    computed: {
        dur300: function() {
            return {
                'btn--stroke': this.duration === 300
            }
        },
        dur3600: function() {
            return {
                'btn--stroke': this.duration === 3600
            }
        },
        dur14400: function() {
            return {
                'btn--stroke': this.duration === 14400
            }
        },
        dur42200: function() {
            return {
                'btn--stroke': this.duration === 42200
            }
        },
        dur84400: function() {
            return {
                'btn--stroke': this.duration === 84400
            }
        },
        endDate: {
            get() {
                if (this.chartEnd) {
                    return new Date(this.chartEnd).toDatetimeLocal();
                } else {
                    return null;
                }
            },
            set(value) {
                if (value) {
                    this.chartEnd = Date.parse(value);
                    return;
                    const date = Date(value);
                    this.chartEnd = +date + (date.getTimezoneOffset() * 60000);
                }
            },
        },
    },

    methods: {
        loadHash() {
            const hash = window.location.hash.slice(1)
            const searchParams = new URLSearchParams(hash);
            const chartEndStr = searchParams.get('chartEnd');
            const lastHashStr = searchParams.get('lastHash');
            if (chartEndStr && !Number.isNaN(+chartEndStr) &&
                lastHashStr && !Number.isNaN(+lastHashStr) && 
                (+new Date() - (+lastHashStr) < 1800000)) {
                this.chartEnd = +chartEndStr;
                this.jumping = true;
            }
            const durationStr = searchParams.get('duration');
            if (durationStr && !Number.isNaN(+durationStr)) {
                this.duration = +durationStr;
            }
        },
        setHash() {
            const hash = window.location.hash.slice(1)
            const searchParams = new URLSearchParams(hash); 
            if (this.chartEnd && this.jumping) {
                searchParams.set('chartEnd', this.chartEnd);
                searchParams.set('lastHash', +new Date());
            } else {
                searchParams.delete('chartEnd');
                searchParams.delete('lastHash');
            }
            searchParams.set('duration', this.duration);
            //window.location.hash=searchParams;
            history.replaceState(null, null, `#${searchParams}`);
        }
    }
}
</script>