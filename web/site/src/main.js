import Vue from 'vue';
import App from './App.vue';

// https://webreflection.medium.com/using-the-input-datetime-local-9503e7efdce
Date.prototype.toDatetimeLocal =
  function toDatetimeLocal() {
    var
      date = this,
      ten = function (i) {
        return (i < 10 ? '0' : '') + i;
      },
      YYYY = date.getFullYear(),
      MM = ten(date.getMonth() + 1),
      DD = ten(date.getDate()),
      HH = ten(date.getHours()),
      II = ten(date.getMinutes()),
      SS = ten(date.getSeconds())
    ;
    return YYYY + '-' + MM + '-' + DD + 'T' +
             HH + ':' + II + ':' + SS;
  };

window.onload = () => {
    Vue.filter('number1', function(value) {
        if (value === null || value === undefined)
            return '--';
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
