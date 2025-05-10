#include "Arduino.h"
#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define COMP 10
#define VENT 23
#define PRES A2
#define CAN0_INT 41 //pin 41 is the receive interrupt pin for the CAN module

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

//////~~~~~~~~~~~~~~~~~~~~~~~~~~~DEFINES FOR BOLSTERS~~~~~~~~~~~~~~~~~
#define LEFT_CUSHION_BOLSTER 29
#define RIGHT_CUSHION_BOLSTER 31
#define LEFT_BACKREST_BOLSTER 25
#define RIGHT_BACKREST_BOLSTER 27
//////////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct SavedLumbarValues {
  byte byte0;
  byte byte1;
  int pressure;
  byte position;
};

struct LumbarStruct {
  byte on; //on or off. identifies that it is in the lumbar mode
  int desiredpressure; //the value received in the CAN message
  byte desiredposition; //the value received in the CAN message
  byte bladderchange;
  byte pressuresavetimerenable;//enable to start the timer to save and exit once dpad forward or backward is no longer being held in a lumbar bladder pressure change scenario. 
  unsigned long pressuresavestarttime;
  byte pins[6] = {18, 22, 44, 20, 46, 42};
};

struct BladderStruct{//stuff specific for a bladder change
  unsigned long exittimerstarttime;
  byte exittimerenable;//enable to start the timer to save and exit once within a certain pressure theshold while doing a lumbar bladder change scenario
};

struct DPadStruct{
  byte up;
  byte down;
  byte forward;
  byte backward;
  byte transition;
  unsigned long lastmessagetimer;
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
  unsigned long statestarttime; //this used to equal 1000. unsure why
};

struct BolsterStruct{
  byte on;
  byte pins[4] = {29, 31, 25, 27};
};

int observedpressure;//variable to store pressure reads

long unsigned int rxId;//3 variables for the CAN read. just leaving them like they are in the example
byte len;
byte rxBuf[8];

