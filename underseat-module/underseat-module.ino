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
#define MAX_STATE 5 //this is the maximum state number, 1 less than the state count (state 0 is a state)
//#define FPSerial softSerial

byte voicestate = 0; //use this just for voice messages
byte truestate = 0;
byte messageplaysupress;
unsigned long canmessagetime = 0;


struct SavedMassageValues {
  byte mode;
  byte intensity;
};

struct LumbarStruct {
  byte on; //on or off
  byte desiredposition; //the value sent in the CAN message
  byte desiredpositionnew;
  byte positionup;
  byte positiondown;
  byte pressureup;
  byte pressuredown;
  byte suppressmessages;
  byte messagecounter;
  byte data[4];
  unsigned long messagetime;
  unsigned long desiredpressurenewtime;
};

struct MassageStruct{
  unsigned long starttime;
  byte on = 0;
  byte transition = 0;
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
  //byte ud_released;
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

BezelStruct bezelring;
DPadStruct dpad;
LumbarStruct lumbar;
SavedMassageValues savedmassagevalues;
MassageStruct massage;
BolsterStruct bolster;

SoftwareSerial softSerial(/*rx =*/6, /*tx =*/7);//ON THE UNO THIS NEEDS TO BE RX 6 AND TX 7
DFRobotDFPlayerMini myDFPlayer;

byte ignit[4] = {0x00, 0x00, 0xff, 0xff};
byte hvac[3]= {0x0, 0xC0, 0x0};


void seatmotorAdjustfunction();
void lumbarAdjustfunction();
void massageAdjustfunction();
void bolsterAdjustfunction();

MCP_CAN CAN0(10); //CS is pin 10 on arduino uno


void setup() {
  // put your setup code here, to run once:
  pinMode(2, OUTPUT);//do i need to write these low?
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);

  softSerial.begin(9600);//LITERALLY ONLY ALLOWS MP3 PLAYER TO WORK AT 9600. NOT FASTER NOT SLOWER
  Serial.begin(115200);
 
  myDFPlayer.begin(softSerial, /*isACK = */true, /*doReset = */true);//reset needs to be true for this shit to work on external power
  
  myDFPlayer.volume(15);  //Set volume value. From 0 to 30
  
  
  
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
  
  messageplaysupress = 1; //this is how i am avoiding a state 0 audio message from playing when the seat is turned on. if the user cycles back to state 0, the message will play
  
////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~MASSAGE STUFF~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  savedmassagevalues.mode = EEPROM.read(0);
  savedmassagevalues.intensity = EEPROM.read(1);
    if(savedmassagevalues.mode > 2 || savedmassagevalues.intensity > 2){
      EEPROM.put(0, 1);//if the values are corrupted, set them both to zero
      EEPROM.put(0, 1);
      savedmassagevalues.mode = 0;
      savedmassagevalues.intensity = 0;
      Serial.println("reset saved values!");
    }
  Serial.println(savedmassagevalues.mode);
  Serial.println(savedmassagevalues.intensity);

  massage.mode = savedmassagevalues.mode;
  massage.intensity = savedmassagevalues.intensity;
  
 
///~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
}

