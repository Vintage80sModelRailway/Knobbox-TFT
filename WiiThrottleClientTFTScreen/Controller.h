// Controller.h
#ifndef Controller_h
#define Controller_h

#include <Arduino.h>

class Controller {
  private:
    int encoderPinA; // CLK pin
    int encoderPinB; // DT pin
    int encoderBtn; // SW pin

    int PinA_prev;
    int PinA_value;
    bool CW;
    int id;
    unsigned long millisAtButtonPress;
        int InternalCount;

  public:
    //selector
    bool selectionIsChanging;

    //throttle / both
    String TrainName;
    int rosterIndex;
    int newSelectedRosterIndex;
    String mtIndex;
    bool ButtonState;
    bool buttonIsHeld;
    bool selectionChangedSinceHoldInitiated;
    bool hasMovedSinceAcquired;
    bool functionSelectionChangedSinceHoldInitiated;
    int lastExecutedFunction;
    int KnobPosition;
    char signalAspect;
    int tftX;
    int tftY;


    //Controller(String MTIndex = "", int EncoderPinA = -1, int EncoderPinB  = -1, int EncoderBtn  = -1);
    Controller(String MTIndex, int EncoderPinA, int EncoderPinB, int EncoderBtn, int tftLocationX, int tftLocationY);
    bool CheckMovement();
};


#endif
