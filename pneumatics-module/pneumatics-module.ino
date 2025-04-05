#include "Arduino.h"
#include <SPI.h>
#include <mcp_can.h>


#define COMP 10
#define VENT 23
#define PRES A2

byte pins[22]={10, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};
byte bladderpins[20] = {18, 19, 20, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};
byte lumbarpins[3] = {18, 22, 44};
byte lumbarvents[3] = {20, 46, 42};
MCP_CAN CAN0(53);     // Set CS to pin 10, change this to 53 for mega

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
  // put your main code here, to run repeatedly:
  
  
  for(int i = 2; i < 3; i++){//changing this value to see what the different solenoids do
    digitalWrite(30, HIGH);

    while(analogRead(PRES)<700){
      digitalWrite(COMP, HIGH);
    }
    digitalWrite(COMP, LOW);
    delay(2000);

    while(analogRead(PRES)>50){
      digitalWrite(VENT,HIGH);
    }
    digitalWrite(VENT, LOW);
    //Serial.println(analogRead(PRES));
    delay(2000);
    digitalWrite(30, LOW);
    delay(50);
  }
  
  
}


/* 
 

 byte massagestate; //no need to assign this a value, it should start at state 0
 unsigned long statestart; //set this equal to millis() when the massage starts
 unsigned int delaytime; //give this an initial value

 byte massagestatus; //this will be another global value stating if massage is on, and if so, which program to run. massagestatus will be set by reading CAN messages

  if(massagestatus){
    if(millis()-statestart > delaytime){ //
      if(massagestate == MAX_MASSAGE_STATE){
        massagestate=0;   
      }
      else{
        massagestate++;
      }
    
      statestart = millis();

      if(massagestate%2 == 0){ //setting the delay time to the next state here 
        delaytime = 200;
      }
      else{
        delaytime = 400;
      }
    }

    switch(massagestate){
    
      case 0:{
        //assign pins to be high here, and assign pins from the previous state to be low (if applicable);
      }
      break;
      case 1:{
      }
      break;
      case 2:{
        myDFPlayer.play(3);
      }
  
    }
  }
  else{
    switch(massagestate){
    
      case 0:{
        //this is a second switch case that for a given state above, this case sets the pins in that particular massage state low. This 
        //is to set massage pins low when massagestatus goes to 0, and its faster than setting every single massage pin to 0 in a for loop
        //on second thought, maybe i should just put a for loop here and set everything to LOW?
        
      }
      break;
      case 1:{
      }
      break;
      case 2:{
        myDFPlayer.play(3);
      }






  }











