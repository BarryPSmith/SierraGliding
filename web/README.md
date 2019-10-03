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

## API
