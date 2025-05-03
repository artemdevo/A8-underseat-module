#include "Arduino.h"
#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>


#define COMP 10
#define VENT 23
#define PRES A2
#define MAX_MASSAGE_STATE 4
#define CAN0_INT 41 


///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~DEFINES FOR LUMBAR~~~~~~~~~~~~~~~~~~~~~~~
#define HIGH_LUMBAR_BLD 18
#define MID_LUMBAR_BLD 22
#define LOW_LUMBAR_BLD 44
#define HIGH_LUMBAR_VNT 20
#define MID_LUMBAR_VNT 46
#define LOW_LUMBAR_VNT 42
/////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~DEFINES FOR MASSAGE~~~~~~~~~~~~~~~~~~~~`
#define UPPERMOST_LEFT_BLD 34
#define UPPERMOST_RIGHT_BLD 28
#define MID_UPPER_LEFT_BLD 36
#define MID_UPPER_RIGHT_BLD 26
#define MID_LEFT_BLD 30
#define MID_RIGHT_BLD 38
#define MID_LOWER_LEFT_BLD 40
#define MID_LOWER_RIGHT_BLD 19
#define LOWERMOST_LEFT_BLD 21
#define LOWERMOST_RIGHT_BLD 24
//////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct SavedLumbarValues {
  byte byte0;
  byte byte1;
  int pressure;
  byte position;
};

