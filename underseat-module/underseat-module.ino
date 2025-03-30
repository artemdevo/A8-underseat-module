#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
#include <SPI.h>
#include <mcp_can.h>


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
byte massageon = 0;
byte massagetransition = 0;
unsigned long massagestarttime;
unsigned long canmessagetime = 0;
SoftwareSerial softSerial(/*rx =*/6, /*tx =*/7);//ON THE UNO THIS NEEDS TO BE RX 6 AND TX 7
DFRobotDFPlayerMini myDFPlayer;
int time1 = 0; //for testing 3/28/2025- i think int will work? there will be overflow but I think it will turn out ok
int time2;// for testing 3/28/2025

MCP_CAN CAN0(10); //CS is pin 10 on arduino uno

void setup() {
  // put your setup code here, to run once:
  myDFPlayer.begin(FPSerial, /*isACK = */true, /*doReset = */false);
  myDFPlayer.volume(15);  //Set volume value. From 0 to 30
  Serial.begin(9600);
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
  messageplaycount = 1; //this is how i am avoiding a state 0 audio message from playing when the seat is turned on. if the user cycles back to state 0, the message will play
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
  transitiondown = 0; //make sure transitiondown is 0
  messageplaycount = 0;//set to 0 so that the voice message for the voice state will be played 
}

if(analogRead(BEZ_RNG)>900 && transitionup){//if the bezel ring is released, this completes the state transition up
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
  transitionup = 0; //make sure transitionup is off
  messageplaycount = 0;//set to 0 so that the voice message for the voice state will be played
}

if(analogRead(BEZ_RNG)>900 && transitiondown){//if the bezel ring is released, this completes the state transition down
  if(truestate == 0){
    truestate = MAX_STATE; //if at 0 state, loop back around to highest state
  }
  else{
  truestate--; //complete the state transition down
  }; 
  transitiondown = 0;
}
//////--------------------------------------------------------------------------------------------------------

////////-----------------------------------------------------------------------------massage button and massage function stuff. adjustments to the massage will be done in a state
if(analogRead(BTN)<500 && !massagetransition){
  if(massageon){//if button is pressed with massage on
    massageon = 0;
  }
  else if(!massageon){//start the massage when button pressed measure time start
    massageon = 1;
    massagetransition = 1;
    massagestarttime = millis();
  }
}
if(massageon){//turn massage off if 10 minutes have elapsed
  if((millis()-canmessagetime) > 200){//if more than 150 ms have elapsed, send CAN message

    //CAN0.sendMsgBuf(0x3C0, 0, 4, ignit); placeholder, need to figure out messageframe 
    canmessagetime = millis();
  }
  else if((millis()-massagestarttime)>1000*60*10){
    massageon = 0;
  }
}

if(analogRead(BTN)>700 && massagetransition){//if you let go of the massage button, return massagetransition to value 0
  massagetransition = 0;
}





/////////-------------------------


if(messageplaycount==0){
  switch(voicestate){
    case 0:{
      //this is the basestate, so no voice message here on startup (would be annoying), but can activate massage from it
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
