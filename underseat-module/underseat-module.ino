#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define BTN A0
#define D_PAD_UD A1
#define D_PAD_FB A2
#define BEZ_RNG A3

struct SeatStates{
  byte state;
  byte statebuffer[2];
  byte state_message_play_supress;
};

struct SavedMassageValues {
  byte mode;
  byte intensity;
};

struct LumbarStruct {
  byte on; //on or off
  byte desiredposition; //the value sent in the CAN message
  byte positionup;
  byte positiondown;
  byte pressureup;
  byte pressuredown;
  byte data[4];
  unsigned long messagetime;
};

struct MassageStruct{
  unsigned long starttime;
  byte on;
  byte transition;//a transition byte specifically for the massage button
  byte btnpressed;
  byte btnreleased;
  int btn_read;
  byte mode;
  byte intensity;
  byte data[2];
  unsigned long messagetime;
};

struct BezelStruct{
  byte up;
  byte down;
  byte released;
  byte transition;
  int read; 
};

struct DPadStruct{
  byte up;
  byte down;
  byte forward;
  byte back;
  byte transition;
  int fb_read;
  int ud_read;
};

struct BolsterStruct{
  byte data[4];
  byte cushionup;
  byte cushiondown;
  byte backrestup;
  byte backrestdown;
  unsigned long messagetime;
};

SeatStates seat;
BezelStruct bezelring;
DPadStruct dpad;
LumbarStruct lumbar;
SavedMassageValues savedmassagevalues;
MassageStruct massage;
BolsterStruct bolster;

SoftwareSerial softSerial(/*rx =*/6, /*tx =*/7);//ON THE UNO THIS NEEDS TO BE RX 6 AND TX 7
DFRobotDFPlayerMini myDFPlayer;

byte ignit[4] = {0x00, 0x00, 0xff, 0xff};//these both are HVAC CAN messages. leaving these here even though they are unused
byte hvac[3]= {0x0, 0xC0, 0x0};

void seatmotorAdjustfunction();
void lumbarAdjustfunction();
void massageAdjustfunction();
void bolsterAdjustfunction();
void playStatemessages();

MCP_CAN CAN0(10); //CS for SPI is pin 10 on arduino uno. sets up the SPI interface with the the CAN module

void setup() {
  pinMode(2, OUTPUT);//initializing the 4 outputs needed to drive the 2 channel motor controller. default to low.
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);

  softSerial.begin(9600);//LITERALLY ONLY ALLOWS MP3 PLAYER TO WORK AT 9600. NOT FASTER NOT SLOWER. sets up serial channel with mp3 player
  Serial.begin(115200);//for the arduino serial console
 
  myDFPlayer.begin(softSerial, /*isACK = */true, /*doReset = */true);//mp3 player initializing. reset needs to be true for this shit to work on external power. will make a "pop"
  
  myDFPlayer.volume(27);  //Set volume value on mp3 player. From 0 to 30
  
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);//CAN initializing function. set to 500 KBPS because that is the speed the seat module CAN is running on (need one speed sent across the bus)
  CAN0.setMode(MCP_NORMAL);//not sure about this function but it is some other initializing thing and it is needed
  
  
//////////////////////////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MASSAGE STUFF INITIALIZING STUFF~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  savedmassagevalues.mode = EEPROM.read(0);//read the values from EEPROM
  savedmassagevalues.intensity = EEPROM.read(1);
    if(savedmassagevalues.mode > 2 || savedmassagevalues.intensity > 2){//if the values are outside the acceptable range (0 to 2)
      EEPROM.put(0, 0);//if the values are corrupted, set them both to zero
      EEPROM.put(1, 0);
      savedmassagevalues.mode = 0;//then also set the savedmassage values to zero because massage.mode and intensity use these. want to set massage mode and intensity to 0 if memory is corrupted
      savedmassagevalues.intensity = 0;
    }
  
  massage.mode = savedmassagevalues.mode;//set the massage values equal to the saved ones (or 0 if the values are bad)
  massage.intensity = savedmassagevalues.intensity;
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
}

