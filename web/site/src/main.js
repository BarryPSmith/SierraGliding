import Vue from 'vue';
import App from './App.vue';

window.onload = () => {
    Vue.filter('number1', function(value) {
        if (typeof(value) != 'number') {
            return value;
        }
        return Intl.NumberFormat(undefined, { maximumFractionDigits: 1, minimumFractionDigits: 1}).format(value);
    });

    Vue.filter('number0', function(value) {
        if (typeof(value) != 'number') {
            return value;
        }
        return Intl.NumberFormat(undefined, { maximumFractionDigits: 0, minimumFractionDigits: 0}).format(value);
    });

    new Vue({
        el: '#app',
        render: h => h(App)
    });
}
