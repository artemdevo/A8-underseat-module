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
#define FPSerial softSerial

byte voicestate = 0; //use this just for voice messages
byte truestate = 0;
byte messageplaycount;
unsigned long canmessagetime = 0;


struct SavedLumbarValues {
  byte byte0;
  byte byte1;
  int pressure;
  byte position;
};

struct LumbarStruct {
  byte on; //on or off
  int desiredpressure; //the value sent in the CAN message
  int desiredpressurenew;
  byte desiredposition; //the value sent in the CAN message
  byte desiredpositionnew;
  byte suppressmessages;
  byte messagecounter;
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
  byte ud_released;
  int fb_read;
  int ud_read;
};

BezelStruct bezelring;
DPadStruct dpad;
LumbarStruct lumbar;
SavedLumbarValues savedlumbarvalues;
MassageStruct massage;

SoftwareSerial softSerial(/*rx =*/6, /*tx =*/7);//ON THE UNO THIS NEEDS TO BE RX 6 AND TX 7
DFRobotDFPlayerMini myDFPlayer;

unsigned long int time1 = 0; //for testing 3/28/2025- i think int will work? there will be overflow but I think it will turn out ok
unsigned long int time2;// for testing 3/28/2025

byte ignit[4] = {0x00, 0x00, 0xff, 0xff};
byte hvac[3]= {0x0, 0xC0, 0x0};
byte lumbardata[3];

void seatmotoradjust();

MCP_CAN CAN0(10); //CS is pin 10 on arduino uno




void setup() {
  // put your setup code here, to run once:
  pinMode(2, OUTPUT);//do i need to write these low?
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);

  FPSerial.begin(9600);
  Serial.begin(115200);
 
  myDFPlayer.begin(FPSerial, /*isACK = */true, /*doReset = */true);//reset needs to be true for this shit to work on external power
  
  myDFPlayer.volume(15);  //Set volume value. From 0 to 30
  
  
  
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
  
  messageplaycount = 1; //this is how i am avoiding a state 0 audio message from playing when the seat is turned on. if the user cycles back to state 0, the message will play
  
//--------------------stuff for lumbar saved value--------------------------------------------------------------------//
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
  lumbar.desiredposition = savedlumbarvalues.position;
  lumbar.desiredpressure = savedlumbarvalues.pressure;
  
//---------------------------------------------------------------------------------------------------------------------//
  
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
  dpad.ud_released = dpad.ud_read>900;
  

