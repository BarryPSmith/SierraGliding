Hardware for the deployed weather stations is in this folder.

Changes to investigate:
====================
* We need a better wind vane. The unit sold by Argent Data only reports 8 possible wind directions. Ultrasonic might be a good alternative.
* Can we use alternatives to the RadioShield + UV5R? LoRa looks like a possible improvement.
* Can we alter the B3603 to act as a solar controller? There might be ideas at https://github.com/baruch/b3603
* Can we replace the arduino with a lower power option? Perhaps the MSP430?