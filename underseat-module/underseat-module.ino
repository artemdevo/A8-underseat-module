#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>


#define BTN A0
#define D_PAD_UD A1
#define D_PAD_FB A2
#define BEZ_RNG A3
#define MAX_STATE 5 //this is the maximum state number, 1 less than the state count (state 0 is a state)
#define FPSerial softSerial

byte voicestate = 0; //use this just for voice messages
byte truestate = 0;
byte transitionup = 0; 
byte transitiondown = 0;
byte messageplaycount;
SoftwareSerial softSerial(/*rx =*/6, /*tx =*/7);//ON THE UNO THIS NEEDS TO BE RX 6 AND TX 7
DFRobotDFPlayerMini myDFPlayer;
int time1 = 0; //for testing 3/28/2025- i think int will work? there will be overflow but I think it will turn out ok
int time2;// for testing 3/28/2025

void setup() {
  // put your setup code here, to run once:
  myDFPlayer.begin(FPSerial, /*isACK = */true, /*doReset = */false);
  myDFPlayer.volume(15);  //Set volume value. From 0 to 30
  Serial.begin(9600);
}

void loop() {
  
////---------------------------------------------------------------------------------------------------going up when press up on the bezel ring
if(analogRead(BEZ_RNG)>470 && analogRead(BEZ_RNG)<800 && !transitionup){//if up on the bezel ring is pressed, increase state by 1 
  if(voicestate == MAX_STATE){
    voicestate = 0; //if at maximum state, go back to 0
  }
  else{
  voicestate++; //play voice message for whatever state you just changed to, probably make it a switch case?
  }
  transitionup = 1; //prep for the state transition
  messageplaycount = 0;//set to 0 so that the voice message for the voice state will be played 
}

if(analogRead(BEZ_RNG)>1000 && transitionup){//if the bezel ring is released, this completes the state transition up
  if(truestate == MAX_STATE){
    truestate = 0; //if at maximum state, go back to 0
  }
  else{
  truestate++; //complete the state transition up
  }
  transitionup = 0;  
}

/////-----------------------------------------------------------------------------------------------------going down when pressing down on bezel ring
if(analogRead(BEZ_RNG)<470 && !transitiondown){//if down on the bezel ring is pressed, decrease state by 1 
  if(voicestate == 0){
    voicestate = MAX_STATE; //if at 0 state, loop back around to highest state
  }
  else{
  voicestate--; //play voice message for whatever state you just changed to, probably make it a switch case?
  }; 
  transitiondown = 1; //prep for the state transition
  messageplaycount = 0;//set to 0 so that the voice message for the voice state will be played
}

if(analogRead(BEZ_RNG)>1000 && transitiondown){//if the bezel ring is released, this completes the state transition down
  if(truestate == 0){
    truestate = MAX_STATE; //if at 0 state, loop back around to highest state
  }
  else{
  truestate--; //complete the state transition down
  }; 
  transitiondown = 0;
}
//////--------------------------------------------------------------------------------------------------------


/////////-------------------------


if(messageplaycount==0){
  switch(voicestate){
    case 0:{

    }
    break;
    case 1:{

    }
    break;
    case 2:{

    }
    break;
    case 3:{

    }
    break;
    case 4:{

    }
    break;

  }
  messageplaycount++;
}
/////----------------------------------------------------

////-----------------------------------------for testing only, make a thing that prints the truestate and voice state once per second

time2 = millis();

if((time2-time1)>1000)
{
Serial.print(voicestate);
Serial.print(" ");
Serial.print(truestate);
Serial.print(" ");
Serial.print(transitionup);
Serial.print(" ");
Serial.println(transitiondown);
time1 = time2; 
}

///////------------------------------------------------------

//delay(500);///for testing 
}