struct LumbarStruct {
  //byte position;//which bladder to choose. not using this?
  //int pressure;//how much pressure in the bladder. not using this?
  byte on; //on or off. identifies that it is in the lumbar mode
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

struct SavedMassageValues{
  byte mode;
  byte intensity;
};

struct MassageStruct{
  byte state;
  byte mode;
  byte on;
  byte intensity;
  byte firststate;
  byte previousmode[2];
  byte pinstosethigh[10];
  byte bladderpins[10] = {28, 34, 26, 36, 38, 30, 19, 40, 24, 21};//THESE ARE JUST THE BLADDERS
  unsigned long delaytime;
  unsigned long statestarttime = 1000;
  unsigned long lastmessagetimer;
};

int observedpressure;//variable to store pressure reads

long unsigned int rxId;//variables for the CAN read. just leaving them like they are in the example
byte len;
byte rxBuf[8];

//~~~~~~~~~~~~~~~~~~~~~~~~~~~ARRAYS TO ORGANIZE SPECIFIC PINS TO SET PINMODE AND OUTPUT LOW WHEN NOT IN  USE~~~~~~~~~~~~~~~~~
byte pins[22]={10, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};
//byte bladderpins[20] = {18, 19, 20, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};

byte lumbarpins[8] = {18, 22, 44, 20, 46, 42, 10, 23};//this includes 10, the compressor. remove it?

byte massagepins[11] = {19, 21, 24, 26, 28, 30, 34, 36, 38, 40, 10};//THIS INCLUDES THE COMPRESSOR


///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LumbarStruct lumbar;
SavedLumbarValues savedlumbarvalues;
BladderStruct bladder;
DPadStruct dpad;
MassageStruct massage;
SavedMassageValues savedmassagevalues;

MCP_CAN CAN0(53);   

void lumbarAdjustfunction();

void massagebladderpinsetfunction();



void setup() {
  
  for (int i = 0; i <22; i++){
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  } 

  Serial.begin(115200);
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
  pinMode(CAN0_INT, INPUT);

  //digitalWrite(VENT, HIGH);//these initialized vents help the reservoir pressure start lower when massaging, less strain on compressor?
  //delay(1000);
 //digitalWrite(VENT, LOW);
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~LUMBAR STUFF~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //for(int i = 0; i < 8; i++){
   // pinMode(lumbarpins[i], OUTPUT);
  //}

  savedlumbarvalues.byte0= EEPROM.read(0);//get the saved lumbar values from eeprom on startup
  savedlumbarvalues.byte1 = EEPROM.read(1);
  savedlumbarvalues.pressure = (savedlumbarvalues.byte1 << 8) | savedlumbarvalues.byte0;
  savedlumbarvalues.position = EEPROM.read(2);

  if(savedlumbarvalues.pressure < 0 || savedlumbarvalues.pressure > 730 || savedlumbarvalues.position > 2){//if the read pressure value is -1 (indicating a value of 0xffff in these two bytes, for EEPROM with no writes, set it to 0)
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
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MASSAGE STUFF~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*
  savedmassagevalues.mode = EEPROM.read(3);
  savedmassagevalues.intensity = EEPROM.read(4);
    if(savedmassagevalues.mode == 0 || savedmassagevalues.mode > 3 || savedmassagevalues.intensity == 0 || savedmassagevalues.intensity > 3){
      EEPROM.write(3, 1);//if the values are corrupted, set them both to one
      EEPROM.write(4, 1);
      savedmassagevalues.mode = 1;
      savedmassagevalues.intensity = 1;
    }
  Serial.println(savedmassagevalues.mode);
  Serial.println(savedmassagevalues.intensity);

  massage.mode = savedmassagevalues.mode;
  massage.intensity = savedmassagevalues.intensity;
  */
  //THIS IS ALL POINTLESS. SAVED VALUES NEED TO COME FROM UNDERSEAT MODULE BECAUSE IT NEEDS TO KNOW MASSAGE STATE TO MAKE THE VOICE MESSAGE
  //THE MASSAGE CAN MESSAGE WILL JUST STATE THE INTENSITY AND MODE AND THE PNEUMATICS MODULE PERFORMS THAT
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


}

void loop() {
  observedpressure = analogRead(PRES); //do the pressure read first (will be useful for the other functions and stuff)

  if(!digitalRead(CAN0_INT)){                         // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
  }

  if(rxId == 0x707 || rxId== 0x751){//these two CAN frames for lumbar and the side bolsters specifically indicate d-pad button presses 
    dpad.up = rxBuf[0];
    dpad.down = rxBuf[1];
    dpad.forward = rxBuf[2];
    dpad.backward = rxBuf[3];
    dpad.lastmessagetimer = millis();
  }
  else if(rxId == 0x650){//the massage CAN frame does not indicate d-pad button presses so the receive buffer values do different things
     
    massage.mode = rxBuf[0];//WILL HAVE WAVE, STRETCH, AND LUMBAR (SHORTER STRETCH) MASSAGE MODES
    massage.intensity = rxBuf[1];//1 to 3"HIGH" WILL BE 2700 MS DELAY. "MEDIUM" IS 2000 MS DELAY, "LOW" IS 1000 MS DELAY
    dpad.lastmessagetimer = millis();
    //massage.lastmessagetimer = millis();
  }

  
  if(!dpad.up && !dpad.down && !dpad.forward && !dpad.backward){//set the transition byte low if all of the values are low. transition gets set to 1 when button pressed
    dpad.transition = 0;
  }

  switch(rxId){//THIS IS A SWITCH CASE I AM SPECIFICALLY USING TO SET THE MAIN FUNCTIONS ON OR OFF
    case 0x707:{
      //lumbar.on is set by the other statements within its own function. dpad buttons must be pressed.
        massage.on = 0;
        massage.firststate = 1;//reset statement for the first time massage starts
        //bolster.on = 0;
      //}
    }
    break;
    case 0x650:{//0x650 (massage)
      massage.on = 1;//unlike the other cases, if messages are being received at all, that means massage is on 
      
      lumbar.on = 0;
      //bolster.on = 0;
    }
    break;
    case 0x751:{//if the newest received frame has an id of 0x751 (side bolster adjustment)
      massage.on = 0;//bolster.on will be turned on depending on the dpad button presses
      massage.firststate = 1;//resetting this to 1 for the next time i use massage
      lumbar.on = 0;
    }
    break;
  }
  
  if(millis()- dpad.lastmessagetimer > 500){//if more than 500 ms has passed since any usable message was received, turn everything off
    lumbar.on = 0;
    massage.on = 0;
    massage.firststate = 1;//reseting this to 1 so that the next time massage is turned on, we go through the first state time measurement
    Serial.println("CAN wait timeout");
    //bolster.on = 0;
    for(int i = 0; i < 4; i++){
      rxBuf[i] = 0;//flush the receive buffer too, so the states aren't preserved;
    }
  }

  //if((millis() - massage.lastmessagetimer > 500) && massage.on){//if massage is on and the last time a massage message was received was 500 ms ago, turn off massage.
  //  massage.on = 0; //this IF statement is so that if we remain in the massage state in the underseat module but stop sending messages, massage will turn off. massage already turns off with different messages being received
  //}

///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~old massage code is everything down below this in the loop()~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~`
  
  if(massage.on){
    ///////////////////////////////////////////checks if massage has just been started so that we can reset the state position and set the start time for the first state
    if(massage.firststate){
      massage.statestarttime = millis();//specifically using this so the state time start is done on the first pass when massage is started;
      massage.state = 0;
      massage.firststate = 0;//we will no longer be in the first state, in the second time we run through this, so turn it off
    }
    ///////////////////////////////////////////

    //massage.previousmode[1] = massage.previousmode[0];
    //massage.previousmode[0] = massage.mode;//these two statements are to save the previous value of massage mode to compare the current value to the previous one.

    //if(massage.previousmode[1] != massage.previousmode[0]){//if the previous mode does not equal the current mode;
     // massage.state = 0;//if we changed modes, make sure we go back to the first massage state so we aren't in a state too high for the current mode and sit there doing nothing
    //  Serial.println("went back to massage state 0!");
   // }
    
    switch(massage.mode){//this is for selecting which massage mode. 0 = wave. 1 = shoulder, 2 = lumbar 
      case 0:{
        switch(massage.state){//for setting pins in a particular massage state for massage mode 0(wave)
          case 0:{
            //digitalWrite(28, HIGH);
            massage.pinstosethigh[0] = UPPERMOST_RIGHT_BLD;
            //digitalWrite(34, HIGH);
            massage.pinstosethigh[1] = UPPERMOST_LEFT_BLD;
            massagebladderpinsetfunction();
            
          }
          break;
          case 1:{
            //digitalWrite(26, HIGH);
            //digitalWrite(36, HIGH);
            //digitalWrite(28, LOW);
            //digitalWrite(34, LOW); 
            massage.pinstosethigh[0] = MID_UPPER_LEFT_BLD;
            massage.pinstosethigh[1] = MID_UPPER_RIGHT_BLD;
            massagebladderpinsetfunction();
          }
          break;
          case 2:{
            //digitalWrite(38, HIGH);
            //digitalWrite(30, HIGH);
            //digitalWrite(26, LOW);
            //digitalWrite(36, LOW);
            massage.pinstosethigh[0] = MID_LEFT_BLD;
            massage.pinstosethigh[1] = MID_RIGHT_BLD;
            massagebladderpinsetfunction();
          }
          break;
          case 3:{
            //digitalWrite(19, HIGH);
            //digitalWrite(40, HIGH);
            //digitalWrite(38, LOW);
            //digitalWrite(30, LOW);
            massage.pinstosethigh[0] = MID_LOWER_LEFT_BLD;
            massage.pinstosethigh[1] = MID_LOWER_RIGHT_BLD;
            massagebladderpinsetfunction();
          }
          break;
          case 4:{
            //digitalWrite(24, HIGH);
            //digitalWrite(21, HIGH);
            //digitalWrite(19, LOW);
            //digitalWrite(40, LOW);
            massage.pinstosethigh[0] = LOWERMOST_LEFT_BLD;
            massage.pinstosethigh[1] = LOWERMOST_RIGHT_BLD;
            massagebladderpinsetfunction();
          }
          break;
        }
      }
      break;
      case 1:{ //for setting pins in a particular massage state for massage mode 1
        switch(massage.state){//for setting pins in a particular massage state for massage mode 1
          case 0:{
            massage.pinstosethigh[0] = UPPERMOST_RIGHT_BLD;
            massagebladderpinsetfunction();
          }
          break;
          case 1:{
            massage.pinstosethigh[0] = UPPERMOST_LEFT_BLD;
            massagebladderpinsetfunction();
          }
          break;
          case 2:{
            massage.pinstosethigh[0] = MID_UPPER_RIGHT_BLD;
            massagebladderpinsetfunction();
          }
          break;
          case 3:{
            massage.pinstosethigh[0] = MID_UPPER_LEFT_BLD;
            massagebladderpinsetfunction();
          }
          break;
        }
      } 
      break;
      case 2:{//for setting pins in a particular massage state for massage mode 2-LUMBAR 
        switch(massage.state){//for setting pins in a particular massage state for massage mode 2
          case 0:{
            massage.pinstosethigh[0] = MID_LOWER_LEFT_BLD;
            massage.pinstosethigh[1] = MID_LOWER_RIGHT_BLD;
            massagebladderpinsetfunction();
          }
          break;
          case 1:{
            massage.pinstosethigh[0] = MID_LEFT_BLD;
            massage.pinstosethigh[1] = MID_RIGHT_BLD;
            massagebladderpinsetfunction(); 
          }
          break;
          case 2:{
            massage.pinstosethigh[0] = MID_LOWER_RIGHT_BLD;
            massagebladderpinsetfunction(); 
          }
          break;
          case 3:{
            massage.pinstosethigh[0] = MID_LOWER_LEFT_BLD;
            massagebladderpinsetfunction(); 
          }
          break;
          case 4:{
            massage.pinstosethigh[0] = MID_RIGHT_BLD;
            massagebladderpinsetfunction(); 
          }
          break;
          case 5:{
            massage.pinstosethigh[0] = MID_LEFT_BLD;
            massagebladderpinsetfunction(); 
          }
          break;
        }
      }
      break;
    }
    ///////////////////////////////////////////////STATEMENTS TO CHANGE THE MASSAGE STATE DEPENDING ON THE MODE AND THE ELAPSED TIME IN THE CURRENT STATE///////////////////
    if(millis()-massage.statestarttime > ((unsigned long)massage.intensity*500 + 1100)){ //if the time has elapsed to change state, advance the state
      massage.state++;
      massage.statestarttime = millis();
    }

    if(massage.mode == 0 && massage.state > 4){//if we are in massage mode 0, being at state 5 or higher (if that somehow happens)                                                
        massage.state = 0;        //return to state 0
        massage.statestarttime = millis();  
      }
    else if(massage.mode == 1 && massage.state > 3){
      massage.state = 0; 
      massage.statestarttime = millis(); 
    }
    else if(massage.mode == 2 && massage.state > 5){//if we are in massagemode 2 and at state 6 or higher, such as when moving from another mode, return state to 0
        massage.state = 0;
        massage.statestarttime = millis();
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    digitalWrite(COMP, HIGH); //turning comp on all the time when massage is on;

    if(observedpressure<600){//during the massage, compressor should always be running but should not exceed a pressure of 800. MAYBE PUT THIS OUTSIDE THE MASSAGE FUNCTION? 
                              //CAN APPLY TO THE OTHER PNEUMATICS AS WELL
      digitalWrite(VENT, LOW);
    }
    else if(observedpressure>800){//add a statement to turn off slightly above 800 to introduce hysteresis
     
      digitalWrite(VENT, HIGH);  
    }


  }
  else{ //if massage is off, set all massage pins low. this is so that whatever was on from the last massage state is set back to 0
    for(int i= 0; i < 11; i++ ){
      digitalWrite(massagepins[i],LOW);
    }
  }
  Serial.println(observedpressure);
  //digitalWrite(VENT, HIGH);
  //delay(100);
  //digitalWrite(VENT, LOW);

}

///NEED TO WRITE A STATEMENT TO SET COMP LOW IF MASSAGE AND BOLSTERS AND LUMBAR ARE NOT ACTIVE




void lumbarAdjustfunction(){
  if(dpad.up && !dpad.transition && rxId == 0x707){
    if(lumbar.desiredposition < 2){
      lumbar.desiredposition++;
      lumbar.bladderchange = 1;
      dpad.transition = 1;
      lumbar.on = 1;
      
    }
    else if(lumbar.desiredposition == 2){//
      lumbar.on = 0;
    }
  
  }
  else if(dpad.down && !dpad.transition && rxId == 0x707){
    if(lumbar.desiredposition> 0){
      lumbar.desiredposition--;
      lumbar.bladderchange = 1;
      dpad.transition = 1;
      lumbar.on = 1;
    }
    else if(lumbar.desiredposition == 0){
      lumbar.on = 0;
    }
    
  }

  if((dpad.forward || dpad.backward) && rxId == 0x707){
    lumbar.bladderchange = 0;//set bladder change to 0 if pressure is commanded to change. this is an easy way to override a bladder change in progress
    lumbar.on = 1;
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
      digitalWrite(lumbarpins[i], LOW);//GOING TO HAVE TO REMOVE THE COMPRESSOR FROM THIS?
      
    }
  }
}

void massagebladderpinsetfunction(){
  for(int i = 0; i < 10; i++){ //the array will always have a maximum length of 10, there are 10 massage bladders
    if(massage.pinstosethigh[i] != 0){
      digitalWrite(massage.pinstosethigh[i], HIGH);//write the pin numbers in the array to high
    }
    else{
      break;
    }
  }
  for(int massagebladderpinIndex = 0; massagebladderpinIndex < 10; massagebladderpinIndex++){
    for(int highpinIndex = 0; highpinIndex < 10; highpinIndex++){
      if(massage.bladderpins[massagebladderpinIndex] == massage.pinstosethigh[highpinIndex]){//if the current massage bladder pin is equal to one that was previosly written high
        break;//get out of the INNER for loop because this bladder pin must be one of the ones we wrote high and go on to the next bladder pin 
      }
      if(massage.pinstosethigh[highpinIndex] == 0){//if we got to a point in the massage high pin for loop without breaking out in the previous statement
        digitalWrite(massage.bladderpins[massagebladderpinIndex], LOW);//the current massage bladder pin must not be one of the ones we wrote high, so write it low
        break;//and then break and go to the next massage pin to test and see if it was written high
      }
    }

  }
 for(int i = 0; i <10; i++){
  massage.pinstosethigh[i] = 0; //finally, reset the pinstosethigh array back to 0;
 }

}
