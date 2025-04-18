#include <mcp_can.h>
#include <SPI.h>
#include <EEPROM.h>

#define COMP 10
#define VENT 23
#define PRES A2

#define HIGH_LUMBAR_BLD 18
#define MID_LUMBAR_BLD 22
#define LOW_LUMBAR_BLD 44
#define HIGH_LUMBAR_VNT 20
#define MID_LUMBAR_VNT 46
#define LOW_LUMBAR_VNT 42
#define CAN0_INT 41 //interrupt pin for CAN module   

byte lumbarpins[8] = {18, 22, 44, 20, 46, 42, 10, 23};

MCP_CAN CAN0(53); //CS default pin on arduino mega

long unsigned int rxId;
byte len;
byte rxBuf[8];
//char msgString[128];   //TEST WITH THIS DISABLED   works? 4-18-25                  // Array to store serial string

struct SavedLumbarValues {
  byte byte0;
  byte byte1;
  int pressure;
  byte position;
};

struct LumbarStruct {
  //byte position;//which bladder to choose. not using this?
  //int pressure;//how much pressure in the bladder. not using this?
  byte on; //on or off. this applies to both pressure and position adjustments
  int desiredpressure; //the value received in the CAN message
  byte desiredposition; //the value received in the CAN message
  byte bladderchange;
  byte pressuresavetimerenable;
  long int pressuresavestarttime;
};

struct BladderStruct{//stuff specific for a bladder change
  long int exittimerstarttime;
  byte exittimerenable;
};

struct DPadStruct{
  byte up;
  byte down;
  byte forward;
  byte backward;
  byte transition;
  long int lastmessagetimer;
};

int observedpressure;

LumbarStruct lumbar;
SavedLumbarValues savedlumbarvalues;
BladderStruct bladder;
DPadStruct dpad;

void lumbarAdjustfunction();

void setup() {
  
  Serial.begin(115200);

  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
  pinMode(CAN0_INT, INPUT);

  for(int i = 0; i < 8; i++){
    pinMode(lumbarpins[i], OUTPUT);
  }

  savedlumbarvalues.byte0= EEPROM.read(0);//get the saved lumbar values from eeprom on startup
  savedlumbarvalues.byte1 = EEPROM.read(1);
  savedlumbarvalues.pressure = (savedlumbarvalues.byte1 << 8) | savedlumbarvalues.byte0;
  savedlumbarvalues.position = EEPROM.read(2);

  if(savedlumbarvalues.pressure < 0 || savedlumbarvalues.pressure > 800 || savedlumbarvalues.position > 2){//if the read pressure value is -1 (indicating a value of 0xffff in these two bytes, for EEPROM with no writes, set it to 0)
                            //NOTE: this only works for atmega based MCUs that use 2 byte ints. also using >800 value if it somehow becomes corrupted 
    for(int i = 0; i < 3; i++){//0 through 2 because it is also clearing the address for the lumbar bladder state
      EEPROM.write(i, 0);
      savedlumbarvalues.pressure = 0;
      savedlumbarvalues.position = 0;
    }
  }
  Serial.println(savedlumbarvalues.pressure);
  Serial.println(savedlumbarvalues.position);
  
  lumbar.desiredpressure = savedlumbarvalues.pressure; //set a value for the desired lumbar pressure. this will be
  lumbar.desiredposition = savedlumbarvalues.position;

  
}

void loop() {
  observedpressure = analogRead(PRES); //do the pressure read first (will be useful for the other functions and stuff)

  if(!digitalRead(CAN0_INT)){                         // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
  }

  if(rxId == 0x707 || rxId == 0x650 || rxId== 0x751){
    dpad.up = rxBuf[0];
    dpad.down = rxBuf[1];
    dpad.forward = rxBuf[2];
    dpad.backward = rxBuf[3];
    dpad.lastmessagetimer = millis();
  }

  
  if(!dpad.up && !dpad.down && !dpad.forward && !dpad.backward){//set the transition byte low if all of the values are low. transition gets set to 1 when button pressed
    dpad.transition = 0;
  }
  

  ///////////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  switch(rxId){
    case 0x707:{
      if(dpad.up || dpad.down || dpad.forward || dpad.backward){//set lumbar on if any of the buttons are pressed and the rxID is for lumbar
        lumbar.on = 1;
        
      }
    }
    break;
    case 0x650:{//0x650 (massage)

    }
    break;
    case 0x751:{//if the newest received frame has an id of 0x751 (side bolster adjustment)

    }
    break;
  }
  
  if(millis()- dpad.lastmessagetimer > 500){//if more than 500 ms has passed since any usable message was received, turn everything off
    lumbar.on = 0;
    Serial.println("CAN wait timeout");
    //massage.on = 0;
    //bolster.on = 0;
  }
  ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~LUMBAR FUNCTION STUFF ALL BELOW

  lumbarAdjustfunction();


  Serial.print(observedpressure);
  Serial.print(" ");
  Serial.print(lumbar.desiredpressure);
  Serial.print(" ");
  
  Serial.print(lumbar.desiredposition);
  Serial.print(" ");
  
  Serial.print(bladder.exittimerenable);
  Serial.print(" ");
  
 // Serial.print(" ");
  Serial.println(millis()-bladder.exittimerstarttime);
  
  
  
    

}



