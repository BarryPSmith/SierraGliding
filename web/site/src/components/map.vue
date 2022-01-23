<template>
    <div id="map" class="h-full"></div>
</template>

<script>
import singleStationView from './singleStationView.vue';
import Vue from 'vue';

export default {
    name: 'sgMap',
    components: {
        singleStationView,
    },
    props: ['stations', 'stationDict'],
    data() {
        return {
            credentials: {
                map: 'pk.eyJ1IjoiaW5nYWxscyIsImEiOiJjam42YjhlMG4wNTdqM2ttbDg4dmh3YThmIn0.uNAoXsEXts4ljqf2rKWLQg',
            },
            mapLoaded: false,
        };
    },
    mounted() {
        this.map_init();
        this.onFeaturesChanged();
    },
    watch: {
        stationFeatures() {
            this.onFeaturesChanged();
            this.setupSources();
        },
        stationDict() {
            this.onFeaturesChanged();
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
            }
        },
        get_color(value, legend) {
            const ptColor = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ?
                'white' : 'black';

            if (typeof (value) != "number"
                || !legend) {
                return ptColor;
            }
            for (const entry of legend) {
                if (entry.top >= value) {
                    return entry.color;
                }
            }
            return legend[legend.length - 1].color;
        },
        map_init() {
            try {
                mapboxgl.accessToken = this.credentials.map;
                this.map = new mapboxgl.Map({
                    hash: false,
                    container: 'map',
                    attributionControl: false,
                    style: 'mapbox://styles/mapbox/satellite-streets-v11',
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
                this.setupLayers();
            });
            this.map.on('click', 'stationCircles', this.onMapClick);
        },
        setupLayers() {
            this.map.loadImage('/arrow.png', (err, img) =>
            {
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
            });
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
            if (this.stations)
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
                        duration: 300,
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
