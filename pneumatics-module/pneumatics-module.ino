#include <SPI.h>
#include <mcp_can.h>


#define COMP 10
#define VENT 23


void setup() {
  // put your setup code here, to run once:



  CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  CAN0.setMode(MCP_NORMAL);
}

void loop() {
  // put your main code here, to run repeatedly:

}
