# Knobbox-TFT

Code for an ESP32 to drive throttles using tactile controls (knobs) in JMRI using the WiThrottle interface.

Accompanying video https://youtu.be/b8cglU2hCIE

Requires this library https://github.com/Bodmer/TFT_eSPI

Code for ring meter taken from here https://www.instructables.com/Arduino-analogue-ring-meter-on-colour-TFT-display/

Add an arduino_secrets.h file to the project, or replace the lines const char* ssid = SECRET_SSID; (and the password one) with const char* ssid = "my SSID";

This is a hurried copy / paste / edit / rewrite of the code I wrote for the original Knob box, here.
https://github.com/Vintage80sModelRailway/WiiThrottleKnobController

ESP, TFT and knob connections

Assumes the following in the TFT_eSPI/User_setup.h, in the Ardiono libraries folder

<code>#define TFT_MISO -1
  #define TFT_MOSI 0
  #define TFT_SCLK 12
  #define TFT_CS   23  // Chip select control pin
  #define TFT_DC    22  // Data Command control pin
  #define TFT_RST   -1  // Reset pin (could connect to RST pin)</code>



<img src = "https://github.com/Vintage80sModelRailway/Knobbox-TFT/blob/main/esp32-kb3.jpg" />

https://github.com/Vintage80sModelRailway/Knobbox-TFT/blob/main/esp32-kb3.jpg
