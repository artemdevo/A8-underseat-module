#include <mcp_can.h>
#include <SPI.h>

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
};

int observedpressure;
//byte pressurechange;
//byte positionchange;
//byte pressurereached;
int exitcounterstarttime;
byte exitcounterbegin;

LumbarStruct lumbar;
SavedLumbarValues savedlumbarvalues;

void setup() {
  
  Serial.begin(115200);

  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
  pinMode(CAN0_INT, INPUT);
  for(int i = 0; i < 8; i++){
    pinMode(lumbarpins[i], OUTPUT);
  }

  /*savedlumbarvalues.byte0= EEPROM.read(0);//get the saved lumbar values from eeprom on startup
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
  
  lumbar.desiredpressure = 500; //set a value for the desired lumbar pressure. this will be
   //sent to the controller from the underseat module, in addition to the chosen lumbar bladder (0 through 2)
  lumbar.desiredposition = 2;
  //lumbar.on = 1;
  //digitalWrite(LOW_LUMBAR_VNT, HIGH);
  //digitalWrite(MID_LUMBAR_VNT, HIGH);
  //digitalWrite(HIGH_LUMBAR_VNT, HIGH);
  //delay(5000);
  //digitalWrite(LOW_LUMBAR_VNT, LOW);
  //digitalWrite(MID_LUMBAR_VNT, LOW);
  //digitalWrite(HIGH_LUMBAR_VNT, LOW);
  */
}

void loop() {
  if(!digitalRead(CAN0_INT)){                         // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    if(rxId == 0x707){//if the newest received frame has an id of 0x707 (lumbar adjustment)
      lumbar.on = 1;//start lumbar
      lumbar.desiredpressure = (rxBuf[0] << 8) | rxBuf[1]; //reconstructing the pressure value from the message. byte 0 is MSB
      lumbar.desiredposition = rxBuf[2];
      Serial.println(lumbar.desiredpressure);
      Serial.println(lumbar.desiredposition);

    }
    if(lumbar.on){//if a new CAN message for lumbar is received, lumbar.on will be set to 1. otherwise, it is 0
      //lumbar.desiredpressure and lumbar.desiredposition come from the CAN message
      
      switch(lumbar.desiredposition){
        case 0:{//if the desired position is 0, the bottom lumbar bladder is switched on, others vented
          digitalWrite(LOW_LUMBAR_BLD, HIGH);
          digitalWrite(MID_LUMBAR_VNT, HIGH);
          digitalWrite(HIGH_LUMBAR_VNT, HIGH);
        }
        break;
        case 1:{//if the desired position is 1, the middle lumbar bladder is switched on, others vented
          digitalWrite(MID_LUMBAR_BLD, HIGH);
          digitalWrite(HIGH_LUMBAR_VNT, HIGH);
          digitalWrite(LOW_LUMBAR_VNT, HIGH);
        }
        break;
        case 2:{//if the desired position is 2, the top lumbar bladder is switched on, others vented
          digitalWrite(HIGH_LUMBAR_BLD, HIGH);
          digitalWrite(LOW_LUMBAR_VNT, HIGH);
          digitalWrite(MID_LUMBAR_VNT, HIGH);
        }
        break;
      }
      
      observedpressure = analogRead(PRES);

      if(abs(observedpressure - lumbar.desiredpressure) < 30){//if within 30 of the target pressure, start the exitcounter
                                                          //adding hysteresis with the 30 to avoid compressor jitter
        if(!exitcounterbegin){//if this is the first time through this if statement since this pressure condition has been true, 
          exitcounterstarttime = millis(); //if the reservoir is at the desired pressure, that means the lumbar bladder must be close as well, so start a counter
          exitcounterbegin = 1;
        }
        
      }
      else if(observedpressure - lumbar.desiredpressure >= 0){//if the reservoir pressure is more than 30 greater than desired, turn comp off
                                                          //adding hysteresis with the 30 to avoid compressor jitter
        digitalWrite(COMP, LOW);

        exitcounterbegin = 0; //reset exitcounterbegin, since the pressure was outside bounds
      }
      else if(observedpressure - lumbar.desiredpressure <= -30){//if the reservoir pressure is more than 30 less than desired, turn comp on
                                                          //adding hysteresis with the 30 to avoid compressor jitter
        digitalWrite(COMP, HIGH);
        exitcounterbegin = 0;
      }

      if(observedpressure - lumbar.desiredpressure >= 30){ //turn the specific bladder vent on if the pressure is more than 30 higher 

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

      if(millis()-exitcounterstarttime > 3000 && exitcounterbegin){//if more than 5 seconds have elapsed with the real pressure within +/- 30 of the desired
        lumbar.on = 0;
        exitcounterbegin = 0;
        
        //EEPROM.put(0, lumbar.desiredpressure);//write the desired pressure to saved addresses;don't need this anymore
        //EEPROM.put(2, lumbar.desiredposition);//write the desired position to saved address;
      }
      
    }
    else if(!lumbar.on){
      for(int i = 0; i < 8; i++){
        digitalWrite(lumbarpins[i], LOW);
      }
    }
    
    Serial.println(observedpressure);
    //Serial.println(exitcounterbegin);
    
    //Serial.println(millis() - exitcounterstarttime);
  
  }
}