////---------------------------------------------------------------------------------------------------going up when press up on the bezel ring
  if(bezelring.up && !bezelring.transition){//if up on the bezel ring is pressed, increase state by 1 
    if(voicestate == MAX_STATE){
      voicestate = 0; //if at maximum state, go back to 0
    }
    else{
    voicestate++; //play voice message for whatever state you just changed to, probably make it a switch case?
    }
    bezelring.transition = 1;
    messageplaycount = 0;//set to 0 so that the voice message for the voice state will be played 
  }
  
  /////-----------------------------------------------------------------------------------------------------going down when pressing down on bezel ring
  else if(bezelring.down && !bezelring.transition){//if down on the bezel ring is pressed, decrease state by 1 
    if(voicestate == 0){
      voicestate = MAX_STATE; //if at 0 state, loop back around to highest state
    }
    else{
    voicestate--; //play voice message for whatever state you just changed to, probably make it a switch case?
    }; 
    bezelring.transition = 1; //prep for the state transition
    //make sure transitionup is off
    messageplaycount = 0;//set to 0 so that the voice message for the voice state will be played
  }


  else if(bezelring.released){
    truestate = voicestate;
    bezelring.transition = 0;
   
  }
  //////--------------------------------------------------------------------------------------------------------

  ////////-----------------------------------------------------------------------------massage button and massage function stuff. adjustments to the massage will be done in a state
  if(massage.btnpressed && !massage.transition){
    if(massage.on){//if button is pressed with massage on
      massage.on = 0;
      massage.transition = 1;
    }
    else if(!massage.on){//start the massage when button pressed measure time start
      massage.on = 1;
      massage.transition = 1;
      massage.starttime = millis();
    }
  }
  if(massage.on){//turn massage off if 10 minutes have elapsed
    if((millis()-canmessagetime) > 200){//if more than 150 ms have elapsed, send CAN message

      //CAN0.sendMsgBuf(0x3C0, 0, 4, ignit); placeholder, need to figure out messageframe 
      CAN0.sendMsgBuf(0x3C0, 0, 4, ignit);
      CAN0.sendMsgBuf(0x664, 0, 3, hvac);
      canmessagetime = millis();
    }
    if((millis()-massage.starttime)>(1000L*60L*10L)){//60*10 seconds
      massage.on = 0;
    }
  }

  if(massage.btnreleased && massage.transition){//if you let go of the massage button, return massagetransition to value 0
    massage.transition = 0;
  }
  ////////////----------------------------------------------------------------------------------

  ///////////////------------------------------------------------state 1-lumbar

  if(truestate ==1){
    lumbar.desiredpositionnew = lumbar.desiredposition; //to start, setting the "new" value equal to the existing one. check it at the end. this is needed for the first run of this loop
    lumbar.desiredpressurenew = lumbar.desiredpressure;
    if(dpad.up && !dpad.transition){//if up is pressed on the d pad
      if(lumbar.desiredpositionnew != 2){//if desired lumbar position is already 2, do nothing. do not wrap around
        lumbar.desiredpositionnew++;//otherwise, increase
      }
      dpad.transition = 1;
    }
    else if(dpad.down && !dpad.transition){//if down is pressed on the d pad
      if(lumbar.desiredpositionnew != 0){//if desired lumbar position is already 0, do nothing. do not wrap around
      lumbar.desiredpositionnew--;//otherwise, decrease
      }
      dpad.transition = 1;
    }
    else if(dpad.ud_released){//if neither of these are true, D_PAD_UD must be high,
      dpad.transition = 0;
    }

    

    if(dpad.forward){//if forward is pressed on the d pad
      if(millis()-lumbar.desiredpressurenewtime > 4){//stupid way to slow down how fast the desired pressure rises; CHECK THIS WITH SERIAL PRINT
        if(lumbar.desiredpressurenew < 800){//maximum desired pressure of 800
          lumbar.desiredpressurenew++;
        }
        lumbar.desiredpressurenewtime = millis();
      }
    }
    else if(dpad.back){//if back is pressed on the d pad

      if(millis() - lumbar.desiredpressurenewtime >4){//stupid way to slow down how fast the desired pressure rises; CHECK THIS WITH SERIAL PRINT
        if(lumbar.desiredpressurenew > 0){
          lumbar.desiredpressurenew--;
        }
        lumbar.desiredpressurenewtime = millis();
      }
    }
    
    if(lumbar.desiredpressurenew == lumbar.desiredpressure && lumbar.desiredpositionnew == lumbar.desiredposition){//if the desired values are the same as the previous loop
      if(!lumbar.suppressmessages){
        if(lumbar.messagecounter == 4){//if fewer than 4? lumbar CAN messages have been sent with these same values
          lumbar.suppressmessages = 1;// 4 separate CAN messages with the same values have now been sent over a 800 ms period. Don't send any more unless the values change
          EEPROM.put(0, lumbar.desiredpressure);//write the desired pressure to saved addresses;
          EEPROM.put(2, lumbar.desiredposition);//write the desired position to saved address;
          Serial.println("Lumbar values saved!~~~~~~~~~~~~~~~~");
        }
        else{
          if(millis()-lumbar.messagetime > 150){//if the previous message was sent at least 200 ms ago
            lumbardata[0] = (lumbar.desiredpressurenew >> 8) & 0xFF; //larger byte of pressure int
            lumbardata[1] = lumbar.desiredpressurenew & 0xFF;
            lumbardata[2] = lumbar.desiredpositionnew;
            CAN0.sendMsgBuf(0x707, 0, 3, lumbardata);//send CAN message here of desired lumbar pressure and position
            lumbar.messagecounter++;
            lumbar.messagetime = millis();
          }
        }
      }
    }
    else{ //either desired pressurenew or desiredpositionnew do not match the previous values
      lumbar.suppressmessages = 0; //unsuppress CAN messages if desired position or pressure are found to change  
      lumbar.messagecounter = 0; //restart the counter
      if(millis() - lumbar.messagetime > 150){
        lumbardata[0] = (lumbar.desiredpressurenew >> 8) & 0xFF; //larger byte of pressure int
        lumbardata[1] = lumbar.desiredpressurenew & 0xFF;
        lumbardata[2] = lumbar.desiredpositionnew;
        CAN0.sendMsgBuf(0x707, 0, 3, lumbardata);//send CAN message here of desired lumbar pressure and position
        lumbar.messagecounter = 1;
        lumbar.messagetime = millis();
      }

    }
    /*else if(!lumbar.suppressmessages){//either desiredpressurenew or desiredpositionnew or both do not match the previous, and suppressmessages is not on
      if(millis() - lumbar.messagetime > 200){
        //send CAN message here of new desired lumbar pressure and position
        lumbar.messagecounter = 1;
        lumbar.messagetime = millis();
      }
    }
    else if(lumbar.desiredpressurenew != lumbar.desiredpressure || lumbar.desiredpositionnew != lumbar.desiredposition){
      lumbar.suppressmessages = 0; //unsuppress CAN messages if desired position or pressure are found to change  
      lumbar.messagecounter = 0; 
    }
    */
    lumbar.desiredposition = lumbar.desiredpositionnew;//last step is to set the "old" value to the new value i changed;
    lumbar.desiredpressure = lumbar.desiredpressurenew;
    
  }
  else{
    lumbar.suppressmessages = 1;//this is done so that lumbar messages of the current value are not sent when switching to lumbar state;
    
  }



  ////////////////////---------------------------------------------------when in state 2? can control motors
  
  seatmotoradjust();
  
  //////////////////////////-----------------------------------------------

  /////////------------------------------------switch case for playing messages when a state change happens 


  if(messageplaycount==0){
    
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
      case 4:{
        myDFPlayer.play(5);
      }
      break;

    }
    messageplaycount++;
  }
  /////----------------------------------------------------

  ////-----------------------------------------for testing only, make a thing that prints the truestate and voice state once per second

  time2 = millis();

  if((time2-time1)>200)
  {
    
  //Serial.print(voicestate);
  //Serial.print(" ");
  //Serial.print(truestate);
  //Serial.print(" ");
  //Serial.print(bezelring.transition);
  //Serial.print(" ");
  //Serial.print(massage.on);
  //Serial.print(" ");
  Serial.print(lumbar.desiredpressure);
  Serial.print(" ");
  Serial.print(lumbar.desiredposition);
  Serial.print(" ");
  Serial.println(millis());

  time1 = time2; 
  }
  
  
  //*/
  //Serial.println(millis());
  //}

  ///////------------------------------------------------------

//delay(500);///for testing 
}

void seatmotoradjust(){
  if(truestate == 2){
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
  }
  else{//if the current state is not 2 (or whatever state), make sure that the motors cannot move 
    digitalWrite(2, LOW);
    digitalWrite(3, LOW);
    digitalWrite(4, LOW);
    digitalWrite(5, LOW);
  }
}