void loop() {

////////////////////////////////////////////////READING THE ANALOG PINS AND SETTING VALUES BASED ON THE READS////////////////////////////
  bezelring.read = analogRead(BEZ_RNG);
  massage.btn_read = analogRead(BTN);
  dpad.fb_read = analogRead(D_PAD_FB);
  dpad.ud_read = analogRead(D_PAD_UD);

  bezelring.up = bezelring.read > 470 && bezelring.read < 800;
  bezelring.down = bezelring.read < 470;
  bezelring.released = bezelring.read > 900;

  massage.btnpressed = massage.btn_read < 500;
  massage.btnreleased = massage.btn_read > 700;

  dpad.forward = dpad.fb_read > 470 && dpad.fb_read < 800;
  dpad.back = dpad.fb_read < 470;
  dpad.up = dpad.ud_read > 470 && dpad.ud_read < 800;
  dpad.down = dpad.ud_read < 470;
////////////////////////////////////////////////////////////////END OF THE ANALOG READS/////////////////////////////////////////////////////////////
  
//////////////////////////////////////INCREASING THE STATE WHEN PRESSING UP ON THE BEZEL RING//////////////////////////////////////////////
  if(bezelring.up && !bezelring.transition){//if up on the bezel ring is pressed, increase state by 1 
    if(seat.state == 3){
      seat.state = 0; //if at maximum state, go back to 0
    }
    else{
    seat.state++; //increase seat state if not yet 3
    }
    bezelring.transition = 1;
    
  }
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~DECREASING THE STATE WHEN PRESSING DOWN ON THE BEZEL RING~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  else if(bezelring.down && !bezelring.transition){//if down on the bezel ring is pressed, decrease state by 1 
    if(seat.state == 0){
      seat.state = 3; //if at 0 state, loop back around to highest state
    }
    else{
    seat.state--; //decrease seat state if not yet 0
    }; 
    bezelring.transition = 1; //set transition bit high since the bezel ring is held down
  }
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  ////////////////////////////////////////////////WHEN BEZEL RING, DPAD, AND MASSAGE BUTTON ARE RELEASED//////////////////////////////////////////////////////////
  else if(bezelring.released){//finally, if the bezel ring is released to its original position, set transition to 0.
    bezelring.transition = 0;
  }

  if(!dpad.up && !dpad.down && !dpad.forward && !dpad.back){//if no button is being pressed on the dpad
    dpad.transition = 0;//set the transition byte back to 0, so that things can change when a d pad button press is detected again
  }
  if(massage.btnreleased){//if you let go of the massage button, return massagetransition to value 0
    massage.transition = 0;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  playStatemessages(); //PLAYING VOICE MESSAGES WHEN A STATE CHANGE HAPPENS WITH THE BEZEL RING

  //////////////////////////////MASSAGE BUTTON AND MASSAGE CANBUS MESSAGE. ADJUSTMENTS TO THE MASSAGE WILL BE DONE BY A FUNCTION WHEN IN STATE 2/////////////////////////
  if(massage.btnpressed && !massage.transition){
    if(massage.on){//if button is pressed with massage on
      massage.on = 0;//turn massage off
      massage.transition = 1;
      myDFPlayer.play(6);//"massage off"
    }
    else if(!massage.on){//if button is pressed with massage off
      seat.state = 1; //switch to massage state so that adjustments can be made with the d pad
      seat.state_message_play_supress = 1; //need to supress the voice message "massage adjustment, press the massage button to start", so the other one plays
      myDFPlayer.play(5);//"massage on"
      massage.on = 1;
      massage.transition = 1;
      massage.starttime = millis();//start counting the time since massage started. massage will stop after 10 minutes. (or whatever is chosen)
    }
  }
  
  massage.data[0] = massage.mode;//enter the massage values into a buffer to be sent over CAN. these values come from memory but are adjusted in the massage adjustment function
  massage.data[1] = massage.intensity;

  if(massage.on){//sending CAN messages once massage is set on when massage button is pressed. sending messages at all indicates that massage is on!
    if((millis()-massage.messagetime) > 200){//if more than 200 ms have elapsed, send CAN message
      CAN0.sendMsgBuf(0x650, 0, 2, massage.data);//send the massage values buffer. hex ID is 0x650 
      massage.messagetime = millis();
    }
    if((millis()-massage.starttime)>(900000)){//if the time that massage has been on exceeds 15 minutes (changed from 10 minutes)
      massage.on = 0;//turn massage off
      myDFPlayer.play(6);//"massage off"
    }
  }
  ////////////////////////////////////////////////END OF MASSAGE BUTTON AND CAN MESSAGE STUFF///////////////////////////////////////////////////////////////////////

   //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STATE 0-SEAT MOTOR CONTROLS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(seat.state == 0){
    seatmotorAdjustfunction();//if we are in state 0, the starting state. 
  }
  else{//if the current state is not 0 make sure that the motors cannot move by writing the 2 channel motor driver controller inputs to low
    digitalWrite(2, LOW);
    digitalWrite(3, LOW);
    digitalWrite(4, LOW);
    digitalWrite(5, LOW);
  }
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   /////////////////STATE 1 MASSAGE VALUE ADJUSMENT.THE MASSAGE ITSELF IS TURNED ON BY THE MASSAGE BUTTON AND CAN MESSAGE STATEMENT ABOVE////////////////////////////
    //////////////MASSAGE CAN ALSO OPERATE IN STATE 0 (BUT NOT 2 OR 3 BECAUSE THEY USE PNEUMATICS) BUT ADJUSTMENTS CAN ONLY BE DONE IN STATE 1//////////////////
  if(seat.state == 1){
    massageAdjustfunction();
  }
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~STATE 2. LUMBAR ADJUSTMENT~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(seat.state ==2){
    massage.on = 0;//must make sure massage is not on when in this state because we need the pneumatics. no massage messages must be transmitted.
    lumbarAdjustfunction();
  }
  ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

////////////////////////////////////////////////////////STATE 3 SIDE BOLSTER ADJUSTMENT//////////////////////////////////////////////////////////////
  if(seat.state == 3){
    massage.on = 0;//same as lumbar, must make sure the massage is not on when in this state because we need the pneumatics
    bolsterAdjustfunction();
  }
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 
}

