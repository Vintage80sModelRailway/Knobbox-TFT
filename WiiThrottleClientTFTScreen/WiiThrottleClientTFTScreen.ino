#include <WiFiClientSecure.h>
#include <MQTT.h>
#include "RosterEntry.h"
#include "Controller.h"
#include "arduino_secrets.h"
#include <TFT_eSPI.h> // Hardware-specific library

#define NUMBER_OF_THROTTLES 6

#define TFT_GREY 0x5AEB
#define RED2RED 0
#define GREEN2GREEN 1
#define BLUE2BLUE 2
#define BLUE2RED 3
#define GREEN2RED 4
#define RED2GREEN 5

#define METERXOFFSET 26
#define METERYOFFSET 5

String prefix;
String locoName;
String delimeter = "";
String receivedLine = "";
int locoArrayCounter = 0;
int rosterCounter = 0;
bool rosterCollecting = false;
bool actionCollecting = false;
bool trackPowerCollecting = false;
bool trackPowerState = false;
String actionReceivedLine = "";
String actionControllerIndex = "";
int numberOfLocosInRoster = -1;
RosterEntry currentLoco;
RosterEntry roster[30];

const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASS;
const char * clientName = "KnobBoxTFT";
const uint16_t port = 1883;
const char * server = "192.168.1.29";
String cabSignalTopic = "layout/cabsignal/";
String locoMovementTopic = "layout/locomovement/";

//mtIndex, CLK, DT, SW, TFTX, TFTY
Controller throttles[NUMBER_OF_THROTTLES] = {
  Controller("1", 36, 39, 34, 0, 0),
  Controller("2", 35, 32, 33, 160, 0),
  Controller("3", 4, 26, 27, 0, 170),
  Controller("4", 14, 13, 15, 160, 170),
  Controller("5", 5, 17, 16, 0, 340),
  Controller("6", 21, 19, 18, 160, 340)
};

WiFiClient wifiClient;
WiFiClient wifiClientMQTT;
MQTTClient mqttClient;
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

void setup() {

  Serial.begin (19200);
  WiFi.begin(ssid, password);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  int tryDelay = 500;
  int numberOfTries = 20;
  bool isConnected = false;

  while (!isConnected) {

    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.println("[WiFi] SSID not found");
        break;
      case WL_CONNECT_FAILED:
        Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
        return;
        break;
      case WL_CONNECTION_LOST:
        Serial.println("[WiFi] Connection was lost");
        break;
      case WL_SCAN_COMPLETED:
        Serial.println("[WiFi] Scan is completed");
        break;
      case WL_DISCONNECTED:
        Serial.println("[WiFi] WiFi is disconnected");
        break;
      case WL_CONNECTED:
        Serial.println("[WiFi] WiFi connected!");
        Serial.print("[WiFi] IP address: ");
        //String ip = "";// WiFi.localIP();
        //scrollToScreen("IP address: "+ip);
        Serial.println(WiFi.localIP());

        isConnected = true;
        break;
      default:
        Serial.print("[WiFi] WiFi Status: ");
        Serial.println(WiFi.status());
        //scrollToScreen("WiFi Status: "+WiFi.status());
        break;
    }
    delay(tryDelay);

    if (numberOfTries <= 0) {
      Serial.print("[WiFi] Failed to connect to WiFi!");
      WiFi.disconnect();
      return;
    } else {
      numberOfTries--;
    }
  }

  mqttClient.begin(server, wifiClientMQTT);
  mqttClient.setKeepAlive(360);
  mqttClient.setTimeout(360);
  mqttClient.onMessage(messageReceived);

  connect();

  if (!wifiClient.connect(IPAddress(192, 168, 1, 29), 12090)) {
    Serial.println("Connection to host failed");
  }
  else {
    wifiClient.write("NKnobBoxTFT\n");
    Serial.println("Connected to WiiThrottle");
  }
  prefix = "";
  locoName = "";

  for (int i = 0; i < NUMBER_OF_THROTTLES; i++) {
    clearThrottleScreen(i);
  }
}

