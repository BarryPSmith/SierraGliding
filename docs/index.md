Sierragliding weather stations

See the data at https://sierragliding.us

This project contains the information necessary to build a simple low cost low interval weather station.
The weather stations report only wind speed, wind direction, and temperature.
The stations report over radio, using the LoRa protocol. On a low traffic channel with line of sight, these should be able to communicate over 100km.
The project includes a website to display historical data from the stations, and a simple app to post the data to Windy.

To build and operate these stations, one should be a licensed amatuer radio (ham) operator. It might be legal to run the stations in the unlicensed ISM bands, I don't know nor do I want to go looking through the FCC regulations. Even if it is legal, operating in such bands will likely result in drastically worse performance: Lower range and more dropped packets.

All up component cost is around $75 per station, depending on how many stations one orders at the same time. This does not include the mounting pole.
My website to display the stations is hosted on Digital Ocean for $5/month, this is the only ongoing cost.

The weather stations consist of a custom circuit board installed in an off-the-shelf wind vane / anemometer combo.

Following is a description of how I have built my stations.

Hardware
Circuit boards:
The 

Wind Vane and Anemometer
The wind vane and anemometer used in these stations are purchased from ArgentData, item: https://www.argentdata.com/catalog/product_info.php?products_id=145 .
One can contact the owner of that business to request to buy only the windvane and anemometer, not the rain guage, for $12 cheaper per unit.

The hardware is modified:

The wind vane as supplied only provides 8 possible wind directions (N, NE, E, SE, S, SW, W, and NW); we want to be able to report wind from any direction. To do this pull off the wind vane and glue some magnets to have a field paralell with the direction of the wind.
I used 4 magnets, each 2mm x 10mm, glued as shown in the picture: [TODO]

Remove the wind vane circuit board and install the custom circuit board. You will need to cut the RJ11 cable soldered to the board in order to remove it. You can discard the board supplied with the vane.

The bottom of the wind vane needs to be modified for our connectors:
 - Enlarge the existing cable hole to be large enough to fit a 2mm JST connector through. A 5/8" drill will do well.
 - Create a new hole for the thermistor to hang out of the case. A 1/8" drill will work.
 - Create a new hole for the SMA connector. A 1/4" drill works.
