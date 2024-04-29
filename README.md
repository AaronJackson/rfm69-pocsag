# RFM69 Pager Transmitter

This is a relatively complete project for building a DIY pager transmitter using 
an Adafruit Feather LoRa board and ESP8266. Two microcontrollers are used to enable
full remote shutdown of the transmitter. This repo contains the following,

* Code for an ESP8266, which acts as a bridge between MQTT and the Adafruit LoRa
  board. This simply writes out the MQTT messages to UART.
* Code for an Adafruit LoRa (433MHz variant) board, which configures the RFM69 to
  transmit FSK and perform the POCSAG encoding. This is mostly based around a
  modified [pocsag-tool](https://github.com/hazardousfirmware/pocsag-tool/blob/main/pocsag.c)
  by hazardousfirmware (Thank you!)
* A KiCAD PCB for connecting up the ESP8266 and AdaFruit Feather boards. It includes
  a separate voltage regulator, which taps the 5v from the Feather. The ESP8266 pulls
  the Feather's enable pin to ground, which shuts off its 3.3v regulator.
* A quick laser cut box designed with MakerCase and Inkscape. The final version ended
  up being slightly different I think after some tweaking next to the laser cutter.

I have two deployments of this now:

- At home, using my callsign M6PIU.
- At Nottingham Hackspace, using the FAC callsign MB7PNH (Pager NottingHack).

<img src="https://wiki.nottinghack.org.uk/images/d/dc/MB7PNH.JPG" alt="Assembled circuit board" width="400" />
<img src="https://aaronsplace.co.uk/blog/img/pager-tx/img_1592.jpg" alt="Assembled circuit board" width="400" />
