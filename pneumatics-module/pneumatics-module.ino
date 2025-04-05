#include "Arduino.h"
#include <SPI.h>
#include <mcp_can.h>


#define COMP 10
#define VENT 23
#define PRES A2
#define MAX_MASSAGE_STATE 4
#define CAN0_INT 2 //change this to match the mega

byte pins[22]={10, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};
byte bladderpins[20] = {18, 19, 20, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};
byte lumbarpins[3] = {18, 22, 44};
byte lumbarvents[3] = {20, 46, 42};
byte massagepins[10] = {19, 21, 24, 26, 28, 30, 34, 36, 38, 40};
MCP_CAN CAN0(53);     // Set CS to pin 10, change this to 53 for mega


byte massagestate; //no need to assign this a value, it should start at state 0
unsigned long statestarttime; //set this equal to millis() when moving to the next massage state
unsigned int delaytime = 400; //give this an initial value, matters for the transition away from the starting state.
byte massagestatus; //this will be another global value stating if massage is on, and if so, which program to run. massagestatus will be set by reading CAN messages

void setup() {
  // put your setup code here, to run once:
  for (int i = 0; i <22; i++){
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  } 

  Serial.begin(9600);
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
}

void loop() {

  massagestatus = 1;
  
  if(massagestatus){//if massagestatus is 1, 2, 3 etc

    switch(massagestate){//setting pins high and returning previous ones to low based on the current state
      case 0:{
        digitalWrite(28, HIGH);
        digitalWrite(34, HIGH);
        digitalWrite(24, LOW);
        digitalWrite(21, LOW);
      }
      break;
      case 1:{
        digitalWrite(26, HIGH);
        digitalWrite(36, HIGH);
        digitalWrite(28, LOW);
        digitalWrite(34, LOW);
      }
      break;
      case 2:{
        digitalWrite(38, HIGH);
        digitalWrite(30, HIGH);
        digitalWrite(26, LOW);
        digitalWrite(36, LOW);
      }
      case 3:{
        digitalWrite(19, HIGH);
        digitalWrite(40, HIGH);
        digitalWrite(38, LOW);
        digitalWrite(30, LOW);
      }
      case 4:{
        digitalWrite(24, HIGH);
        digitalWrite(21, HIGH);
        digitalWrite(19, LOW);
        digitalWrite(40, LOW);
      }
    }

    if(millis()-statestarttime > delaytime){ //if the time has elapsed to change state, advance the state
      if(massagestate == MAX_MASSAGE_STATE){
        massagestate=0;   
      }
      else{
        massagestate++;
      }
    
      statestarttime = millis();

      if(massagestate%2 == 0){ //setting the delay time to the next state here 
        delaytime = 500;
      }
      else{
        delaytime = 500;
      }
    }

    if(analogRead(PRES)<800){//during the massage, compressor should always be running but should not exceed a pressure of 800
      digitalWrite(COMP, HIGH);
    }
    else{
      digitalWrite(COMP, LOW);
    }


  }
  else{ //if massage is off, set all massage pins low. this is so that whatever was on from the last massage state is set back to 0
    for(int i= 0; i < 10; i++ ){
      digitalWrite(massagepins[i],LOW);
    }
  }
  
}


