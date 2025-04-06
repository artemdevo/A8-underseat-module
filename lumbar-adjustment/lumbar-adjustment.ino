
#include <EEPROM.h>

// start reading from the first byte (address 0) of the EEPROM
int address = 0;
byte value;

struct findingpressure {

  byte byte0;

  byte byte1;

  int value;
};


findingpressure pressure;

void setup() {
  // initialize serial and wait for port to open:
  Serial.begin(9600);
  //EEPROM.put(0, customVar);
  //EEPROM.write(0, 0xFF);
  //EEPROM.write(1, 0xFF);

  pressure.byte0= EEPROM.read(0);
  pressure.byte1 = EEPROM.read(1);
  pressure.value = (pressure.byte1 << 8) | pressure.byte0;

  if(pressure.value == -1){//if the read pressure value is -1 (indicating a value of 0xffff in these two bytes, for EEPROM with no writes, set it to 0)
                            //NOTE: this only works for atmega based MCUs that use 2 byte ints 
    for(int i = 0; i < 2; i++){
      EEPROM.write(i, 0);
    }
  }

}

void loop() {
  // read a byte from the current address of the EEPROM
  pressure.byte0= EEPROM.read(0);
  pressure.byte1 = EEPROM.read(1);
  pressure.value = (pressure.byte1 << 8) | pressure.byte0;

  Serial.println(pressure.value);

  /***
    Advance to the next address, when at the end restart at the beginning.

    Larger AVR processors have larger EEPROM sizes, E.g:
    - Arduino Duemilanove: 512 B EEPROM storage.
    - Arduino Uno:         1 kB EEPROM storage.
    - Arduino Mega:        4 kB EEPROM storage.

    Rather than hard-coding the length, you should use the pre-provided length function.
    This will make your code portable to all AVR processors.
  ***/
  //address = address + 1;
  //if (address == EEPROM.length()) {
    //address = 0;
  //}

  delay(500);
}