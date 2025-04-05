#include "Arduino.h"
#include <SPI.h>
#include <mcp_can.h>

//WILL HAVE WAVE, STRETCH, AND LUMBAR (SHORTER STRETCH) MASSAGE MODES
//"HIGH" WILL BE 2700 MS DELAY. "MEDIUM" IS 2000 MS DELAY, "LOW" IS 1000 MS DELAY



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
unsigned int delaytime = 1000; //give this an initial value, matters for the transition away from the starting state.
byte massagemode; //this will be another global value stating if massage is on, and if so, which program to run. massagestatus will be set by reading CAN messages

void setup() {
  // put your setup code here, to run once:
  for (int i = 0; i <22; i++){
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  } 

  Serial.begin(9600);
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);

  digitalWrite(VENT, HIGH);//these initialized vents help the reservoir pressure start lower when massaging, less strain on compressor?
  delay(500);
  digitalWrite(VENT, LOW);
}

void loop() {

  massagemode = 1;
  
  if(massagemode){//if massagestatus is 1, 2, 3 etc
    switch(massagemode){//this is for selecting which massage mode. 1 = wave. 2 = stretch, 3 = lumbar stretch
      case 1:{
        switch(massagestate){//for setting pins in a particular massage state for massage mode 1
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
          break;
          case 3:{
            digitalWrite(19, HIGH);
            digitalWrite(40, HIGH);
            digitalWrite(38, LOW);
            digitalWrite(30, LOW);
          }
          break;
          case 4:{
            digitalWrite(24, HIGH);
            digitalWrite(21, HIGH);
            digitalWrite(19, LOW);
            digitalWrite(40, LOW);
          }
          break;
        }
      }
      break;
      case 2:{ //for setting pins in a particular massage state for massage mode 2
        switch(massagestate){//for setting pins in a particular massage state for massage mode 2
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
          break;
          case 3:{
            digitalWrite(19, HIGH);
            digitalWrite(40, HIGH);
            digitalWrite(38, LOW);
            digitalWrite(30, LOW);
          }
          break;
          case 4:{
            digitalWrite(24, HIGH);
            digitalWrite(21, HIGH);
            digitalWrite(19, LOW);
            digitalWrite(40, LOW);
          }
          break;
        }
      } 
      break;
      case 3:{//for setting pins in a particular massage state for massage mode 3
        switch(massagestate){//for setting pins in a particular massage state for massage mode 3
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
          break;
        }
      }
      break;
    }
    if(millis()-statestarttime > delaytime){ //if the time has elapsed to change state, advance the state
      
      if((massagemode == 1 || massagemode == 2) && massagestate < 4){//if we are in massage mode 1 or 2, being at state 4 or higher (if that somehow happens)
                                                                  //return to state 0
        massagestate = 0;   
      }
      else if(massagemode == 3 && massagestate < 2){//if we are in massagemode 3 and at state 2 or higher, return state to 0
        massagestate = 0;
      }
      else{
        massagestate++;
      }
      statestarttime = millis();
    }

    if(analogRead(PRES)<800){//during the massage, compressor should always be running but should not exceed a pressure of 800. MAYBE PUT THIS OUTSIDE THE MASSAGE FUNCTION? 
                              //CAN APPLY TO THE OTHER PNEUMATICS AS WELL
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
  Serial.println(analogRead(PRES));
  //digitalWrite(VENT, HIGH);
  //delay(100);
  //digitalWrite(VENT, LOW);

}

///NEED TO WRITE A STATEMENT TO SET COMP LOW IF MASSAGE AND BOLSTERS AND LUMBAR ARE NOT ACTIVE