void drawThrottle(int i) {
  char text[1] = {
    'F'
  };
  if (roster[throttles[i].rosterIndex].currentDirection < 1) ringMeter(throttles[i].KnobPosition, 0, 128, throttles[i].tftX + METERXOFFSET, throttles[i].tftY + METERYOFFSET, 52, "R" , GREEN2RED); // Draw analogue meter
  else ringMeter(throttles[i].KnobPosition, 0, 128, throttles[i].tftX + METERXOFFSET, throttles[i].tftY + METERYOFFSET, 52, "F" , GREEN2RED); // Draw analogue meter

}

void clearThrottle(int i) {
  clearThrottleScreen(i);
  drawThrottle(i);
  drawTrainName(throttles[i].tftX, throttles[i].tftY, "Unassigned");
  drawSignal(i);
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
  String strTopic = (String)topic;
  int posLast = strTopic.lastIndexOf("/");
  int posFirst = strTopic.indexOf("/");
  String justTheID = strTopic.substring(posLast + 1);
  bool signalAspectHasChanged = false;
  for (int i = 0; i < NUMBER_OF_THROTTLES; i++) {
    if (roster[throttles[i].rosterIndex].Id == justTheID) {
      char aspect = payload.charAt(0);
      Serial.println("Found signal set throttle for " + roster[throttles[i].rosterIndex].Name + " - " + String(aspect));
      if (aspect != throttles[i].signalAspect) {
        Serial.println("Aspect changed from " + String(throttles[i].signalAspect) + " to " + String(aspect));
        throttles[i].signalAspect = aspect;
        signalAspectHasChanged = true;
        drawSignal(i);
      }
    }
  }
}

