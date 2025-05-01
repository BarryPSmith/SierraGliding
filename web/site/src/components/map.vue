<template>
    <div class="h-full">
        <div id="map" class="h-full"></div>
        <!--div class="align-center outlineTextBlack txt-h4 txt-bold absolute top left right color-white">
            {{title}}
        </div-->
        <div class="absolute bottom left mb30 ml6">
            <button class="block outlineTextBlack txt-bold color-white" @click="optionsVisible = !optionsVisible">
                {{optionsVisible ? 'Hide' : 'Show'}} Options
            </button>
            <div v-if="optionsVisible" class="flex flex--column">
                <button class="outlineTextBlack txt-bold color-white" @click="legendVisible = !legendVisible">
                    <p>{{legendVisible ? 'Hide' : 'Show'}} Legend</p>
                </button>
                <div class="backColor foreColor px3 py3 border border--gray-light"
                    v-if="legendVisible">
                <ul @click="legendVisible = false" 
                    class="txt-ul">
                    <li>Arrows show the direction the wind is blowing from.</li>
                    <li>Inntermost circle is instantaneous wind speed</li>
                    <li>Next circle is average windspeed over the last 5 minutes.</li>
                    <li>Outermost circle is strongest gust in the last 5 minutes.</li>
                    <li>Click on a station for recent history graphs</li>
                </ul>
                </div>
                <label class='checkbox-container outlineTextBlack txt-bold color-white'>
                    <input type='checkbox' v-model="showRoads"/>
                    <div class='checkbox mr6'>
                        <svg class='icon'><use xlink:href='#icon-check' /></svg>
                    </div>
                    Places and Roads
                </label>
                <label class='checkbox-container outlineTextBlack txt-bold color-white'>
                    <input type='checkbox' v-model="stationNames"/>
                    <div class='checkbox mr6'>
                        <svg class='icon'><use xlink:href='#icon-check' /></svg>
                    </div>
                    Launch Names
                </label>
                <label class='checkbox-container outlineTextBlack txt-bold color-white'>
                    <input type='checkbox' v-model="windDirections"/>
                    <div class='checkbox mr6'>
                        <svg class='icon'><use xlink:href='#icon-check' /></svg>
                    </div>
                    Wind Direction Names
                </label>
            </div>
        </div>
    </div>
</template>

<script>
import singleStationView from './singleStationView.vue';
import Vue from 'vue';

const directionNames = {
    0: 'N',
    22.5: 'NNE',
    45: 'NE',
    67.5: 'ENE',
    90: 'E',
    112.5: 'ESE',
    135: 'SE',
    157.5: 'SSE',
    180: 'S',
    202.5: 'SSW',
    225: 'SW',
    247.5: 'WSW',
    270: 'W',
    292.5: 'WNW',
    315: 'NW',
    337.5: 'NNW',
    360: 'N'
};

