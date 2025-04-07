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

byte lumbarpins[6] = {18, 22, 44, 20, 46, 42};
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
};

byte pressurechange;
byte positionchange;
byte pressurereached;
int exitcounterstart;
byte exitcounterfirst;

LumbarStruct lumbar;
SavedLumbarValues savedlumbarvalues;

void setup() {
  
  Serial.begin(9600);
  
  for(int i = 0; i < 6; i++){
    pinMode(lumbarpins[i], OUTPUT);
  }
  pinMode(COMP, OUTPUT);
  pinMode(VENT, OUTPUT);

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
  lumbar.desiredpressure = 600; //set a value for the desired lumbar pressure. this will be
  // sent to the controller from the underseat module, in addition to the chosen lumbar bladder (0 through 2)
  lumbar.on = 1;
}

void loop() {

  if(lumbar.on){//if a new CAN message for lumbar is received, lumbar.on will be set to 1. otherwise, it is 0
    //lumbar.desiredpressure and lumbar.desiredposition come from the CAN message
    
    if(abs(savedlumbarvalues.pressure - lumbar.desiredpressure) > 30){//if the saved pressure value doesn't match the newly received pressure by more than 20
      //lumbar.pressure = lumbar.desiredpressure;
      pressurechange = 1;  
    }
    else if(savedlumbarvalues.position != lumbar.desiredposition){//if the saved position value doesn't match the newly received position
      //lumbar.position = lumbar.desiredposition;
      positionchange = 1;
    }

    if(pressurechange && (lumbar.desiredpressure > savedlumbarvalues.pressure)){
      switch(savedlumbarvalues.position){//since we are changing the pressure in the already chosen bladder, switch based on position
        case 0:{//if the lowest lumbar is the current setting, open its reservoir solenoid. vent the top two 
          digitalWrite(LOW_LUMBAR_BLD, HIGH);
          digitalWrite(MID_LUMBAR_VNT, HIGH);
          digitalWrite(HIGH_LUMBAR_VNT, HIGH);
        }
        break;
        case 1:{//if the middle lumbar is the current setting, open its reservoir solenoid. vent the top and bottom
          digitalWrite(MID_LUMBAR_BLD, HIGH);
          digitalWrite(HIGH_LUMBAR_VNT, HIGH);
          digitalWrite(LOW_LUMBAR_VNT, HIGH);
        }
        break;
        case 2:{//if the highest lumbar is the current setting, open its reservoir solenoid. vent the bottom two
          digitalWrite(HIGH_LUMBAR_BLD, HIGH);
          digitalWrite(LOW_LUMBAR_VNT, HIGH);
          digitalWrite(MID_LUMBAR_VNT, HIGH);
        }
        break;
      }
    }
    else if(pressurechange && (lumbar.desiredpressure < savedlumbarvalues.pressure)){//same as previous loop but also have to vent the bladder
      //if(!pressurereached){
        switch(savedlumbarvalues.position){//since we are changing the pressure in the already chosen bladder, switch based on position
          case 0:{//if the lowest lumbar is the current setting, open its reservoir solenoid and specific vent solenoid, along with the others to release residual
            digitalWrite(LOW_LUMBAR_BLD, HIGH);
            digitalWrite(LOW_LUMBAR_VNT, HIGH);
            digitalWrite(MID_LUMBAR_VNT, HIGH);
            digitalWrite(HIGH_LUMBAR_VNT, HIGH);
          }
          break;
          case 1:{//if the middle lumbar is the current setting, open its reservoir solenoid and specific vent solenoid
            digitalWrite(MID_LUMBAR_BLD, HIGH);
            digitalWrite(LOW_LUMBAR_VNT, HIGH);
            digitalWrite(MID_LUMBAR_VNT, HIGH);
            digitalWrite(HIGH_LUMBAR_VNT, HIGH);
          }
          break;
          case 2:{//if the highest lumbar is the current setting, open its reservoir solenoid and specific vent solenoid
            digitalWrite(HIGH_LUMBAR_BLD, HIGH);
            digitalWrite(LOW_LUMBAR_VNT, HIGH);
            digitalWrite(MID_LUMBAR_VNT, HIGH);
            digitalWrite(HIGH_LUMBAR_VNT, HIGH);
          }
          break;
        }
      }
      /*if(abs((int)(analogRead(PRES) - lumbar.desiredpressure)) < 30) {//if the absolute difference between the measured pressure and desired is less than 30
        pressurereached = 1;
        switch(savedlumbarvalues.position){
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
    }*/
    else if(positionchange){
      switch(lumbar.desiredposition){
        case 0:{//if the desired position is 0, the bottom lumbar bladder
          digitalWrite(LOW_LUMBAR_BLD, HIGH);
          digitalWrite(MID_LUMBAR_VNT, HIGH);
          digitalWrite(HIGH_LUMBAR_VNT, HIGH);
        }
        break;
        case 1:{//if the desired position is 1, the middle lumbar bladder
          digitalWrite(MID_LUMBAR_BLD, HIGH);
          digitalWrite(HIGH_LUMBAR_VNT, HIGH);
          digitalWrite(LOW_LUMBAR_VNT, HIGH);
        }
        break;
        case 2:{//if the desired position is 2, the top lumbar bladder
          digitalWrite(HIGH_LUMBAR_BLD, HIGH);
          digitalWrite(LOW_LUMBAR_VNT, HIGH);
          digitalWrite(MID_LUMBAR_VNT, HIGH);
        }
        break;
      }
    }
    
    if(abs((int)(analogRead(PRES) - lumbar.desiredpressure)) < 30){//if the reservoir pressure is more than 30 greater than desired, turn comp off
                                                        //adding hysteresis with the 30 to avoid compressor jitter
      if(!exitcounterfirst){//if this is the first time through this if statement since this pressure condition has been true, 
        exitcounterstart = millis(); //if the reservoir is at the desired pressure, that means the lumbar bladder must be close as well, so start a counter
        exitcounterfirst++;
      }
    }
    if((analogRead(PRES) - lumbar.desiredpressure) > 30){//if the reservoir pressure is more than 30 greater than desired, turn comp off
                                                        //adding hysteresis with the 30 to avoid compressor jitter
      digitalWrite(COMP, LOW);
      exitcounterfirst = 0; //reset exitcounterfirst, since the pressure was outside bounds
    }
    if((int)(analogRead(PRES) - lumbar.desiredpressure) < -30){//if the reservoir pressure is more than 30 less than desired, turn comp on
                                                        //adding hysteresis with the 30 to avoid compressor jitter
      digitalWrite(COMP, HIGH);
      exitcounterfirst = 0;
    }
    if(millis()-exitcounterstart == 5000){//if more than 5 seconds have elapsed with the real pressure within +/- 30 of the desired
      lumbar.on = 0;
      exitcounterfirst = 0;
      pressurereached = 0;
      pressurechange = 0;
      positionchange = 0;
      
      EEPROM.put(0, lumbar.desiredpressure);//write the desired pressure to saved addresses;
      EEPROM.put(2, lumbar.desiredposition);//write the desired position to saved address;
    }
    
  }
  else if(!lumbar.on){
    for(int i = 0; i < 6; i++){
      digitalWrite(lumbarpins[i], LOW);
    }
  }
  //Serial.println(pressure.value);
}