void loop() {

  if (!wifiClient.connected()) {
    if (!wifiClient.connect(IPAddress(192, 168, 1, 29), 12090)) {
      Serial.println("Connection to host failed");
    }
    else {
      wifiClient.write("NESP32 Knob Throttle Sig\n");
      Serial.println("Connected to WiiThrottle");
    }
  }

  while (wifiClient.available() && wifiClient.available() > 0) {
    readFromWiiTHrottle();
  }

  mqttClient.loop();
  if (!mqttClient.connected()) {
    Serial.println("MQTT client disc");
    connect();
  }

  for (int i = 0; i < NUMBER_OF_THROTTLES; i++) {
    int previousKnobPosition = throttles[i].KnobPosition;
    bool previousButtonState = throttles[i].ButtonState;
    bool previousButtonHeldState = throttles[i].buttonIsHeld;

    throttles[i].CheckMovement();

    if (previousKnobPosition != throttles[i].KnobPosition) {
      if (throttles[i].buttonIsHeld == 1 && previousButtonHeldState == 1) {
        //change selected loco
        if (throttles[i].KnobPosition > numberOfLocosInRoster)
          throttles[i].KnobPosition = numberOfLocosInRoster - 1;
        throttles[i].newSelectedRosterIndex = throttles[i].KnobPosition;
        throttles[i].selectionChangedSinceHoldInitiated = true;
        printLocoToScreen(i);
        //Serial.println("Loco at dial " + roster[throttles[i].KnobPosition].Name + " - KP " + String(throttles[i].KnobPosition));
      }
      else {
        //change speed
        roster[throttles[i].rosterIndex].currentSpeed = throttles[i].KnobPosition;
        if (throttles[i].rosterIndex >= 0 && throttles[i].KnobPosition >= 0) {
          String spd = "M" + throttles[i].mtIndex + "A" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>V" + String(throttles[i].KnobPosition) + "\n";
          Serial.print("New speed " + String(throttles[i].KnobPosition) + " writing " + spd);
          writeString(spd);
          drawThrottle(i);
          if (!throttles[i].hasMovedSinceAcquired) {
            throttles[i].hasMovedSinceAcquired = true;
            mqttClient.publish(locoMovementTopic + roster[throttles[i].rosterIndex].Id , "Start", false, 0);
            Serial.println("Loco start detected - MQTT - " + locoMovementTopic + roster[throttles[i].rosterIndex].Id);
          }
        }
      }
    }

    //Stop / direction change
    if (throttles[i].ButtonState != previousButtonState && throttles[i].ButtonState == 0 && previousButtonHeldState != 1) {// && functionSelector.selectionIsChanging <0 && functionSelector.selectionIsChanging <0) { //buttonIsHeld has not yet been processed so this check will think it's held / been held
      if (throttles[i].rosterIndex > -1)
      {
        if (throttles[i].KnobPosition > 0) {
          String spd = "M" + throttles[i].mtIndex + "A" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>V0\n";
          Serial.print("Train moving, emergency stop here - New speed 0 writing " + spd);
          throttles[i].KnobPosition = 0;
          roster[throttles[i].rosterIndex].currentSpeed = 0;
          writeString(spd);
          drawThrottle(i);
        }
        else
        {
          bool oldDir = roster[throttles[i].rosterIndex].currentDirection;
          roster[throttles[i].rosterIndex].currentDirection = !oldDir;
          String dir =  "M" + throttles[i].mtIndex + "A" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>R" + String(roster[throttles[i].rosterIndex].currentDirection) + "\n";
          Serial.print("OK to change direction from " + String(oldDir) + " to " + String(roster[throttles[i].rosterIndex].currentDirection) + " - " + dir);
          writeString(dir);
          drawThrottle(i);
        }
      }
    }

    //Change selection
    if (previousButtonHeldState != throttles[i].buttonIsHeld) {
      if (throttles[i].buttonIsHeld == 1) {
        Serial.println("Selector button held - throttle " + String(i) + " current train assignment " + throttles[i].TrainName + " - " + String(throttles[i].rosterIndex));
        //throttles[i].selectionIsChanging = true;
        clearThrottleScreen(i);
        String selectMessage = centreString("Select loco", 20);
        tft.drawString(selectMessage, throttles[i].tftX, throttles[i].tftY + 30, 4);
        if (throttles[i].rosterIndex >= 0)
          throttles[i].KnobPosition = throttles[i].rosterIndex;
        else
          throttles[i].KnobPosition = 0;
        throttles[i].selectionChangedSinceHoldInitiated = false;
      }
      else { //hold has been released
        Serial.println("Hold released on throttle " + String(i));
        if (throttles[i].selectionChangedSinceHoldInitiated == 1) {
          //deselect train from MT
          throttles[i].TrainName = "";
          throttles[i].rosterIndex = -1;
          if (throttles[i].KnobPosition > -1) {
            Serial.println("Selector button hold released - assign currently selected train - ");
            throttles[i].TrainName = roster[throttles[i].KnobPosition].Name;
            throttles[i].signalAspect = 'X';
            throttles[i].hasMovedSinceAcquired = false;

            if (throttles[i].rosterIndex >= 0) {
              String rel = "M" + throttles[i].mtIndex + "-" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>r\n";
              Serial.print("Released else Write " + rel);

              writeString(rel);

              mqttClient.unsubscribe(cabSignalTopic + roster[throttles[i].rosterIndex].Id);
              if (throttles[i].hasMovedSinceAcquired) {
                mqttClient.publish(locoMovementTopic + roster[throttles[i].rosterIndex].Id , "Stop", false, 0);
                Serial.println("MQTT release " + locoMovementTopic + roster[throttles[i].rosterIndex].Id + " - Stop");
              }
              Serial.println("Ubsub " + cabSignalTopic + roster[throttles[i].rosterIndex].Id);
            }

            //Handle newly assigned loco
            throttles[i].rosterIndex = throttles[i].newSelectedRosterIndex;
            String assign = "M" + throttles[i].mtIndex + "+" + roster[throttles[i].newSelectedRosterIndex].IdType +  roster[throttles[i].newSelectedRosterIndex].Id + "<;>" + roster[throttles[i].newSelectedRosterIndex].IdType +  roster[throttles[i].newSelectedRosterIndex].Id + "\n";
            Serial.print("Assigned Write " + assign);

            writeString(assign);

            mqttClient.subscribe(cabSignalTopic + roster[throttles[i].rosterIndex].Id);
            Serial.println("Sub " + cabSignalTopic + roster[throttles[i].rosterIndex].Id);
            throttles[i].signalAspect = 'A';
            throttles[i].KnobPosition = 0;
            clearThrottleScreen(i);
            drawThrottle(i);
            drawTrainName(throttles[i].tftX, throttles[i].tftY, throttles[i].TrainName);
            drawSignal(i);
          }
        }
        else {
          Serial.println("Selector button hold released - no change in selection detected, no new assignment - " );

          //deselect train from MT
          if (throttles[i].rosterIndex >= 0) {
            String rel = "M" + throttles[i].mtIndex + "-" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>r\n";
            Serial.print("Write " + rel);

            writeString(rel);

            Serial.println("Ubsub " + cabSignalTopic + roster[throttles[i].rosterIndex].Id);
            mqttClient.unsubscribe(cabSignalTopic + roster[throttles[i].rosterIndex].Id);

            if (throttles[i].hasMovedSinceAcquired) {
              mqttClient.publish(locoMovementTopic + roster[throttles[i].rosterIndex].Id , "Stop", false, 0);
              Serial.println("MQTT release " + locoMovementTopic + roster[throttles[i].rosterIndex].Id + " - Stop");
            }

            throttles[i].signalAspect = 'X';
          }

          throttles[i].TrainName = "";
          throttles[i].rosterIndex = -1;
          throttles[i].KnobPosition = -1;
          throttles[i].signalAspect = 'X';
          clearThrottleScreen(i);
        }

        throttles[i].selectionChangedSinceHoldInitiated = false;
        throttles[i].functionSelectionChangedSinceHoldInitiated = 0;
      }
    }
  }
}

