#include "Controller.h"
#include <Arduino.h>

#define offsetVal 0;

Controller::Controller(String MTIndex, int EncoderPinA, int EncoderPinB, int EncoderBtn, int tftLocationX, int tftLocationY)
  : encoderPinA(EncoderPinA)
  , encoderPinB(EncoderPinB)
  , encoderBtn(EncoderBtn)
  , mtIndex(MTIndex)
  , tftX(tftLocationX)
  , tftY(tftLocationY)
{
  rosterIndex = 0;

  pinMode (encoderPinA, INPUT);
  pinMode (encoderPinB, INPUT);
  pinMode(encoderBtn, INPUT_PULLUP);
  PinA_prev = digitalRead(encoderPinA);
  ButtonState = !digitalRead(encoderBtn);

  KnobPosition = -1;
  rosterIndex = -1;
  buttonIsHeld = 0;

  selectionIsChanging = -1;
  selectionChangedSinceHoldInitiated = false;

  functionSelectionChangedSinceHoldInitiated = -1;
  lastExecutedFunction = -1;

  hasMovedSinceAcquired = false;
  signalAspect = 'X';
}

bool Controller::CheckMovement() {
  PinA_value = digitalRead(encoderPinA);
  bool movementDetected = false;

  if (PinA_value != PinA_prev && PinA_value == HIGH) {
    movementDetected = true;
    Serial.println(mtIndex+" - moveement on A - "+String(PinA_value));
    if (digitalRead(encoderPinB) == HIGH) {
      CW = false;
      KnobPosition--;
      Serial.println(mtIndex + " AC movement " + String(KnobPosition));
    } else {
      // the encoder is rotating in clockwise direction => increase the counter
      CW = true;
      KnobPosition++;
      Serial.println(mtIndex + " CW movement " + String(KnobPosition));
    }
    
    /*
    if (digitalRead(encoderPinB) != PinA_value) {
      CW = true;
      KnobPosition++;
      Serial.println(mtIndex + " CW movement " + String(KnobPosition));
    } else {
      // if pin B state changed before pin A, rotation is counter-clockwise
      CW = false;
      KnobPosition--;
      Serial.println(mtIndex + " AC movement " + String(KnobPosition));
    }
  */

    if (KnobPosition < -1) KnobPosition = -1;
    if (KnobPosition > 126) KnobPosition = 126;
  }
  PinA_prev = PinA_value;

  // check if button is pressed (pin SW)
  bool newButtonState = !digitalRead(encoderBtn);
  if (newButtonState != ButtonState) {
    buttonIsHeld = 0;
    millisAtButtonPress = millis();
    if (newButtonState == HIGH) {
      Serial.println(mtIndex+" Button Pressed");
      ButtonState = 1;
      //Serial.println("Button change pressed");
    }
    else {
      Serial.println(mtIndex+" Button Released");
      ButtonState = 0;
      //Serial.println("Button change released");
    }
  }
  else {
    if (ButtonState == 1 && (millis() - millisAtButtonPress > 1000) && buttonIsHeld == 0) {
      buttonIsHeld = 1;
      Serial.println(mtIndex+" Button held");
    }
  }






  return movementDetected;
}
