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
byte len = 0;
byte rxBuf[8];
char msgString[128];                        // Array to store serial string



struct SavedLumbarValues {
  byte byte0;
  byte byte1;
  int pressure;
  byte position;
};

struct LumbarStruct {
  byte position;//which bladder to choose. not using this?
  int pressure;//how much pressure in the bladder. not using this?
  byte on; //on or off
  int desiredpressure; //the value received in the CAN message
  byte desiredposition; //the value received in the CAN message
  byte pressureup;
  byte pressuredown;
  byte positionup;
  byte positiondown;
  byte dpadpositiontransition;
  byte bladderchange;
  byte pressuresavetimersuppress;
  long int pressuresavetimer;
};

struct BladderStruct{
  long int exitcounterstarttime;
  byte exitcounterstarted;
};

int observedpressure;
//byte pressurechange;
//byte positionchange;
//byte pressurereached;


//byte exitenable;

LumbarStruct lumbar;
SavedLumbarValues savedlumbarvalues;
BladderStruct bladder;

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
  //lumbar.on = 1;
  //digitalWrite(LOW_LUMBAR_VNT, HIGH);
  //digitalWrite(MID_LUMBAR_VNT, HIGH);
  //digitalWrite(HIGH_LUMBAR_VNT, HIGH);
  //delay(5000);
  //digitalWrite(LOW_LUMBAR_VNT, LOW);
  //digitalWrite(MID_LUMBAR_VNT, LOW);
  //digitalWrite(HIGH_LUMBAR_VNT, LOW);
  
}

void loop() {
  observedpressure = analogRead(PRES); //do the pressure read first (will be useful for the other functions and stuff)

  if(!digitalRead(CAN0_INT)){                         // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    if(rxId == 0x707){//if the newest received frame has an id of 0x707 (lumbar adjustment)
      //lumbar.on = 1;//start lumbar

      lumbar.positionup = rxBuf[0];
      lumbar.positiondown = rxBuf[1];
      lumbar.pressureup = rxBuf[2];
      lumbar.pressuredown = rxBuf[3];
    }
  }
  if(lumbar.positionup && !lumbar.dpadpositiontransition){
    if(lumbar.desiredposition < 2){
      lumbar.desiredposition++;
      lumbar.bladderchange = 1;
      
    }
    lumbar.dpadpositiontransition = 1;
    //Serial.println(lumbar.desiredposition);
  }
  else if(lumbar.positiondown && !lumbar.dpadpositiontransition){
    if(lumbar.desiredposition> 0){
      lumbar.desiredposition--;
      lumbar.bladderchange = 1;
    }
    lumbar.dpadpositiontransition = 1;
    //Serial.println(lumbar.desiredposition);
  }

  if(!lumbar.positionup && !lumbar.positiondown){//if neither up or down are being pressed on the dpad
    lumbar.dpadpositiontransition = 0; //set transition back to 0 so that values can change
  }

  if(lumbar.pressureup || lumbar.pressuredown){
    lumbar.bladderchange = 0;//set bladder change to 0 if pressure is commanded to change. this is an easy way to override a bladder change in progress
  }

  if(lumbar.pressureup || lumbar.pressuredown || lumbar.positionup || lumbar.positiondown){
    lumbar.on = 1;
  }
  

  if(lumbar.on){//if a new CAN message for lumbar is received, lumbar.on will be set to 1. otherwise, it is 0
    //lumbar.desiredpressure and lumbar.desiredposition come from the CAN message
    
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
      if(observedpressure - lumbar.desiredpressure >= 50){ //turn the specific bladder vent on if the pressure is more than 30 higher. is this even possible to occur?
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
      else if(observedpressure - lumbar.desiredpressure <= -50){//if the reservoir pressure is more than 50 less than desired, turn comp on                               
        digitalWrite(COMP, HIGH);
      }

      if(abs(observedpressure - lumbar.desiredpressure) < 50){//if within 50 of the target pressure, start the exitcounter
        if(!bladder.exitcounterstarted){//if this is the first time through this if statement since this pressure condition has been true, 
          bladder.exitcounterstarttime = millis(); //if the reservoir is at the desired pressure, that means the lumbar bladder must be close as well, so start a counter
          bladder.exitcounterstarted = 1;//so we don't refresh exitcountertime
          //exitenable = 1;
          Serial.println("is the exit start time being refreshed?");
        }
      }
      else{
        bladder.exitcounterstarted = 0;
        //exitenable = 0;
      }
      if(bladder.exitcounterstarted){//if more than 2 seconds have elapsed with the real pressure within +/- 50 of the desired
        if (millis()-bladder.exitcounterstarttime > 2000){//if current time minus the start of the exit counter exceeds 2000 ms
          lumbar.bladderchange= 0;
          bladder.exitcounterstarted = 0;
          lumbar.on = 0;
          EEPROM.put(2, lumbar.desiredposition);//write the desired bladder position to saved address;
          Serial.println("Bladder position saved to eeprom!");
          //exitenable = 0;
      
        }
      }
    }
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    /////~~~~~~~~~~~~~~~~~~~~~~~~~~~CODE FOR PRESSURE ADJUSTMENT AND SAVING VALUE TO EEPROM
    if(!lumbar.bladderchange){//if we aren't doing a bladder change, it must be a pressure change 
      if(observedpressure > 800 || !lumbar.pressureup){
        digitalWrite(COMP, LOW);//turn compressor off if measured pressure exceeds 800 or if lumbar.pressureup is not high;
      }
      
      else if(lumbar.pressureup && observedpressure < 750){
        digitalWrite(COMP, HIGH);//turn comp on if measured pressure is below 750 and pressure increase is commanded high
      }
      
      
      if(lumbar.pressuredown){
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
        switch(lumbar.desiredposition){//if there is no command to drop pressure, vent the current solenoid 
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
      }
      if(lumbar.pressureup || lumbar.pressuredown){
        lumbar.pressuresavetimer = millis();
        lumbar.pressuresavetimersuppress = 0; //
      }
      if(millis() - lumbar.pressuresavetimer > 500 && !lumbar.pressuresavetimersuppress){
        EEPROM.put(0, observedpressure);
        lumbar.desiredpressure = observedpressure;
        lumbar.pressuresavetimersuppress = 1;
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
  Serial.print(observedpressure);
  Serial.print(" ");
  Serial.print(lumbar.desiredpressure);
  Serial.print(" ");
  Serial.print(lumbar.desiredposition);
   Serial.print(" ");
  Serial.print(bladder.exitcounterstarted);
  Serial.print(" ");
  Serial.print(millis()-bladder.exitcounterstarttime);
  Serial.print(" ");
  Serial.print(lumbar.bladderchange);
  Serial.println(" ");
  //Serial.println(lumbar.pressuredown);
    //Serial.println(bladder.exitcounterstarted);
  //Serial.println(millis() - bladder.exitcounterstarttime);
}