export default {
    name: 'sgMap',
    components: {
        singleStationView,
    },
    props: ['stations', 'stationDict', 'mapGeometry'],
    data() {
        return {
            credentials: {
                map: 'pk.eyJ1IjoiaW5nYWxscyIsImEiOiJjbG12OWE1eTIwZmoxMm1wZWdnNnJiNXYxIn0.VnEsi3jdemHZbOcQQAL1hQ',
            },
            mapLoaded: false,
            title: 'Sierra Gliding',
            showRoads: null,
            map: null,
            loaded: false,
            legendVisible: false,
            optionsVisible: false,
            stationNames: null,
            windDirections: null,
        };
    },
    mounted() {
        this.showRoads = window.localStorage.getItem('showRoads') != 'false';
        this.stationNames = window.localStorage.getItem('stationNames') != 'false';
        this.windDirections = window.localStorage.getItem('windDirections') == 'true';
        this.map_init();
        this.onFeaturesChanged();
        this.loaded = true;
    },
    watch: {
        stationFeatures() {
            this.onFeaturesChanged();
            this.setupSources();
        },
        mapGeometry() {
            this.setupGeometrySources();
        },
        stationDict() {
            this.onFeaturesChanged();
        },
        showRoads(newVal, oldVal) {
            if (oldVal == null) return;
            if (this.map) {
                if (this.showRoads) {
                    this.map.setStyle('mapbox://styles/mapbox/satellite-streets-v11');
                } else {
                    this.map.setStyle('mapbox://styles/mapbox/satellite-v9');
                }
            }
            window.localStorage.setItem('showRoads', this.showRoads.toString());
        },
        stationNames(newVal, oldVal) {
            if (oldVal == null) return;
            if (this.mapLoaded) {
                this.map.setLayoutProperty('stationNames', 'visibility', this.stationNames ? 'visible' : 'none');
            }
            window.localStorage.setItem('stationNames', this.stationNames.toString());
        },
        windDirections(newVal, oldVal) {
            if (oldVal == null) return;
            if (this.mapLoaded) {
                this.map.setLayoutProperty('windDirNames', 'visibility', this.windDirections ? 'visible' : 'none');
            }
            window.localStorage.setItem('windDirections', this.windDirections.toString());
        }
    },
    computed: {
        stationFeatures() {
            if (!this.stations) {
                return null;
            }
            return {
                type: 'FeatureCollection',
                features: this.stations.map(s => this.toFeature(s)),
            };
        }
    },
    methods: {
        toFeature(station)
        {
            return {
                id: station.id,
                type: 'Feature',
                geometry: {
                    type: 'Point',
                    coordinates: [station.lon, station.lat]
                },
                properties: {
                    name: station.name,
                },
            }
        },
        onFeaturesChanged() {
            if (!this.stationFeatures || !this.stationDict) return;
            for (const feature of this.stationFeatures.features) {
                const dataManager = this.stationDict[feature.id] && this.stationDict[feature.id].dataManager;
                dataManager.on('data_updated', this.data_updated)
                this.updateFeature(feature);
                }
        },
        data_updated(stationId) {
            const feature = this.stationFeatures.features.find(f => f.id == stationId);
            if (!feature) return;
            this.updateFeature(feature);
            const existing = this.map.getSource('stations');
            if (existing) {
                existing.setData(this.stationFeatures);
            }
        },
        updateFeature(feature) {
            const station = this.stationDict[feature.id];
            const dataManager = station && station.dataManager;
            const mostRecent = dataManager.stationData[dataManager.stationData.length - 1];
            const legend = station.Wind_Speed_Legend;
            
            if (mostRecent) {
                feature.properties.rotation = mostRecent.wind_direction;
                feature.properties.avgRotation = mostRecent.wind_direction_avg;
                feature.properties.gustColor = this.get_color(mostRecent.windspeed_max, legend);
                feature.properties.avgColor = this.get_color(mostRecent.windspeed_avg, legend);
                feature.properties.instColor = this.get_color(mostRecent.windspeed, legend);
                feature.properties.windDirName = this.getName(mostRecent.wind_direction_avg);
                feature.properties.windDirColor = this.get_direction_color(mostRecent.wind_direction_avg, station.Wind_Dir_Legend);
            }
        },
        getName(direction) {
            let closest = 360;
            let ret = 'N';
            for (const testDir in directionNames) {
                const test = Math.abs(direction - testDir);
                if (test < closest) {
                    closest = test;
                    ret = directionNames[testDir];
                }
            }
            return ret;
        },
        get_color(value, legend) {
            if (typeof (value) != "number"
                || !legend) {
                return 'black';
            }
            for (const entry of legend) {
                if (entry.top >= value) {
                    return entry.color;
                }
            }
            return legend[legend.length - 1].color;
        },
        get_direction_color(value, legend)
        {
            if (typeof (value) != "number"
                || !legend) {
                return 'white';
            }

            for (const entry of legend) {
                if ((entry.start < entry.end && entry.start <= value && value <= entry.end) ||
                    (entry.start > entry.end && (entry.start <= value || value <= entry.end))) {
                    return entry.color;
                }
            }
            return 'red';
        },
        map_init() {
            try {
                mapboxgl.accessToken = this.credentials.map;
                this.map = new mapboxgl.Map({
                    hash: false,
                    container: 'map',
                    attributionControl: false,
                    style: this.showRoads ? 'mapbox://styles/mapbox/satellite-streets-v11'
                        : 'mapbox://styles/mapbox/satellite-v9',
                    center: [-118.41064453125, 37.3461426132468],
                    zoom: 11
                }).addControl(new mapboxgl.AttributionControl({
                    compact: true
                }));
            } catch (err) {
            }
            if (!this.map) return;
            this.map.on('style.load', () => {
                this.mapLoaded = true;
                this.setupSources();
                this.setupGeometrySources();
                this.setupLayers();
            });
            this.map.on('click', 'stationCircles', this.onMapClick);
        },
        setupLayers() {
            this.map.loadImage('/arrow.png', (err, img) =>
            {
                this.map.addLayer({
                    id: 'geometryLines',
                    type: 'line',
                    source: 'geometry',
                    paint: {
                        "line-width": 2,
                        "line-color": ['get', 'stroke']
                    }
                });
                this.map.addLayer({
                    id: 'stationCircles',
                    type: 'circle',
                    source: 'stations',
                    paint: {
                        'circle-color': 'white',
                        'circle-radius': 22
                    }
                });
                this.map.addLayer({
                    id: 'stationGusts',
                    type: 'circle',
                    source: 'stations',
                    paint: {
                        'circle-color': ['get', 'gustColor'],
                        'circle-radius': 20
                    }
                });
                this.map.addLayer({
                    id: 'stationAvgs',
                    type: 'circle',
                    source: 'stations',
                    paint: {
                        'circle-color': ['get', 'avgColor'],
                        'circle-radius': 15
                    }
                });
                this.map.addLayer({
                    id: 'stationInsts',
                    type: 'circle',
                    source: 'stations',
                    paint: {
                        'circle-color': ['get', 'instColor'],
                        'circle-radius': 5
                    }
                });
                this.map.addImage('arrow', img);
                this.map.addLayer({
                    id: 'stationDirs',
                    type: 'symbol',
                    source: 'stations',
                    layout: {
                        'icon-image': 'arrow',
                        'icon-rotate': ['get', 'rotation'],
                        'icon-ignore-placement': true,
                    },
                    paint: {
                        'icon-opacity': 1.0,
                    },
                });
                this.map.addLayer({
                    id: 'stationAvgDirs',
                    type: 'symbol',
                    source: 'stations',
                    layout: {
                        'icon-image': 'arrow',
                        'icon-rotate': ['get', 'avgRotation'],
                        'icon-ignore-placement': true,
                    },
                    paint: {
                        'icon-opacity': 0.4,
                    },
                });
                this.map.addLayer({
                    id: 'windDirNames',
                    type: 'symbol',
                    source: 'stations',
                    layout: {
                        'text-field': ['get', 'windDirName'],
                        'text-anchor': 'bottom',
                        'text-size': 12,
                        'text-font': ["Open Sans Bold","Arial Unicode MS Regular"],
                        'text-ignore-placement': true,
                        'visibility': this.windDirections ? 'visible' : 'none'
                    },
                    paint: {
                        'text-color': ['get', 'windDirColor'],
                        'text-translate': [0, -20],
                        'text-halo-blur': 1,
                        'text-halo-width': 1,
                        'text-halo-color': 'black'
                    },
                });
                this.map.addLayer({
                    id: 'stationNames',
                    type: 'symbol',
                    source: 'stations',
                    layout: {
                        'text-field': ['get', 'name'],
                        'text-anchor': 'top',
                        'text-size': 12,
                        'text-font': ["Open Sans Bold","Arial Unicode MS Regular"],
                        'text-ignore-placement': true,
                        'visibility': this.stationNames ? 'visible' : 'none'
                    },
                    paint: {
                        'text-translate': [0, 20],
                        'text-halo-blur': 1,
                        'text-halo-width': 1,
                        'text-halo-color': 'white'
                    },
                });
            });
        },
        async setupGeometrySources() {
            if (!this.map || !this.mapLoaded) return;

            const existing = this.map.getSource('geometry');
            if (existing) {
                existing.setData(this.mapGeometry);
            } else {
                this.map.addSource('geometry', {
                    type: 'geojson',
                    data: this.mapGeometry
                });
            }
        },
        setupSources() {
            if (!this.map || !this.mapLoaded) return;

            const existing = this.map.getSource('stations');
            if (existing) {
                existing.setData(this.stationFeatures);
            } else {
                this.map.addSource('stations', {
                    type: 'geojson',
                    data: this.stationFeatures
                }); 
            }
            if (this.stations && !this.stationsFitted)
            {
                const lats = this.stations.map(s => s.lat);
                const lons = this.stations.map(s => s.lon);
                this.map.fitBounds([
                    [
                        Math.min(...lons), Math.min(...lats),
                    ], [
                        Math.max(...lons), Math.max(...lats),
                    ]
                ], {
                    padding: 40,
                    animate: false,
                });
                this.stationsFitted = true;
            }
        },
        onMapClick(e) {
            const feature = e.features[0];
            if (!feature) return;
            const station = this.stationDict[feature.id];
            if (!station) return;
            new mapboxgl.Popup({
                maxWidth: '600px',
                //closeOnMove: true,
                className: 'w300',
            })
                .setLngLat({
                    lon: station.lon,
                    lat: station.lat
                })
                .setHTML('<div style="max-height:300px;overflow-y:auto"><div id="popup-content"></div></div')
                .addTo(this.map);
            this.$nextTick(() => {
                const ssv = Vue.extend(singleStationView);
                const vueView = new ssv({
                    propsData: {
                        station: station,
                        dataType: '',
                        duration: 900,
                        chartEnd: null,
                        collapsedProp: false,
                        compact: true,
                    }
                });
                vueView.$mount('#popup-content');
            });
            this.setHash(station.id);
        },
        setHash(id) {
            const hash = window.location.hash.slice(1)
            const searchParams = new URLSearchParams(hash); 
            /*if (this.chartEnd && this.jumping) {
                searchParams.set('chartEnd', this.chartEnd);
                searchParams.set('lastHash', +new Date());
            } else {
                searchParams.delete('chartEnd');
                searchParams.delete('lastHash');
            }
            searchParams.set('duration', this.duration);*/
            //window.location.hash=searchParams;
            searchParams.delete('station');
            if (id != null) {
                searchParams.set('station', id);
            }
            history.replaceState(null, null, `#${searchParams}`);
        },
    },
};
</script>