void writeString(String stringData) { // Used to serially push out a String with Serial.write()

  for (int i = 0; i < stringData.length(); i++)
  {
    wifiClient.write(stringData[i]);   // Push each char 1 by 1 on each loop pass
  }

}// end writeString

void readFromWiiTHrottle() {
  char x = wifiClient.read();
  prefix += x;
  receivedLine += x;
  if (trackPowerCollecting == true) {
    if (x == '\n') {
      //Serial.println(receivedLine);
      String cTrackPowerState = receivedLine.substring(3, 4);
      //Serial.println("Track power message "+cTrackPowerState+" - ");
      if (cTrackPowerState == "1") {
        trackPowerState = true;
      }
      else {
        trackPowerState = false;
      }
      trackPowerCollecting = false;
    }
  }
  if (actionCollecting == true) {
    if (x == '\n') {
      //Serial.println("WT action: " + actionReceivedLine);

      int index = actionReceivedLine.indexOf("<;>");
      String firstHalf = actionReceivedLine.substring(0, index);
      // Serial.println("Firsthalf "+firstHalf);

      int addressIndex = firstHalf.indexOf("AS");
      if (addressIndex == -1) {
        addressIndex = firstHalf.indexOf("AL");
      }
      String throttleIndex = firstHalf.substring(1, addressIndex);
      int iThrottleIndex = throttleIndex.toInt();
      //Serial.println("Controller index "+controllerIndex);
      String address = firstHalf.substring(addressIndex + 1);
      //Serial.println("Address "+address);
      String secondHalf = actionReceivedLine.substring(index + 3);
      //Serial.println("Secondhalf "+secondHalf);

      String actionType = secondHalf.substring(0, 1);
      String actionInstruction = secondHalf.substring(1);

      int rosterIndexToUpdate = -1;
      for (int r = 0; r < numberOfLocosInRoster; r++) {
        String locoAddress = roster[r].IdType + roster[r].Id;
        if (locoAddress == address) {
          rosterIndexToUpdate = r;
          //Serial.println("Matched loco " + roster[r].Name);
          break;
        }
      }
      if (actionType == "V") {
        //velocity/speed
        if (throttles[iThrottleIndex - 1].rosterIndex > -1) {
          int originalKnobPosition = throttles[iThrottleIndex - 1].KnobPosition;
          int newSpeed = actionInstruction.toInt();
          int diff = newSpeed -  roster[rosterIndexToUpdate].currentSpeed;
          if (diff > 5 || diff < -5)
          {
            roster[rosterIndexToUpdate].currentSpeed = actionInstruction.toInt();
            //Serial.println("Set " + roster[rosterIndexToUpdate].Name + " to speed " + String(roster[rosterIndexToUpdate].currentSpeed));
            //Serial.println("Action type " + actionType + " instruction " + actionInstruction + " controller " + String(throttleIndex));
            if (throttles[iThrottleIndex - 1].rosterIndex == rosterIndexToUpdate) {
              throttles[iThrottleIndex - 1].KnobPosition = roster[rosterIndexToUpdate].currentSpeed;
              Serial.println("Set active controller roster knob speed");
            }
            int diff = originalKnobPosition -  roster[rosterIndexToUpdate].currentSpeed;

            //update speed on screen?
            for (int i = 0; i < NUMBER_OF_THROTTLES; i++) {
              if (throttles[i].rosterIndex == rosterIndexToUpdate) {
                drawThrottle(i);
                Serial.println("Speed update on throttle " + String(i));
              }
            }
          }
        }
      }
      else if (actionType == "R") {
        //direction
        roster[rosterIndexToUpdate].currentDirection = actionInstruction.toInt();
        Serial.println("Set " + roster[rosterIndexToUpdate].Name + " to direction " + String(roster[rosterIndexToUpdate].currentDirection));
        for (int i = 0; i < NUMBER_OF_THROTTLES; i++) {
          if (throttles[i].rosterIndex == rosterIndexToUpdate) {
            drawThrottle(i);
            Serial.println("Dir update on throttle " + String(i));
          }
        }

      }
      else if (actionType == "X") {
        //emergency stop
        roster[rosterIndexToUpdate].currentSpeed = 0;
        String spd = "M" + throttleIndex + "A*<;>V0\n";
        writeString(spd);
      }
      else if (actionType == "F") {
        //function
        String functionState = actionInstruction.substring(0, 1);
        String functionAddress = actionInstruction.substring(1);

        int iFunctionState = functionState.toInt();
        int iFunctionAddress = functionAddress.toInt();

        // Serial.println("Function - " + roster[rosterIndexToUpdate].Name + "storing state " + functionState + " for address " + functionAddress);
        roster[rosterIndexToUpdate].functionState[iFunctionAddress] = iFunctionState;
      }

      actionReceivedLine = "";
    }
    actionReceivedLine += x;
    if (actionReceivedLine.length() == 1)
    {
      actionControllerIndex = x;

    }
  }
  if (rosterCollecting == true) {
    if (x == '{' || x == '|' || x == '[' || x == '}' || x == ']' || x == '\\' || x == '\n') {
      delimeter += x;
      if (delimeter == "]\\[" || x == '\n') {
        //new loco
        Serial.println("Loco " + locoName);
        if (numberOfLocosInRoster < 0) {
          numberOfLocosInRoster = locoName.toInt();
          Serial.println ("Number of locos " + locoName);
          locoName = "";
        }
        else {
          //Serial.println ("Idtype " + locoName);
          currentLoco.IdType = locoName;
          locoName = "";
          locoArrayCounter = 0;
          roster[rosterCounter] = currentLoco;
          Serial.println(currentLoco.Name + " - " + String(rosterCounter));
          rosterCounter++;
        }

      }
      else if (delimeter == "}|{") {
        if (locoArrayCounter == 0) {
          //Serial.println ("Name " + locoName);
          currentLoco.Name = locoName;
          locoName = "";
        }
        else if (locoArrayCounter == 1) {
          //Serial.println ("Id " + locoName);
          currentLoco.Id = locoName;
          locoName = "";
        }
        else if (locoArrayCounter == 2 ) {

          locoName = "";
        }
        locoArrayCounter++;
        //Serial.println(delimeter);
      }
    }
    else {
      delimeter = "";
      locoName += x;
    }
  }
  if (x == '\n') {
    //Serial.print("Nreline");
    prefix = "";
    if (rosterCollecting) {
      //Serial.println("Roster complete");

    }

    rosterCollecting = false;
    actionReceivedLine = "";
    actionCollecting = false;
    trackPowerCollecting = false;
    receivedLine = "";
  }
  if (prefix == "RL") {
    //Serial.println("Roster line");
    rosterCollecting = true;
  }
  if (prefix == "M") {
    //Serial.println("Incoming action");
    actionCollecting = true;
    actionReceivedLine += x;
  }
  if (prefix == "PPA") {
    Serial.println("Track powerr prefix " + prefix);
    trackPowerCollecting = true;
  }
  //Serial.print(x);

}