//~~~~~~~~~~~~~~~~~~~~~~~~~~~ARRAY TO ORGANIZE SPECIFIC PINS TO SET PINMODE AS OUTPUT~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
byte pins[22]={10, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LumbarStruct lumbar;
SavedLumbarValues savedlumbarvalues;
BladderStruct bladderchange;
DPadStruct dpad;
MassageStruct massage;
SavedMassageValues savedmassagevalues;
BolsterStruct bolster;

MCP_CAN CAN0(53);//initialize the CAN module, default SPI CS on the arduino mega is pin 53

void lumbarAdjustfunction();
void massagefunction();
void massagebladderpinsetfunction();
void bolsterfunction();
void setup() {
  
  for (int i = 0; i <22; i++){//increment through all of the pins (except the analog input and CAN receive interrupt pin) to set them as outputs
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  } 
  Serial.begin(115200);//baud rate for the arduino serial console used in debugging
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);//initializing the CAN module, 500 KBPS speed
  CAN0.setMode(MCP_NORMAL);//other function for initializing the CAN module
  pinMode(CAN0_INT, INPUT);//set the CAN receive interrupt pin as an input
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~LUMBAR STUFF~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  savedlumbarvalues.byte0= EEPROM.read(0);//get the saved lumbar values from eeprom on startup
  savedlumbarvalues.byte1 = EEPROM.read(1);
  savedlumbarvalues.pressure = (savedlumbarvalues.byte1 << 8) | savedlumbarvalues.byte0;//pressure is an int, so it requires bitwise operations to be formed from 2 bytes
  savedlumbarvalues.position = EEPROM.read(2);

  if(savedlumbarvalues.pressure < 0 || savedlumbarvalues.pressure > 730 || savedlumbarvalues.position > 2){//if the read pressure value is -1 (indicating a value of 0xffff in these two bytes, for EEPROM with no writes, set it to 0)
                            //NOTE: this only works for atmega based MCUs that use 2 byte ints. also using >730 value if it somehow becomes corrupted and is bigger than this
    for(int i = 0; i < 3; i++){//if saved values of pressure or position are impossible, write all 3 addresses to 0
      EEPROM.write(i, 0);
      savedlumbarvalues.pressure = 0;
      savedlumbarvalues.position = 0;
    }
  }
  lumbar.desiredpressure = savedlumbarvalues.pressure; //set a value for the desired lumbar pressure. this will be used in the lumbar function if a bladder change up or down is made
  lumbar.desiredposition = savedlumbarvalues.position;//set a value for the current lumbar position. this tells us which bladder (of the 3), is currently inflated, which is needed
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}
void loop() {

  observedpressure = analogRead(PRES); //do the pressure read first (will be useful for the other functions and stuff)

  //////////////////////////////////////////////////////CHECKING FOR AND READING CANBUS MESSAGES//////////////////////////////////////////////////////
  if(!digitalRead(CAN0_INT)){// If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxId, &len, rxBuf);// Read data: len = data length, buf = data byte(s)
  }
  else{
    rxId = 0x0;//set rxId to 0 since no message was received
    for(int i = 0; i < 4; i++){//at the very end of the loop, once we have done what we needed to with the receive buffer, get rid of it so that it doesn't persist next loop
      rxBuf[i] = 0x0;//flush the receive buffer too, so the states aren't preserved;
    }
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////ASSIGNING RECEIVE BUFFER TO VARIABLES AND TURNING ON FUNCTIONS BASED ON THE RECEIVED CANBUS MESSAGE ID (EXCEPT FOR LUMBAR)/////////////////////////////////
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
  }
  switch(rxId){//THIS IS A SWITCH CASE I AM SPECIFICALLY USING TO SET THE MAIN FUNCTIONS ON OR OFF
    case 0x707:{
      //lumbar.on is set by the other statements below. dpad buttons must be pressed.
      massage.on = 0;
      bolster.on = 0;

      /////////////////////////////CHANGING LUMBAR SPECIFIC CONTROLS BASED ON D PAD VALUE. THIS IS A PRECURSOR TO THE LUMBAR FUNCTION//////////////////////////////////////////
      if(dpad.up && !dpad.transition){//if the received message was for lumbar and d pad is pressed up when transition hasn't been set
        if(lumbar.desiredposition < 2){//if the desired position value is less than 2 (2 is the max bladder position)
          lumbar.desiredposition++;
          lumbar.bladderchange = 1;
          dpad.transition = 1;
          lumbar.on = 1;//turn lumbar function on knowing the current desired bladder position and that we are doing a bladder change
        }
      }
      else if(dpad.down && !dpad.transition){//if the received message was for lumbar and d pad is pressed down when transition hasn't been set
        if(lumbar.desiredposition> 0){
          lumbar.desiredposition--;
          lumbar.bladderchange = 1;
          dpad.transition = 1;
          lumbar.on = 1;//turn lumbar function on knowing the current desired bladder position and that we are doing a bladder change 
        }
      }
      if(dpad.forward || dpad.backward){//if forward or backwards are pressed/were pressed last on the dpad and the received message is for lumbar
        lumbar.bladderchange = 0;//set bladder change to 0 if forward or backward is held down. this is an easy way to override a bladder change in progress
        lumbar.on = 1;//turn lumbar on knowing we are using the existing value of desired bladder position but we are changing the pressure in the bladder
      }
      /////////////////////////////////////////////////////END OF LUMBAR SPECIFIC CONTROLS///////////////////////////////////////////
    }
    break;
    case 0x650:{//0x650 (massage)
      massage.on = 1;//unlike the other cases, if messages are being received at all, that means massage is on 
      lumbar.on = 0;
      bolster.on = 0;
    }
    break;
    case 0x751:{//if the newest received frame has an id of 0x751 (side bolster adjustment)
      massage.on = 0;
      lumbar.on = 0;
      bolster.on = 1;
    }
    break;
  }
  ///////////////////////////////END OF ASSIGNING VALUES AND TURNING ON FUNCTIONS BASED ON THE RECEIVED CANBUS MESSAGE ID (EXCEPT FOR LUMBAR)//////////////////////////////////////

 //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STATEMENT TO SET THE VIRTUAL D PAD TRANSITION (MEASURED FROM CANBUS MESSAGES) BACK TO 0~~~~~~~~~~~~~~~~~~~~~~~~
  if(!dpad.up && !dpad.down && !dpad.forward && !dpad.backward){//set the transition byte low if all of the values are low. transition gets set to 1 when button pressed
    dpad.transition = 0;
  }
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STATEMENT TO TURN EVERYTHING OFF IF NO USABLE CANBUS MESSAGE IS RECEIVED AFER 500 MILLISECONDS~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(millis()- dpad.lastmessagetimer > 500){//if more than 500 ms has passed since any usable message was received, turn everything off
    lumbar.on = 0;
    massage.on = 0;
    massage.firststate = 1;//reseting this to 1 so that the next time massage is turned on, we go through the first state time measurement
    bolster.on = 0;
  }
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  ////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~PNEUMATICS FUNCTIONS AND PIN ASSIGNMENTS WHEN NOT IN USE~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(massage.on){
    massagefunction();
  }
  else{ //if massage is off, set all massage pins low. this is so that whatever was on from the last massage state is set back to 0
    for(int i= 0; i < 10; i++ ){
      digitalWrite(massage.bladderpins[i],LOW);
    }
    massage.firststate = 1; //if massage is off, the next time it turns on will be starting at the first state
  }
  if(lumbar.on){//if a new CAN message for lumbar is received, lumbar.on will be set to 1. otherwise, it is 0
  lumbarAdjustfunction();
  }
  else{
    lumbar.pressuresavetimerenable = 0;//reset both of these lumbar related timer enables in case they were set when lumbar was suddenly switched off
    bladderchange.exittimerenable = 0;
    for(int i = 0; i < 6; i++){
      digitalWrite(lumbar.pins[i], LOW);
    }
  }
  if(bolster.on){
   bolsterfunction();
  }
  else{
    for(int i = 0; i < 4; i++){
      digitalWrite(bolster.pins[i], LOW);
    } 
  }
  if(!massage.on && !lumbar.on && !bolster.on){//if none are on, make sure that these two are written low
    digitalWrite(COMP, LOW);
  }
  if(!massage.on && !bolster.on){//lumbar has no provisions to switch the normal vent, so make it only dependent on massage and bolster
    digitalWrite(VENT, LOW);
  }
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~END OF PNEUMATICS FUNCTIONS AND PIN ASSIGNMENTS WHEN NOT IN USE~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(observedpressure > 950){// I am worried about the possibility of the compressor getting stuck on. this addresses that. note that vent low must be written by something else
    digitalWrite(VENT, HIGH);
  }

}


