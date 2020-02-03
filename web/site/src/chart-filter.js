// Found this from https://github.com/chartjs/chartjs-plugin-zoom/issues/75

// Keep the real data in a seperate object called allData
// Put only that part of allData in the dataset to optimize zoom/pan performance
// Author: Evert van der Weit - 2018

function filterData(chartInstance) {
    var datasets = chartInstance.data.datasets;
    var originalDatasets = chartInstance.data.allData;
    var chartOptions = chartInstance.options.scales.xAxes[0];

    var startX = chartOptions.time.min
    var endX = chartOptions.time.max

    for(var i = 0; i < originalDatasets.length; i++) {
        var dataset = datasets[i];
        var originalData = originalDatasets[i];
        
        if (!originalData.length) break

        var s = startX;
        var e = endX;
        var sI = null;
        var eI = null;

        for (var j = 0; j < originalData.length; j++) {
            if ((sI==null) && originalData[j].x > s) {
                sI = j
            }
            if ((eI==null) && originalData[j].x > e) {
                eI = j
            }
        }
        if (sI==null || sI == 0) {
            sI = 0;
        } else {
            sI--;
        }
        if (originalData[originalData.length - 1].x < s) {
            eI = 0;
        } else if (eI==null || eI == originalData.length) {
            eI = originalData.length;
        } else {
            eI++;
        }

        dataset.data = originalData.slice(sI, eI)
    }
}

var datafilterPlugin = {
    beforeUpdate: function(chartInstance) {
        filterData(chartInstance)
    }
}

module.exports = datafilterPlugin;