//void bezelRingread(){
  //int bezel_measured_value = analogRead(BEZ_RING); original idea with this was the make a debounce function for the bezel ring but i don't think i need it

//}


void playStatemessages(){
  seat.statebuffer[1] = seat.statebuffer[0];
  seat.statebuffer[0] = seat.state;
  if(seat.statebuffer[1] != seat.statebuffer[0]){
    if(!seat.state_message_play_supress){
      switch(seat.state){
        case 0:{
          //this is the basestate, so no voice message here on startup (would be annoying), but can activate massage from it
          myDFPlayer.play(1);
        }
        break;
        case 1:{
          myDFPlayer.play(2);
        }
        break;
        case 2:{
          myDFPlayer.play(3);
        }
        break;
        case 3:{
          myDFPlayer.play(4);
        }
        break;
      }
    }
    else{
      seat.state_message_play_supress = 0;//set the voice message supress flag back to 0, it did its job
    }
  }
}

void seatmotorAdjustfunction(){//for adjusting the lower leg and upper back adjustments when in state 0
  if(dpad.up){//if up is pressed on the d pad
    digitalWrite(5, HIGH);//no idea if this is the right pin, but i will find out
    digitalWrite(4, LOW);
  }
  else if(dpad.down){//if down is pressed on the d pad
    digitalWrite(4, HIGH);
    digitalWrite(5, LOW);
  }
  else{//if neither of these are true, D_PAD_UD must be high, turn off the upper back motor
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
  }
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(dpad.forward){//if forward is pressed on the d pad
    digitalWrite(2, HIGH);//no idea if this is the right pin, but i will find out
    digitalWrite(3, LOW);
  }
  else if(dpad.back){//if back is pressed on the d pad
    digitalWrite(3, HIGH);
    digitalWrite(2, LOW);
  }
  else{//if neither of these are true, D_PAD_FB must be high, turn off the lower leg motor
  digitalWrite(2, LOW);
  digitalWrite(3, LOW);
  }
}