void lumbarAdjustfunction(){
  switch(lumbar.desiredposition){//firstly, set the correct bladders to be vented using their specific vent and the correct one to be switched on based on the desired position
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
  ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~CODE FOR A BLADDER CHANGE. THIS IS WHEN THE DESIRED POSITION CHANGES FROM WHAT IT PREVIOUSLY WAS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(lumbar.bladderchange){
    lumbar.pressuresavetimerenable = 0; //reset the pressure change state timer in case it was set when we suddenly started doing a bladder change
    /////////////////////////////////////////////////////////IF THE PRESSURE IN THE BLADDER WE ARE CHANGING TO IS TOO HIGH/////////////////////////////////////
    if(observedpressure - lumbar.desiredpressure >= 50){ //turn the specific bladder vent on if the pressure is more than 30 higher than desired. is this even possible to occur?
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
    ////////////////////////////////////////////////////END OF IF THE PRESSURE IN THE BLADDER WE ARE CHANGING TO IS TOO HIGH///////////////////////////////////

    //////////////////////////////////////IF THE PRESSURE IN THE BLADDER WE ARE CHANGING TO IS TOO LOW (THIS IS THE NORMAL CASE)/////////////////////////
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
    }
    /////////////////////////////////////END OF IF THE PRESSURE IN THE BLADDER WE ARE CHANGING TO IS TOO LOW////////////////////////////////////////////////

    if(observedpressure - lumbar.desiredpressure >= 20){//if the reservoir pressure is more than 20 over desired in the bladder, turn comp off. changed this from 0                                          
      digitalWrite(COMP, LOW);
    }
    else if(observedpressure - lumbar.desiredpressure <= -50){//if the reservoir pressure is more than 50 less than desired in the bladder, turn comp on. changed from 30                            
      digitalWrite(COMP, HIGH);
    }
    if(abs(observedpressure - lumbar.desiredpressure) < 50){//if within 50 of the target pressure in the bladder. changed from 30
      if(!bladderchange.exittimerenable){//if this is the first time through this if statement since this pressure condition has been true
        bladderchange.exittimerstarttime = millis(); //if the reservoir is at the desired pressure, that means the lumbar bladder must be close as well, so start a counter
        bladderchange.exittimerenable = 1;//so we don't refresh exitcountertime
      }
    }
    else{
      bladderchange.exittimerenable = 0;//set exittimerenable off if not within the target pressure
    }
    if(bladderchange.exittimerenable){//if exittimerenable was set by the condition of being close to the target pressure in the desired bladder
      if (millis()-bladderchange.exittimerstarttime > 2000){//if current time minus the start of the exit counter exceeds 2000 ms. needs to be 2000 to give time for inactive bladders to vent
        bladderchange.exittimerenable = 0;//reset exittimerenable
        lumbar.on = 0; //turn off lumbar, the goal was reached
        EEPROM.put(2, lumbar.desiredposition);//write the new bladder position to saved address;
      }
    }
  }
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~END OF THE BLADDER CHANGE CODE~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  /////~~~~~~~~~~~~~~~~~~~~~~~~~~~CODE FOR CHANGING THE PRESSURE IN THE EXISTING DESIRED BLADDER~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(!lumbar.bladderchange){//if we aren't doing a bladder change and lumbar is on, it must be a pressure change in the same bladder
    bladderchange.exittimerenable = 0; //reset the bladder change exit timer enable value, in case it was already set when a sudden pressure change interrupted it and it never set back to 0
    if(observedpressure > 730 || !dpad.forward){
      digitalWrite(COMP, LOW);//turn compressor off if measured pressure exceeds 730 or if we are pressing backward on the dpad; using this lower value because it takes forever to get there
    }
    else if(dpad.forward && observedpressure < 700){//if the forward button is being pressed and the pressure in the bladder is less than 700
      digitalWrite(COMP, HIGH);//turn the compressor on to inflate the desired bladder and raise the pressure
    }
    if(dpad.backward){//if backwards is pressed, switch the specific vent solenoid for the desired bladder on to drop its pressure, otherwise switch it off

      ///////////////////////////////////////////CODE FOR SWITCHING THE DESIRED BLADDER VENT SOLENOID ON OR OFF////////////////////////////////////
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
      switch(lumbar.desiredposition){//if there is no longer a command to drop pressure (dpad backward being pressed), turn the solenoid off
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
    ////////////////////////////////////////////END OF CODE FOR SWITCHING THE SPECIFIC DESIRED VENT SOLENOID ON OR OFF//////////////////////////////

    if(dpad.forward || dpad.backward){//if either forward or backward are being actively pressed on the d pad, not just the initial press to get us into a pressure change state
      lumbar.pressuresavestarttime = millis();//reset the save timer to the current time as long as pressure up or down (dpad.forward/dpad.backward)are being held on. 
      lumbar.pressuresavetimerenable = 1; //set the enable for the pressuresavetimer. this will remain on after forward or backward are no longer held down
    }
    if(millis() - lumbar.pressuresavestarttime > 300 && lumbar.pressuresavetimerenable){//if 300 ms pass without the save timer being "reset" from the dpad being held down
      EEPROM.put(0, observedpressure);//EEPROM.put understands that observed pressure is a 2 byte value, and it splits the 2 byte int up and writes to both address 0 and 1
      lumbar.desiredpressure = observedpressure;//the desired pressure is set to the observed one, if more lumbar changes happen, is now equal to the observed bladder pressure
      lumbar.pressuresavetimerenable = 0;//turn off the pressure change save timer enable
      lumbar.on = 0;//the goal was reached, turn off lumbar
    }
  }
  //////~~~~~~~~~~~~~~~~~~~~~~~~~END OF THE CODE FOR CHANGING THE PRESSURE IN THE EXISTING DESIRED BLADDER~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}
  
void massagefunction(){
  ///////////////////////////////////////////checks if massage has just been started so that we can reset the state position and set the start time for the first state
  if(massage.firststate){
    massage.statestarttime = millis();//specifically using this so the state time start is done on the first pass when massage is started;
    massage.state = 0;
    massage.firststate = 0;//we will no longer be in the first state, in the second time we run through this, so turn it off
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~CODE FOR SETTING PINS HIGH OR LOW BASED ON THE MASSAGE MODE AND STATE~~~~~~~~~~~~~~~~~~~~~~~~~~
  switch(massage.mode){//this is for selecting which massage mode. 0 = wave. 1 = shoulder, 2 = lumbar 
    case 0:{
      switch(massage.state){//for setting pins in a particular massage state for massage mode 0(wave)
        case 0:{
          massage.pinstosethigh[0] = UPPERMOST_RIGHT_BLD;
          massage.pinstosethigh[1] = UPPERMOST_LEFT_BLD;
          massagebladderpinsetfunction();
        }
        break;
        case 1:{
          massage.pinstosethigh[0] = MID_UPPER_LEFT_BLD;
          massage.pinstosethigh[1] = MID_UPPER_RIGHT_BLD;
          massagebladderpinsetfunction();
        }
        break;
        case 2:{
          massage.pinstosethigh[0] = MID_LEFT_BLD;
          massage.pinstosethigh[1] = MID_RIGHT_BLD;
          massagebladderpinsetfunction();
        }
        break;
        case 3:{
          massage.pinstosethigh[0] = MID_LOWER_LEFT_BLD;
          massage.pinstosethigh[1] = MID_LOWER_RIGHT_BLD;
          massagebladderpinsetfunction();
        }
        break;
        case 4:{
          massage.pinstosethigh[0] = LOWERMOST_LEFT_BLD;
          massage.pinstosethigh[1] = LOWERMOST_RIGHT_BLD;
          massagebladderpinsetfunction();
        }
        break;
      }
    }
    break;
    case 1:{ //for setting pins in a particular massage state for massage mode 1-shoulder
      switch(massage.state){//for setting pins in a particular massage state for massage mode 1
        case 0:{
          massage.pinstosethigh[0] = UPPERMOST_RIGHT_BLD;
          massage.pinstosethigh[1] = MID_UPPER_RIGHT_BLD;
          massagebladderpinsetfunction();
        }
        break;
        case 1:{
          massage.pinstosethigh[0] = UPPERMOST_LEFT_BLD;
          massage.pinstosethigh[1] = MID_UPPER_LEFT_BLD;
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
          massage.pinstosethigh[0] = MID_LOWER_LEFT_BLD;
          massage.pinstosethigh[1] = MID_LOWER_RIGHT_BLD;
          massagebladderpinsetfunction();
        }
        break;
        case 3:{
          massage.pinstosethigh[0] = LOWERMOST_LEFT_BLD;
          massage.pinstosethigh[1] = LOWERMOST_RIGHT_BLD;
          massagebladderpinsetfunction();
        }
        break;
                      
      }
    }   
    break;
  }
  ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~END OF CODE FOR SETTING PINS BASED ON MASSAGE MODE AND STATE~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  ///////////////////////////////////////////////STATEMENTS TO CHANGE THE MASSAGE STATE DEPENDING ON THE MODE AND THE ELAPSED TIME IN THE CURRENT STATE///////////////////
  if(millis()-massage.statestarttime > ((unsigned long)massage.intensity*500 + 1100)){ //if the time has elapsed to change state, advance the state
    massage.state++;
    massage.statestarttime = millis();//set the current time as the new state start time
  }

  if(massage.mode == 0 && massage.state > 4){//if we are in massage mode 0, being at state 5 or higher (if that somehow happens)                                                
      massage.state = 0; //return to state 0
      massage.statestarttime = millis();  
    }
  else if(massage.mode == 1 && massage.state > 1){//if we are in massage mode 1 being at state 2 or higher, such as when moving from another mode
    massage.state = 0; //return to 0
    massage.statestarttime = millis(); 
  }
  else if(massage.mode == 2 && massage.state > 3){//if we are in massagemode 2 and at state 4 or higher, such as when moving from another mode, return state to 0
      massage.state = 0;
      massage.statestarttime = millis();
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  digitalWrite(COMP, HIGH); //turning comp on all the time when massage is on;

  if(observedpressure<600){//turn the vent on if pressure goes above 800 and close it again if pressure drops below 600
    digitalWrite(VENT, LOW);//these two VENT pin writes are to prevent pressure from rising too high during the massage function
  }
  else if(observedpressure>800){//
    digitalWrite(VENT, HIGH);  
  }
}

void massagebladderpinsetfunction(){//this function was created to simplify the process of writing massage pins high or low depending on state and mode
  for(int i = 0; i < 10; i++){ //the array will always have a maximum length of 10, there are 10 massage bladders
    if(massage.pinstosethigh[i] != 0){
      digitalWrite(massage.pinstosethigh[i], HIGH);//write the pin numbers in the array "pinstosethigh" to high
    }
    else{//if a value is no longer != 0, that must mean it equals 0, and we are at the end of the array, so break out of this for loop and move on
      break;//I GUESS IT WOULD BE A PROBLEM IF ALL 10 PINS ARE TO BE SET HIGH, BECAUSE THERE WOULD BE NO 0 ELEMENT IN THE ARRAY TO BREAK OUT FOR. (WONT EVER HAPPEN)
    }
  }
  for(int massagebladderpinIndex = 0; massagebladderpinIndex < 10; massagebladderpinIndex++){
    for(int highpinIndex = 0; highpinIndex < 10; highpinIndex++){
      if(massage.bladderpins[massagebladderpinIndex] == massage.pinstosethigh[highpinIndex]){//if the current massage bladder pin is equal to one that was previosly written high
        break;//get out of the INNER for loop and move on because this bladder pin must be one of the ones we wrote high and go on to checking the next massage bladder pin 
      }
      if(massage.pinstosethigh[highpinIndex] == 0){//if we got to a point in the massage high pin for loop without breaking out in the previous statement
        digitalWrite(massage.bladderpins[massagebladderpinIndex], LOW);//the current massage bladder pin must not be one of the ones we wrote high, so write it low
        break;//and then break and go to the next massage pin to test and see if it was written high
      }///AGAIN I KNOW IT WOULD BE A PROBLEM IF ALL 10 ELEMENTS ARE SET TO SOMETHING OTHER THAN 0 IN PINSTOSETHIGH
    }
  }
 for(int i = 0; i <10; i++){
  massage.pinstosethigh[i] = 0; //finally, reset the pinstosethigh array back to 0;
 }
}

void bolsterfunction(){
  if(dpad.up){//this means cushionup
    digitalWrite(LEFT_CUSHION_BOLSTER, HIGH);
    digitalWrite(RIGHT_CUSHION_BOLSTER, HIGH);
    digitalWrite(LEFT_BACKREST_BOLSTER, LOW);
    digitalWrite(RIGHT_BACKREST_BOLSTER, LOW);
    digitalWrite(VENT, LOW);

    if(observedpressure < 700){//if we are below 700 pressure, write compressor high
      digitalWrite(COMP, HIGH);
      
    }
    else if(observedpressure > 730){//if we go over 730, write the compressor low
      digitalWrite(COMP, LOW);
    }   
  }
  else if(dpad.down){//this means cushiondown. write stuff to drop the cushion bolster pressure
    digitalWrite(LEFT_CUSHION_BOLSTER, HIGH);
    digitalWrite(RIGHT_CUSHION_BOLSTER, HIGH);
    digitalWrite(LEFT_BACKREST_BOLSTER, LOW);
    digitalWrite(RIGHT_BACKREST_BOLSTER, LOW);
    digitalWrite(COMP, LOW);
    digitalWrite(VENT, HIGH);
  }
  if(dpad.forward){//this means backrestup  
    digitalWrite(LEFT_CUSHION_BOLSTER, LOW);
    digitalWrite(RIGHT_CUSHION_BOLSTER, LOW);
    digitalWrite(LEFT_BACKREST_BOLSTER, HIGH);
    digitalWrite(RIGHT_BACKREST_BOLSTER, HIGH);
    digitalWrite(VENT, LOW);

    if(observedpressure < 700){//write compressor to increase pressure in the backrest bolsters assuming the pressure is below 700
      digitalWrite(COMP, HIGH);
      
    }
    else if(observedpressure > 730){//if the pressure goes above 730, write the compressor low, hit maximum pressure
      digitalWrite(COMP, LOW);
    }
  }
  else if(dpad.backward){//this means backrestdown
    digitalWrite(LEFT_CUSHION_BOLSTER, LOW);//write stuff to reduce pressure in the backrest bolsters
    digitalWrite(RIGHT_CUSHION_BOLSTER, LOW);
    digitalWrite(LEFT_BACKREST_BOLSTER, HIGH);
    digitalWrite(RIGHT_BACKREST_BOLSTER, HIGH);
    digitalWrite(COMP, LOW);
    digitalWrite(VENT, HIGH);
  }
  if(!dpad.up && !dpad.down && !dpad.forward && !dpad.backward){
    digitalWrite(LEFT_CUSHION_BOLSTER, LOW);//set everything low if no button is pressed
    digitalWrite(RIGHT_CUSHION_BOLSTER, LOW);
    digitalWrite(LEFT_BACKREST_BOLSTER, LOW);
    digitalWrite(RIGHT_BACKREST_BOLSTER, LOW);
    digitalWrite(COMP, LOW);
    digitalWrite(VENT, LOW);
  }
}
