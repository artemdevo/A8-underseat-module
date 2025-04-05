#include "Arduino.h"
#include <SPI.h>
#include <mcp_can.h>


#define COMP 10
#define VENT 23
#define PRES A2

byte pins[22]={10, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};
byte bladderpins[20] = {18, 19, 20, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 34, 36, 38, 40, 42, 44, 46};
byte lumbarpins[3] = {18, 22, 44};
byte lumbarvents[3] = {20, 46, 42};
MCP_CAN CAN0(53);     // Set CS to pin 10, change this to 53 for mega

void setup() {
  // put your setup code here, to run once:
  for (int i = 0; i <22; i++){
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  } 

  Serial.begin(9600);
  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
}

void loop() {
  // put your main code here, to run repeatedly:
  
  
  for(int i = 0; i < 3; i++){//changing this value to see what the different solenoids do
    digitalWrite(lumbarpins[i], HIGH);

    while(analogRead(PRES)<400){
      digitalWrite(COMP, HIGH);
    }
    digitalWrite(COMP, LOW);
    delay(5000);

    while(analogRead(PRES)>50){
      digitalWrite(VENT, HIGH);
    }
    digitalWrite(VENT, LOW);
    Serial.println(analogRead(PRES));
    delay(5000);
    digitalWrite(lumbarpins[i], LOW);
    delay(50);
  }
  
  
}