void lumbarAdjustfunction(){
  //////////////////////////////////////////SET THE PRESSURE AND POSITION CHANGE VALUES BASED ON THE DPAD BUTTON PRESSES////////////////////////
  if(dpad.up){//if up is pressed on the d pad
      lumbar.positionup = 1;//request to increase lumbar position
      lumbar.positiondown = 0;
  }
  else if(dpad.down){//if down is pressed on the d pad
    lumbar.positiondown = 1;//request to decrease lumbar position
    lumbar.positionup= 0;
  }
  else{//if neither of these are true, D_PAD_UD must be high,
    lumbar.positionup = 0;
    lumbar.positiondown = 0;
  }
  if(dpad.forward){//if forward is pressed on the d pad
    lumbar.pressureup = 1;
    lumbar.pressuredown = 0;
  }
  else if(dpad.back){//if back is pressed on the d pad
    lumbar.pressureup = 0;
    lumbar.pressuredown = 1;
  }
  else{
    lumbar.pressureup = 0;
    lumbar.pressuredown = 0;
  }
  /////////////////////////////////////////////END OF CHANGING LUMBAR POSITION AND PRESSURE VALUES//////////////////////////////////////////////////////////

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~SENDING LUMBAR CANBUS MESSAGE WITH POSITION AND PRESSURE CHANGE VALUES~~~~~~~~~~~~~~~~~~
    lumbar.data[0] = lumbar.positionup;
    lumbar.data[1] = lumbar.positiondown;
    lumbar.data[2] = lumbar.pressureup;
    lumbar.data[3] = lumbar.pressuredown;

  if(millis()-lumbar.messagetime >100){//every 100 ms, send a message with the desired d pad values
    
    CAN0.sendMsgBuf(0x707, 0, 4, lumbar.data);
    lumbar.messagetime = millis();
  }
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}