void lumbarAdjustfunction(){
  if(dpad.up && !dpad.transition){
    if(lumbar.desiredposition < 2){
      lumbar.desiredposition++;
      lumbar.bladderchange = 1;
      dpad.transition = 1;
      
    }
    else if(lumbar.desiredposition == 2){//
      lumbar.on = 0;
    }
  
  }
  else if(dpad.down && !dpad.transition){
    if(lumbar.desiredposition> 0){
      lumbar.desiredposition--;
      lumbar.bladderchange = 1;
      dpad.transition = 1;
    }
    else if(lumbar.desiredposition == 0){
      lumbar.on = 0;
    }
    
  }

  if(dpad.forward || dpad.backward){
    lumbar.bladderchange = 0;//set bladder change to 0 if pressure is commanded to change. this is an easy way to override a bladder change in progress
  }

  if(lumbar.on){//if a new CAN message for lumbar is received, lumbar.on will be set to 1. otherwise, it is 0
    
    switch(lumbar.desiredposition){
      case 0:{//if the desired position is 0, the bottom lumbar bladder is switched on, others vented
        digitalWrite(LOW_LUMBAR_BLD, HIGH);
        digitalWrite(MID_LUMBAR_BLD, LOW);
        digitalWrite(HIGH_LUMBAR_BLD, LOW);
        digitalWrite(MID_LUMBAR_VNT, HIGH);
        digitalWrite(HIGH_LUMBAR_VNT, HIGH);
      }
      break;
      case 1:{//if the desired position is 1, the middle lumbar bladder is switched on, others vented
        digitalWrite(LOW_LUMBAR_BLD, LOW);
        digitalWrite(MID_LUMBAR_BLD, HIGH);
        digitalWrite(HIGH_LUMBAR_BLD, LOW);
        digitalWrite(HIGH_LUMBAR_VNT, HIGH);
        digitalWrite(LOW_LUMBAR_VNT, HIGH);
      }
      break;
      case 2:{//if the desired position is 2, the top lumbar bladder is switched on, others vented
        digitalWrite(LOW_LUMBAR_BLD, LOW);
        digitalWrite(MID_LUMBAR_BLD, LOW);
        digitalWrite(HIGH_LUMBAR_BLD, HIGH);
        digitalWrite(LOW_LUMBAR_VNT, HIGH);
        digitalWrite(MID_LUMBAR_VNT, HIGH);
      }
      break;
    }
    ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~CODE FOR SWITCHING BLADDERS
    if(lumbar.bladderchange){
      if(observedpressure - lumbar.desiredpressure >= 30){ //turn the specific bladder vent on if the pressure is more than 50 higher. is this even possible to occur?
        switch(lumbar.desiredposition){
          case 0:{//if the desired position is 0 and the pressure is too high, the bottom vent is used
          digitalWrite(LOW_LUMBAR_VNT, HIGH);
          }
          break;
          case 1:{//if the desired position is 1, middle vent used
          digitalWrite(MID_LUMBAR_VNT, HIGH);
          }
          break;
          case 2:{//if the desired position is 2, top vent used
          digitalWrite(HIGH_LUMBAR_VNT, HIGH);
          }
          break;
        }
        Serial.println("opening vent");
      }
      else if (observedpressure - lumbar.desiredpressure <= 0){ //if the pressure is more than 0 less than the desired
        switch(lumbar.desiredposition){
          case 0:{//if the desired position is 0 and the pressure is too high, the bottom vent is used
          digitalWrite(LOW_LUMBAR_VNT, LOW);
          }
          break;
          case 1:{//if the desired position is 1, middle vent used
          digitalWrite(MID_LUMBAR_VNT, LOW);
          }
          break;
          case 2:{//if the desired position is 2, top vent used
          digitalWrite(HIGH_LUMBAR_VNT, LOW);
          }
          break;
        }
        Serial.println("closing vent");
      }

      if(observedpressure - lumbar.desiredpressure >= 0){//if the reservoir pressure is more than 0 over desired, turn comp off                                          
        digitalWrite(COMP, LOW);
      }
      else if(observedpressure - lumbar.desiredpressure <= -30){//if the reservoir pressure is more than 50 less than desired, turn comp on                               
        digitalWrite(COMP, HIGH);
      }

      if(abs(observedpressure - lumbar.desiredpressure) < 30){//if within 50 of the target pressure, start the exitcounter
        if(!bladder.exittimerenable){//if this is the first time through this if statement since this pressure condition has been true, 
          bladder.exittimerstarttime = millis(); //if the reservoir is at the desired pressure, that means the lumbar bladder must be close as well, so start a counter
          bladder.exittimerenable = 1;//so we don't refresh exitcountertime
          //exitenable = 1;
          Serial.println("is the exit start time being refreshed?");
        }
      }
      else{
        bladder.exittimerenable = 0;
        
      }
      if(bladder.exittimerenable){//if more than 2 seconds have elapsed with the real pressure within +/- 50 of the desired
        if (millis()-bladder.exittimerstarttime > 2000){//if current time minus the start of the exit counter exceeds 2000 ms
          lumbar.bladderchange= 0;
          bladder.exittimerenable = 0;
          lumbar.on = 0;
          EEPROM.put(2, lumbar.desiredposition);//write the desired bladder position to saved address;
          Serial.println("Bladder position saved to eeprom!");
          
        }
      }
    }
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    /////~~~~~~~~~~~~~~~~~~~~~~~~~~~CODE FOR PRESSURE ADJUSTMENT AND SAVING VALUE TO EEPROM
    if(!lumbar.bladderchange){//if we aren't doing a bladder change and lumbar is on, it must be a pressure change

      if(observedpressure > 730 || !dpad.forward){
        digitalWrite(COMP, LOW);//turn compressor off if measured pressure exceeds 750 or if lumbar.pressureup is not high;
      }
      
      else if(dpad.forward && observedpressure < 700){
        digitalWrite(COMP, HIGH);//turn comp on if measured pressure is below 700 and pressure increase is commanded high
      }
      
      if(dpad.backward){
        Serial.println("venting to lower pressure");
        switch(lumbar.desiredposition){
          case 0:{//if the desired position is 0 and the pressure is too high, the bottom vent is used
          digitalWrite(LOW_LUMBAR_VNT, HIGH);
          }
          break;
          case 1:{//if the desired position is 1, middle vent used
          digitalWrite(MID_LUMBAR_VNT, HIGH);
          }
          break;
          case 2:{//if the desired position is 2, top vent used
          digitalWrite(HIGH_LUMBAR_VNT, HIGH);
          }
          break;
        }
      }
      else{
       
        switch(lumbar.desiredposition){//if there is no command to drop pressure, turn the respective solenoid off
          case 0:{
          digitalWrite(LOW_LUMBAR_VNT, LOW);
          }
          break;
          case 1:{
          digitalWrite(MID_LUMBAR_VNT, LOW);
          }
          break;
          case 2:{
          digitalWrite(HIGH_LUMBAR_VNT, LOW);
          }
          break;
        }
      }
      if(dpad.forward || dpad.backward){
        lumbar.pressuresavestarttime = millis();//set the save timer equal to the current time as long as pressure up or down are on
        lumbar.pressuresavetimerenable = 1; //
      }
      if(millis() - lumbar.pressuresavestarttime > 300 && lumbar.pressuresavetimerenable){//if 300 ms pass without the save timer being "reset", save and exit
        EEPROM.put(0, observedpressure);
        lumbar.desiredpressure = observedpressure;
        lumbar.pressuresavetimerenable = 0;
        lumbar.on = 0;
        Serial.println("Saving pressure value to EEPROM!");
      }
    }
    ////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~`
    
  }
  else if(!lumbar.on){
    Serial.println("lumbar off!~~~~~~~~~~~~~~~~~");
    for(int i = 0; i < 8; i++){
      digitalWrite(lumbarpins[i], LOW);
      
    }
  }





}