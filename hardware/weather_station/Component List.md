Following is a list of all equipment to make a weather station. Links are provided for ease of use, there might be better options available.

Station
-------
Name|Purpose|Link
----|-------|----
Arduino Compatible Board|Station Brain|https://www.amazon.com/ELEGOO-HT42B534-1-ATmega328P-Compatible-Arduino-Compatible/dp/B07TTN2HMQ/
Radio Shield|Allow communication from Arduino to AFSK|https://www.argentdata.com/catalog/product_info.php?products_id=136
Hand held radio|Transmit radioshield output over VHF|https://www.amazon.com/BaoFeng-UV-5R-Dual-Radio-Black/dp/B007H4VT7A/
Speaker-mic connector|Connect radioshield to hand held radio|To be investigated
Antenna Extension Cable|Allow positioning of antenna atop mast|https://www.amazon.com/15-Meter-Extension-Coaxial-Connector-Applications/dp/B01FVXW5X0/
Wind Vane|Transduce wind direction into electrical signal|https://www.argentdata.com/catalog/product_info.php?products_id=201 (Note: Not recommended - only reports 8 directions. Replacement being sought...)
Anemometer|Transduce wind speed into electical signal|https://www.argentdata.com/catalog/product_info.php?products_id=175
Anemometer + Wind Vane Mounting|Attach anemometer + wind vane to flag pole|To be investigated
4 Pole Cable|Connect sensors to ammo case|Note: This stuff isn't great. It breaks/shorts when pinched. But it's cheap! https://www.amazon.com/gp/product/B01C383026/
20W Solar Panel|Generate power|https://www.amazon.com/gp/product/B00W813E4I/
Solar Charge Controller|Regulate solar panel output to avoid overcharging battery|https://www.amazon.com/dp/B073WQC558/
12V 7Ah Lead Acid Battery|Store energy for darkness|https://www.amazon.com/gp/product/B003S1RQ2S/
Fuse Holder|Protect battery from faults|Local hardware store
2A fuse|Protect battery from faults|Local hardware store
B3603 CC-CV Buck Regulator|Convert 12V to 8V needed to power radio, limit current to avoid overcharing radio battery|https://www.banggood.com/B3603-Precision-CNC-DC-DC-Digital-Buck-Module-Constant-Voltage-Current-p-946751.html
Ammo Case|Protect electronics from the elements|https://www.amazon.com/gp/product/B00C2YELAC/
4 Pole waterproof connector|Break out sensor cable from ammo case|https://www.amazon.com/gp/product/B07B8DFKYX/
2 Pole waterproof connector|Break out solar panel cable from ammo case|https://www.amazon.com/gp/product/B01GRPECNM/
Cable Gland|Break out battery cable from ammo case|https://www.amazon.com/gp/product/B0748JLNR4/
SMA Adaptors|Allow connection of SMA cables|https://www.amazon.com/gp/product/B07FDHBS19/
Short antenna extension|Breakout antenna cable from ammo case|https://www.amazon.com/gp/product/B071FKJ8DS/
6mm O-ring|Waterproof SMA bulkhead connection|
RF Chokes|Reduce electrical noise on sensor cables, reduce spurious RF emissions|https://www.amazon.com/Cedmon-Pieces-Ferrite-Suppressor-Diameter/dp/B07CWCSNW9/
25ft Flag Pole|Locate wind sensors and antenna away from ground|https://www.amazon.com/gp/product/B07MM3Z4R9/
Misc electronics|Custom circuitry|https://www.amazon.com/gp/product/B01ERPEMAC/
Prototyping board|Custom circuitry|https://www.amazon.com/gp/product/B071R3BFNL/
3 x 40ft x 1/16" Wire Rope|Stays - prevent flag pole breaking in high winds|https://www.amazon.com/gp/product/B07V6C5815/
3 x Stakes|Attach wire rope stays to ground|Local hardware store
3 x Turbuckles|Adjust tension of stays|Local hardware store
Grommets|Protect aluminium flagpole from steel stays|Local hardware store

Receiver
--------
The receiver can be any computer really, I didn't have an old one laying about so I set up a Raspberry Pi.

The SDR dongle isn't the only way to go, the system can also be setup up to use a radio (e.g. another UV5R) and a computer soundcard. The Pi doesn't have sound input, and the SDR dongle is cheaper than a radio.

Name|Purpose|Link
----|-------|----
Raspberry Pi|Decode radio signals, post to website|https://www.amazon.com/gp/product/B07TD42S27/
USB C cable|Pi power cable|https://www.amazon.com/gp/product/B01GGKYN0A/
Micro HDMI Cable|Screen cable to set up Pi|https://www.amazon.com/gp/product/B00Z07JYLE/
USB Wallwart|Provide power for Pi|https://www.amazon.com/gp/product/B0774YZR5L/
SDR Dongle|Receive radio signals|https://www.amazon.com/gp/product/B0129EBDS2/
Antenna Extension Cable|Allow positioning of antenna atop mast|https://www.amazon.com/15-Meter-Extension-Coaxial-Connector-Applications/dp/B01FVXW5X0/
Antenna|Convert radio waves into electrical signals|Make your own. To be investiaged. I'm just using a walkie-talkie antenna for now.

Tools
-----
* A soldering iron - for making the custom electronics and cables
* A drill - for holes in the ammo case, for attaching wire rope to flag pole
* Wire rope cutter - to cut the stays to length
* Wire rope crimper - to connect the stays to the pole & stakes (Or use screw connections)
* Hammer - Pouding in the stakes for guy lines
* Shovel - Placing the flag pole