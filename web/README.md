<h1 align=center>Sierra Gliding Web Server</h1>

## Installation

To run the server you will need to download and install [NodeJS](https://nodejs.org/en/)
Binaries are provided for windows, linux, and OSX.

Once you have installed node, open up your favourite terminal app
and in the `SierraGliding/web/` folder, run the following commands:

```
npm install             # Install Server Dependencies
cd site                 # Enter Frontend Folder
npm install             # Install Frontend Dependencies
npm install -g parcel   # Install the frontend build scripts
```

This will install all the dependencies for both the server and the frontend.

Before starting the server, start `parcel`, the frontend build system, to
watch for changes and rebuild for the frontend portions of the site.

From the `web/site/` directory, run


```
npm run dev
```

Then to get documentation on starting the server, from the `web/` directory,
run:

```
node index.js
```

## Resources

### Frontend

The frontend uses a CSS library called [Assembly](https://labs.mapbox.com/assembly/)

## API

### `GET`: `api/stations`

Get a list of all stations, along with applicable wind speed/direction flying ranges

The list is returned as a GeoJSON FeatureCollection

*Example Response*

```JSON
{
    "type": "FeatureCollection",
    "bbox": [ -118.49418410527042, 37.299776417065324, -118.40435257685847, 37.371201273908426 ],
    "features": [{
        "id": 1,
        "type": 'Feature",
        "bbox": [ -118.45375991748506, 37.331926007166906, -118.44477676464385, 37.339068493042674 ],
        "properties": {
            "name": "Windy Ridge",
            "legend": {
                "wind_speed": [ 10, 20, 25 ],
                "wind_dir": []
            }
        },
        "geometry": {
            "type": "Point",
            "coordinates": [ -118.44926834106445, 37.335497334999936 ]
        }
    }]
}
```

### `POST`: `api/station`

Create a new weather station.

Note: Content-Type header must be `application/json`.

*Example Body*

```JSON
{
    "id": 1,
    "name": "Windy Ridge",
    "lon": -118.44926834106445,
    "lat": 37.335497334999936,
    "windspeedlegend": [10,20,25],
    "winddirlegend": []
}
```

### `GET`: `api/station/:id`

Get all information about about a single
given station

*Example Response*

```JSON
{
    "id": 1,
    "name": "Windy Ridge",
    "lon": -118.44926834106445,
    "lat": 37.335497334999936,
    "windspeedlegend": [10,20,25],
    "winddirlegend": []
}
```

### `GET`: `api/station/:id/data`

### `POST`: `api/station/:id/data`

### `GET`: `api/station/:id/data/latest`