void loop() {
  bezelring.read = analogRead(BEZ_RNG);
  massage.btn_read = analogRead(BTN);
  dpad.fb_read = analogRead(D_PAD_FB);
  dpad.ud_read = analogRead(D_PAD_UD);

  bezelring.up = bezelring.read>470 && bezelring.read<800;
  bezelring.down = bezelring.read<470;
  bezelring.released = bezelring.read>900;

  massage.btnpressed = massage.btn_read<500;
  massage.btnreleased = massage.btn_read>700;

  dpad.forward = dpad.fb_read>470 && dpad.fb_read<800;
  dpad.back = dpad.fb_read<470;
  dpad.up = dpad.ud_read>470 && dpad.ud_read<800;
  dpad.down = dpad.ud_read<470;
  //dpad.ud_released = dpad.ud_read>900;
  

////---------------------------------------------------------------------------------------------------going up when press up on the bezel ring
  if(bezelring.up && !bezelring.transition){//if up on the bezel ring is pressed, increase state by 1 
    if(voicestate == 3){
      voicestate = 0; //if at maximum state, go back to 0
    }
    else{
    voicestate++; //play voice message for whatever state you just changed to, probably make it a switch case?
    }
    bezelring.transition = 1;
    messageplaysupress = 0;//set to 0 so that the voice message for the voice state will be played 
  }
  /////-----------------------------------------------------------------------------------------------------going down when pressing down on bezel ring
  else if(bezelring.down && !bezelring.transition){//if down on the bezel ring is pressed, decrease state by 1 
    if(voicestate == 0){
      voicestate = 3; //if at 0 state, loop back around to highest state
    }
    else{
    voicestate--; //play voice message for whatever state you just changed to, probably make it a switch case?
    }; 
    bezelring.transition = 1; //prep for the state transition
    //make sure transitionup is off
    messageplaysupress = 0;//set to 0 so that the voice message for the voice state will be played
  }

  else if(bezelring.released){
    truestate = voicestate;
    bezelring.transition = 0;
  }
  //////--------------------------------------------------------------------------------------------------------

  ///------------------------------------------dpad button transition release if statement. transition 
  if(!dpad.up && !dpad.down && !dpad.forward && !dpad.back){
    dpad.transition = 0;
  }
  /////-----------------------------------------------------------------------------------

  ////////------////////////////////////////////-massage button and massage function stuff. adjustments to the massage will be done in a state
  if(massage.btnpressed && !massage.transition){//going to need to write the specific states that lumbar and bolster adjustment are assigned to
    
    if(massage.on){//if button is pressed with massage on
      massage.on = 0;
      massage.transition = 1;
      //Serial.println("massage turned off");
    }
    else if(!massage.on){//start the massage when button pressed measure time start
      truestate = 1; //switch to massage state 
      voicestate = 1; //play voice message. GOING TO CHANGE THIS TO SAY "MASSAGE ON", NOT JUST THAT ITS IN THE MASSAGE STATE
      messageplaysupress = 0; //make sure sound plays
      massage.on = 1;
      massage.transition = 1;
      massage.starttime = millis();
      //Serial.println("massage turned on");

    }
  }
  if(massage.btnreleased){//if you let go of the massage button, return massagetransition to value 0
    massage.transition = 0;
    
  }
  
  
  massage.data[0] = massage.mode;
  massage.data[1] = massage.intensity;

  if(massage.on){//turn massage off if 10 minutes have elapsed
    if((millis()-massage.messagetime) > 200){//if more than 150 ms have elapsed, send CAN message

      //CAN0.sendMsgBuf(0x3C0, 0, 4, ignit); placeholder, need to figure out messageframe 
      //CAN0.sendMsgBuf(0x3C0, 0, 4, ignit);
      //CAN0.sendMsgBuf(0x664, 0, 3, hvac);
      CAN0.sendMsgBuf(0x650, 0, 2, massage.data);
      
      
      massage.messagetime = millis();
    }

    if((millis()-massage.starttime)>(1000L*60L*10L)){//60*10 seconds
    massage.on = 0;
    }
    
  }
  ////////////-------------------------------------------------END OF MASSAGE BUTTON AND FUNCTION STUFF---------------------------------

   ///////////////////////////////////////////////////////////////////////STATE 0-SEAT MOTOR CONTROLS////////////////////////////////////////////
  if(truestate == 0){
    seatmotorAdjustfunction();
  }
  else{//if the current state is not 2 (or whatever state), make sure that the motors cannot move 
    digitalWrite(2, LOW);
    digitalWrite(3, LOW);
    digitalWrite(4, LOW);
    digitalWrite(5, LOW);
  }
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   /////////////////state 1 for massage setting adjustment. the massage itself is turned on by the massage button statement above. the state is to change values and send CAN messages
  ////SHOULD THERE BE VOICE MESSAGES HERE AS I CHANGE THE INTENSITY/MODE???
  if(truestate == 1){
    massageAdjustfunction();
  }

  ///////////////////////////////////////////////////////////////////////////////////////-----------------------------------

  ///////////////////////////////////////////////////////state 2-lumbar
          ////NEED TO TURN OFF MASSAGE IN WHICHEVER STATE I END UP USING FOR THIS. SAME AS FOR BOLSTER ADJUSTMENT!!!!!!!!!!!!!!!!!!!!!!!!!
  if(truestate ==2){
    massage.on = 0;//must make sure massage is not on when in this state, no massage messages being transmitted
    //bolster.on = 0; NO NEED FOR THIS. IT JUST SENDS CAN MESSAGES BASED ON THE POSITION OF THE DPAD. SAME AS LUMBAR
    lumbarAdjustfunction();
  }
//////////////////////////////////////////////////////////////////////

///////////////////////////////////////////STATE 3 SIDE BOLSTER ADJUSTMENT//////////////////////////////////
  if(truestate == 3){
    massage.on = 0;
    bolsterAdjustfunction();
  }
  
//////////////////////////////////////////////////////////////////////////
 

  /////////------------------------------------switch case for playing messages when a state change happens 


  if(messageplaysupress==0){
    switch(voicestate){
      case 0:{
        //this is the basestate, so no voice message here on startup (would be annoying), but can activate massage from it
        myDFPlayer.play(1);
      }
      break;
      case 1:{
        //Serial.println(millis());
        myDFPlayer.play(2);
        //Serial.println(millis());
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
    messageplaysupress = 1;
  }
  /////----------------------------------------------------

 
  
}

void seatmotorAdjustfunction(){
  
  //if(truestate == 2){
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
    // -     -       -    -        -     -        -     -         -      -        -

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
  //}
  //else{//if the current state is not 2 (or whatever state), make sure that the motors cannot move 
   // digitalWrite(2, LOW);
    //digitalWrite(3, LOW);
    //digitalWrite(4, LOW);
    //digitalWrite(5, LOW);
  //}
}

void lumbarAdjustfunction(){
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

    lumbar.data[0] = lumbar.positionup;
    lumbar.data[1] = lumbar.positiondown;
    lumbar.data[2] = lumbar.pressureup;
    lumbar.data[3] = lumbar.pressuredown;

  if(millis()-lumbar.messagetime >100){//every 100 ms, send a message with the desired d pad values
    
    CAN0.sendMsgBuf(0x707, 0, 4, lumbar.data);
    lumbar.messagetime = millis();
  }
}

void massageAdjustfunction(){

  if(dpad.up && !dpad.transition){
    if(massage.intensity < 2){
      massage.intensity++;//only increment if the value is not already 2
      EEPROM.put(1, massage.intensity); //immediately save the new value to memory
      Serial.println("Intensity written to memory");
    }
    dpad.transition = 1;
  }
  else if(dpad.down && !dpad.transition){
    if(massage.intensity > 0){
      massage.intensity--;//only decrement if the value is not already 0
      EEPROM.put(1, massage.intensity);//immediately save the new value to memory
      Serial.println("Intensity written to memory");
    }
    dpad.transition = 1;
  }
  else if(dpad.forward && !dpad.transition){
    if(massage.mode == 2){
      massage.mode = 0;
    }
    else{
      massage.mode++;
    }
    EEPROM.put(0, massage.mode);
    Serial.println("Mode written to memory");
    dpad.transition = 1;
  }
  else if(dpad.back && !dpad.transition){
    if(massage.mode == 0){
      massage.mode = 2;
    }
    else{
      massage.mode--;
    }
    EEPROM.put(0, massage.mode);
    Serial.println("Mode written to memory");
    dpad.transition = 1;
  }
}

void bolsterAdjustfunction(){//very similar to the one for lumbar

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

  bolster.data[0] = bolster.cushionup;
  bolster.data[1] = bolster.cushiondown;
  bolster.data[2] = bolster.backrestup;
  bolster.data[3] = bolster.backrestdown;

  if(millis()-bolster.messagetime >100){//every 100 ms, send a message with the desired d pad values
    
    CAN0.sendMsgBuf(0x751, 0, 4, bolster.data);
    bolster.messagetime = millis();
  }

}







