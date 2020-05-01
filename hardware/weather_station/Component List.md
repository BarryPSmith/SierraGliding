This list details components necessary in addition to the

Following is a list of all equipment to make a weather station. Links are provided for ease of use, there might be better options available.

Station
-------
Name|Purpose|Link
----|-------|----
Antenna Extension Cable|Allow positioning of antenna atop mast|https://www.amazon.com/15-Meter-Extension-Coaxial-Connector-Applications/dp/B01FVXW5X0/
Wind Vane|Transduce wind direction into electrical signal|https://www.argentdata.com/catalog/product_info.php?products_id=201 (Note: Not recommended - only reports 8 directions. Replacement being sought...)
Anemometer|Transduce wind speed into electical signal|https://www.argentdata.com/catalog/product_info.php?products_id=175
Anemometer + Wind Vane Mounting|Attach anemometer + wind vane to flag pole|To be investigated
4 Pole Cable|Connect sensors to ammo case|Note: This stuff isn't great. It breaks/shorts when pinched. But it's cheap! https://www.amazon.com/gp/product/B01C383026/
Fuse Holder|Protect battery from faults|Local hardware store
2A fuse|Protect battery from faults|Local hardware store
Cable Gland|Break out battery cable from ammo case|https://www.amazon.com/gp/product/B0748JLNR4/
SMA Adaptors|Allow connection of SMA cables|https://www.amazon.com/gp/product/B07FDHBS19/
Short antenna extension|Breakout antenna cable from ammo case|https://www.amazon.com/gp/product/B071FKJ8DS/
RF Chokes|Reduce electrical noise on sensor cables, reduce spurious RF emissions|https://www.amazon.com/Cedmon-Pieces-Ferrite-Suppressor-Diameter/dp/B07CWCSNW9/
25ft Flag Pole|Locate wind sensors and antenna away from ground|https://www.amazon.com/gp/product/B07MM3Z4R9/
3 x 40ft x 1/16" Wire Rope|Stays - prevent flag pole breaking in high winds|https://www.amazon.com/gp/product/B07V6C5815/
3 x Stakes|Attach wire rope stays to ground|Local hardware store
6 x Turnbuckles|Adjust tension of stays|Local hardware store
Grommets|Protect aluminium flagpole from steel stays|Local hardware store

Receiver
--------
The receiver can be any computer really, I didn't have an old one laying about so I set up a Raspberry Pi.

Name|Purpose|Link
----|-------|----
Raspberry Pi|Decode radio signals, post to website|https://www.amazon.com/gp/product/B07TD42S27/
USB C cable|Pi power cable|https://www.amazon.com/gp/product/B01GGKYN0A/
Micro HDMI Cable|Screen cable to set up Pi|https://www.amazon.com/gp/product/B00Z07JYLE/
USB Wallwart|Provide power for Pi|https://www.amazon.com/gp/product/B0774YZR5L/
Micro SD Card|Store Pi state|
Micro SD Card adaptor|Write OS image to Pi from your computer|
Antenna Extension Cable|Allow positioning of antenna atop mast|https://www.amazon.com/15-Meter-Extension-Coaxial-Connector-Applications/dp/B01FVXW5X0/
Antenna|Convert radio waves into electrical signals|Make your own. To be investiaged. I'm just using a walkie-talkie antenna for now.
Moteino|Act as translator between SX1268 and Pi|We could use the SPI on the PI directly... too much hassle.
NiceRF SX1268|LoRa radio|

Tools
-----
* A soldering iron - for making the custom electronics and cables
* A drill - for holes in the ammo case, for attaching wire rope to flag pole
* Wire rope cutter - to cut the stays to length
* Wire rope crimper - to connect the stays to the pole & stakes (Or use screw connections)
* Hammer - Pouding in the stakes for guy lines
* Shovel - Placing the flag pole
