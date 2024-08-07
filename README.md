# Knobbox-TFT

Code for an ESP32 to drive throttles using tactile controls (knobs) in JMRI using the WiThrottle interface.

Accompanying video https://youtu.be/b8cglU2hCIE

Requires this library https://github.com/Bodmer/TFT_eSPI
Code for ring meter taken from here https://www.instructables.com/Arduino-analogue-ring-meter-on-colour-TFT-display/

Add an arduino_secrets.h file to the project, or replace the lines const char* ssid = SECRET_SSID; (and the password one) with const char* ssid = "my SSID";