void massageAdjustfunction(){//entire function is about changing the massage values of mode (wave, shoulder, lumbar) and intensity (low, med, high)
  //////////////////////////////////////////////////////////////////UP IS PRESSED, INCREASE INTENSITY/////////////////////////////////////////////////
  if(dpad.up && !dpad.transition){//if up is pressed, increase the massage intensity value that is sent over CAN bus in the main loop (unless already at 2)
    if(massage.intensity < 2){
      massage.intensity++;//only increment if the value is not already 2
      EEPROM.put(1, massage.intensity); //immediately save the new value to memory
    }
    switch(massage.intensity){//then play a voice message of what the intensity value was changed to, even if we are already at the max
      case 0:{
        myDFPlayer.play(10);//"low intensity"
      }
      break;
      case 1:{
        myDFPlayer.play(11);//"medium intensity"
      }
      break;
      case 2:{
        myDFPlayer.play(12);//"high intensity"
      }
      break;
    }
    
    dpad.transition = 1;
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~DOWN IS PRESSED, DECREASE INTENSITY~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~`
  else if(dpad.down && !dpad.transition){//if down is pressed, decrease the massage intensity value that is sent over CAN bus in the main loop (unless already at 0)
    if(massage.intensity > 0){
      massage.intensity--;//only decrement if the value is not already 0
      EEPROM.put(1, massage.intensity);//immediately save the new value to memory so that we choose this value when massage is started after a power cycle
    }
    switch(massage.intensity){//then play a voice message of what the intensity values was changed to, even if we are already at the minimum
      case 0:{
        myDFPlayer.play(10);//"low intensity"
      }
      break;
      case 1:{
        myDFPlayer.play(11);//"medium intensity"
      }
      break;
      case 2:{
        myDFPlayer.play(12);//"high intensity"
      }
      break;
    }
    dpad.transition = 1;
  }
  ////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  //////////////////////////////////////////////////////////////FORWARD IS PRESSED, POSITIVELY CYCLE MODE///////////////////////////////////////////////////////
  else if(dpad.forward && !dpad.transition){// positively cycle the massage mode value to be sent in the CANbus message when forward is pressed on the dpad in state 1 and massage is on
    if(massage.mode > 1){//if the massage mode equals 2 
      massage.mode = 0;//loop around to the lowest value, 0
    }
    else{
      massage.mode++;
    }
    EEPROM.put(0, massage.mode);//immediately save value to memory so that this value is chosen when massage is started after a power cycle
    switch(massage.mode){//play a voice message of the massage mode that we just changed to 
        case 0:{
          myDFPlayer.play(7);//"wave program"
        }
        break;
        case 1:{
          myDFPlayer.play(8);//"shoulder program"
        }
        break;
        case 2:{
          myDFPlayer.play(9);//"lumbar program"
        }
        break;
      }
    dpad.transition = 1;
  }
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~BACK IS PRESSED, NEGATIVELY CYCLE MASSAGE MODE~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  else if(dpad.back && !dpad.transition){//negatively cycle the massage mode value to be sent in the CANbus message when backward is pressed on the dpad in state 1 and massage is on
    if(massage.mode < 1){//if the massage mode equals 0 
      massage.mode = 2;//loop around to the max value (2)
    }
    else{
      massage.mode--;
    }
    EEPROM.put(0, massage.mode);//immediately save value to memory so that this value is chosen when massage is started after a power cycle
    dpad.transition = 1;
    switch(massage.mode){//play a voice message of the massage mode that we just changed to
      case 0:{
        myDFPlayer.play(7);//"wave program"
      }
      break;
      case 1:{
        myDFPlayer.play(8);//"shoulder program"
      }
      break;
      case 2:{
        myDFPlayer.play(9);//"lumbar program"
      }
      break;
    }
  }
  ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}

void bolsterAdjustfunction(){//very similar to the function for lumbar
  ///////////////////////////////////////////SET CHANGE VALUES FOR ALL 4 BOLSTERS BASED ON D PAD BUTTON PRESSES TO BE SENT IN CANBUS MESSAGE//////////////////////////////////////
  if(dpad.up){//if up is pressed on the d pad
    bolster.cushionup = 1;//request to increase lumbar position
    bolster.cushiondown = 0;
  }
  else if(dpad.down){//if down is pressed on the d pad
    bolster.cushionup = 0;
    bolster.cushiondown = 1;//request to decrease lumbar position
  }
  else{//if neither of these are true, D_PAD_UD must be high,
    bolster.cushionup = 0;
    bolster.cushiondown = 0;
  }
  if(dpad.forward){//if forward is pressed on the d pad
    bolster.backrestup = 1;
    bolster.backrestdown = 0;
  }
  else if(dpad.back){//if back is pressed on the d pad
    bolster.backrestup = 0;
    bolster.backrestdown = 1;
  }
  else{
    bolster.backrestup = 0;
    bolster.backrestdown= 0;
  }
  ////////////////////////////////////////////////////END OF CHANGING BOLSTER VALUES TO BE SENT IN CANBUS///////////////////////////////////////////////////

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~WRITING BOLSTER CHANGE VALUES TO A BUFFER AND SENDING IN A MESSAGE OVER CANBUS~~~~~~~~~~~~~~~~~~~~~~~~~~~
  bolster.data[0] = bolster.cushionup;
  bolster.data[1] = bolster.cushiondown;
  bolster.data[2] = bolster.backrestup;
  bolster.data[3] = bolster.backrestdown;

  if(millis()-bolster.messagetime >100){//every 100 ms, send a message with the desired d pad values to pneumatics module
    CAN0.sendMsgBuf(0x751, 0, 4, bolster.data);//bolster.data is the buffer with the values, hex ID of the message is 0x751
    bolster.messagetime = millis();
  }
  ///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}