void printLocoToScreen(int i) {
  clearThrottleScreen(i);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  String centeredId = centreString( roster[throttles[i].newSelectedRosterIndex].Id, 10);
  String centeredName = centreString(roster[throttles[i].newSelectedRosterIndex].Name.substring(0, 24), 26);
  tft.drawString(centeredId, throttles[i].tftX, throttles[i].tftY + 20, 6);
  tft.drawString(centeredName, throttles[i].tftX, throttles[i].tftY + 100, 2);
}

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("connecting...");

  while (!mqttClient.connect(clientName)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");
}


String centreString(String text, int maxLength) {
  int len = text.length();
  //Serial.println(text);
  //Serial.println("len " + String(len));
  int diff = maxLength - len;
  //Serial.println("diff " + String(diff));
  int halfDiff = diff / 2;
  String paddingString = "";
  for (int i = 0; i < halfDiff; i++) {
    paddingString = paddingString + " ";
  }
  return paddingString + text + paddingString;
}

void drawSignal(int i) {
  int mod = i % 2;
  int sigX = 17;
  //signal icons are placed at the edge of the screen so coordinates are different for left vs right screen meters
  //work out if this throttle is on the left or right of the screen then adjust X coordinates appropriately
  if (mod != 0) sigX = 301;
  uint16_t colour = 0;
  switch (throttles[i].signalAspect) {
    case 'P':
      colour = TFT_GREEN;
      break;
    case 'C':
      colour = TFT_ORANGE;
      break;
    case 'D':
      colour = TFT_RED;
      break;
  }
  if (colour > 0)
    tft.fillCircle(sigX, throttles[i].tftY + 17, 15, colour);
  else
    tft.drawCircle(sigX, throttles[i].tftY + 17, 14, TFT_GREEN);
}

void drawTrainName(int x, int y, String trainName) {
  String centeredString = centreString(trainName.substring(0, 24), 26);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(centeredString, x, y + 110, 2);
}

//ringMeter and rainbow voids by Bodmer
//https://www.instructables.com/Arduino-analogue-ring-meter-on-colour-TFT-display/
int ringMeter(int value, int vmin, int vmax, int x, int y, int r, char *units, byte scheme)
{
  // Minimum value of r is about 52 before value text intrudes on ring
  // drawing the text first is an option

  x += r; y += r;   // Calculate coords of centre of ring

  int w = r / 4;    // Width of outer ring is 1/4 of radius

  int angle = 150;  // Half the sweep angle of meter (300 degrees)

  int text_colour = 0; // To hold the text colour

  int v = map(value, vmin, vmax, -angle, angle); // Map the value to an angle v

  byte seg = 5; // Segments are 5 degrees wide = 60 segments for 300 degrees
  byte inc = 10; // Draw segments every 5 degrees, increase to 10 for segmented ring

  // Draw colour blocks every inc degrees
  for (int i = -angle; i < angle; i += inc) {

    // Choose colour from scheme
    int colour = 0;
    switch (scheme) {
      case 0: colour = TFT_RED; break; // Fixed colour
      case 1: colour = TFT_GREEN; break; // Fixed colour
      case 2: colour = TFT_BLUE; break; // Fixed colour
      case 3: colour = rainbow(map(i, -angle, angle, 0, 127)); break; // Full spectrum blue to red
      case 4: colour = rainbow(map(i, -angle, angle, 63, 127)); break; // Green to red (high temperature etc)
      case 5: colour = rainbow(map(i, -angle, angle, 127, 63)); break; // Red to green (low battery etc)
      default: colour = TFT_BLUE; break; // Fixed colour
    }

    // Calculate pair of coordinates for segment start
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (r - w) + x;
    uint16_t y0 = sy * (r - w) + y;
    uint16_t x1 = sx * r + x;
    uint16_t y1 = sy * r + y;

    // Calculate pair of coordinates for segment end
    float sx2 = cos((i + seg - 90) * 0.0174532925);
    float sy2 = sin((i + seg - 90) * 0.0174532925);
    int x2 = sx2 * (r - w) + x;
    int y2 = sy2 * (r - w) + y;
    int x3 = sx2 * r + x;
    int y3 = sy2 * r + y;

    if (i < v) { // Fill in coloured segments with 2 triangles
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, colour);
      text_colour = colour; // Save the last colour drawn
    }
    else // Fill in blank segments
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREY);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREY);
    }
  }

  // Convert value to a string
  if (value >= 0) {
    char buf[10];
    byte len = 4; if (value > 999) len = 5;
    dtostrf(value, len, 0, buf);

    // Set the text colour to default
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // Uncomment next line to set the text colour to the last segment value!
    // tft.setTextColor(text_colour, ILI9341_BLACK);

    // Print value, if the meter is large then use big font 6, othewise use 4
    int padding = tft.textWidth("128", 4);
    tft.setTextPadding(50);
    if (r > 84) tft.drawCentreString(buf, x - 5, y - 20, 6); // Value in middle
    else tft.drawCentreString(buf, x - 5, y - 20, 4); // Value in middle

    // Print units, if the meter is large then use big font 4, othewise use 2
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    Serial.println(units);
    if (r > 84) {
      int padding = tft.textWidth("Word", 4);
      tft.setTextPadding(50);
      tft.drawCentreString(units, x, y + 30, 4); // Units display
    }
    else {
      int padding = tft.textWidth("Word", 2);
      tft.setTextPadding(50);
      tft.drawCentreString(units, x, y + 5, 2); // Units display
    }
  }
  // Calculate and return right hand side x coordinate
  return x + r;
}

// #########################################################################
// Return a 16 bit rainbow colour
// #########################################################################
unsigned int rainbow(byte value)
{
  // Value is expected to be in range 0-127
  // The value is converted to a spectrum colour from 0 = blue through to 127 = red

  byte red = 0; // Red is the top 5 bits of a 16 bit colour value
  byte green = 0;// Green is the middle 6 bits
  byte blue = 0; // Blue is the bottom 5 bits

  byte quadrant = value / 32;

  if (quadrant == 0) {
    blue = 31;
    green = 2 * (value % 32);
    red = 0;
  }
  if (quadrant == 1) {
    blue = 31 - (value % 32);
    green = 63;
    red = 0;
  }
  if (quadrant == 2) {
    blue = 0;
    green = 63;
    red = value % 32;
  }
  if (quadrant == 3) {
    blue = 0;
    green = 63 - 2 * (value % 32);
    red = 31;
  }
  return (red << 11) + (green << 5) + blue;
}

void clearThrottleScreen(int i) {
  Serial.println("X " + String(throttles[i].tftX) + " Y " + String(throttles[i].tftY));
  tft.fillRect( throttles[i].tftX, throttles[i].tftY, 160, 160, TFT_BLACK);
}
