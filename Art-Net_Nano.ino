//
//    FILE: Art-Net_Nano.ino
//  AUTHOR: jco <jco0jco0jco@gmail.com>
// VERSION: 0.1.00
//    DATE: 2018/09/18
// LICENSE: see LICENSE
//
// Receives and processes Art-Net packets
//
#include <EtherCard.h>
#include <Adafruit_NeoPixel.h>

//NeoPixel Config
#define PIN            6 // sets the Arduino Nano pin for NeoPixel data
#define NUMPIXELS      5 // sets the amount of NeoPixel in a string
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

//EtherCard Config
#define STATIC 1 // set for static IP configuration
uint8_t Ethernet::buffer[700];
static uint8_t mymac[] = {0xFA, 0xFA, 0xFA, 0x01, 0x01, 0x01}; // used for calculating IP in 2.0.0.0/8, mymac[5] unique for each node -> calculates IP in 2.0.1.0/24
uint16_t dstPort = 6454; // standard Art-Net port 
uint16_t srcPort = 6454;
uint8_t myip[4];
uint8_t gwip[4];
uint8_t net[4];

//Art-Net Config
uint16_t oem = 0x00FF;
uint8_t NetSwitch = 0x00; // set the Net part of the port address (0x00-0x7F)
uint8_t SubSwitch = 0x00; // sets the Sub-Net part of the port address (0x00-0x0F)
uint16_t dmxChannel = mymac[5] * 3 * NUMPIXELS; // sets the first DMX channel this node uses (0-511), max (170 / NUMPIXELS) nodes

//Misc Config
#define DEBUG 0 // set for debug information via serial monitor, Baud rate 115200


#define high_byte(x) (x >> 8) & 0xFF
#define low_byte(x) x & 0xFF
const char ArtNetHeader[8] = "Art-Net\0"; // Art-Net header for identifying Art-Net packets

void setup (){
#if DEBUG
  Serial.begin(115200); //initialises serial monitor
#endif
  
  pixels.begin(); // initialises NeoPixels
  for(uint8_t i = 0;i < NUMPIXELS;i++){ // flashes NeoPixel blue on boot
    pixels.setPixelColor(i, 0, 0, 255); //
    pixels.show();                      //
  }                                     //
  delay(500);                           //
  for(uint8_t i = 0;i < NUMPIXELS;i++){ //
    pixels.setPixelColor(i, 0, 0, 0);   //
    pixels.show();                      //
  }                                     //
  
  if(!ether.begin(sizeof Ethernet::buffer, mymac)){ // initialises ethernet controller
#if DEBUG
    Serial.println(F("Failed to access Ethernet controller!"));
#endif
  }
#if STATIC
  myip[0] = 2;              // calculates default static IP address as per Art-Net protocol
  myip[1] = mymac[3] + oem; // 
  myip[2] = mymac[4];       // 
  myip[3] = mymac[5];       // 
  gwip[0] = 2; // calculates default gateway address as per Art-Net protocol
  gwip[1] = 0; //
  gwip[2] = 0; //
  gwip[3] = 1; //
  net[0] = 255; // calculates default subnet mask as per Art-Net protocol
  net[1] = 0;   //
  net[2] = 0;   //
  net[3] = 0;   //
  ether.staticSetup(myip, gwip, 0, net); // intialises ethernet controller for static IP
#if DEBUG
  Serial.print(F("Setting IP statically."));
#endif
#else
 #if DEBUG
  Serial.print(F("Setting IP via DHCP."));
 #endif
  if(!ether.dhcpSetup()){ // intialises ethernet controller for dynamic IP
 #if DEBUG
    Serial.println(F("DHCP failed!"));
 #endif
  }
 #endif
  ether.enableBroadcast(); // enables braodcasting UDP packets
  ether.udpServerListenOnPort(&processArtNetPacket, dstPort);
 #if DEBUG
  Serial.println();
  Serial.print(F("MAC       : "));
  Serial.print(ether.mymac[0]);
  Serial.print(":");
  Serial.print(ether.mymac[1]);
  Serial.print(":");
  Serial.print(ether.mymac[2]);
  Serial.print(":");
  Serial.print(ether.mymac[3]);
  Serial.print(":");
  Serial.print(ether.mymac[4]);
  Serial.print(":");
  Serial.print(ether.mymac[5]);
  ether.printIp("IP-Address:  ", ether.myip);
  ether.printIp("Gateway   :  ", ether.gwip);
  ether.printIp("Subnetmask: ", ether.netmask);
  ether.printIp("Broadcast :  ", ether.broadcastip);
#endif
}

struct ArtPollReply{ 
  char payload[239]; 
};

struct ArtPollReply ComposeArtPollReply(){
  struct ArtPollReply payload;
  uint8_t idx = 0;
  
  uint16_t opcode = 0x2100; // sets the OpCode "ArtPollReply" (don't change!)
  uint16_t artNetVersion = 0x000E; // sets the Art-Net protocol version (don't change!)
  uint8_t UBEAVersion = 0x00; // sets the firmware version of the User BIOS Extension Area (don't change!)
  uint8_t status1 = (1 << 7)|(1 << 6)|(0 << 5)|(0 << 4)|(0 << 2)|(0 << 1)|0; // sets the status of the node, part 1
  uint16_t ESTAManCode = 0x7FFF; // sets the Entertainment Service and Technology Association manufacturer code (don't change!)
  char shortName[18] = "PK Art-Net Node"; // sets the short (max 17 char + \0) name for the node
  char longName[64] = "Predigerkeller Art-Net Node 2018"; // sets the long (max 63 char + \0) name for the node
  char nodeReport[64] = "#0001 [0000] PK"; // sets debug information string (max 63 char + \0), not used
  uint16_t numPorts = 1; // sets the number of max input or output ports (0-4) 
  uint8_t portTypes[4] = { // sets the port types (input/output, type of data)
    (1 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)|(0 << 1)|0
  };
  uint8_t goodInput[4] = { // sets the input port status (don't change!)
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2), 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2), 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2), 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)
  };
  uint8_t goodOutput[4] = { // sets the output port status and configuration (don't change!)
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 7)|(0 << 6)|(0 << 5)|(0 << 4)|(0 << 3)|(0 << 2)|(0 << 1)|0
  };
  uint8_t swIn[4] = { // sets the universe for each input port
    (0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 3)|(0 << 2)|(0 << 1)|0
  };
  uint8_t swOut[4] = { // sets the univers for each output port
    (0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 3)|(0 << 2)|(0 << 1)|0, 
    (0 << 3)|(0 << 2)|(0 << 1)|0
  };
  uint8_t style = 0x00; // sets the style code for the node, deprecated
  uint8_t bindIP[4] = {ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3]}; // sets the IP address of th root device (don't change!)
  uint8_t bindIndex = 0x00; // sets the distance to the root device, 0 or 1 means this is the root device (don't change!)
  uint8_t status2 = (1 << 3)|(1 << 2)|(1 << 1)|0; // sets the status of the node, part 2
  
  for(uint8_t i = 0;i < 8;i++){
    payload.payload[idx++] = ArtNetHeader[i];
  }
  payload.payload[idx++] = low_byte(opcode);
  payload.payload[idx++] = high_byte(opcode);
  
  for(uint8_t i = 0;i < 4;i++){
     payload.payload[idx++] = ether.myip[i];
  }
  
  payload.payload[idx++] = low_byte(dstPort);
  payload.payload[idx++] = high_byte(dstPort);
  
  payload.payload[idx++] = high_byte(artNetVersion);
  payload.payload[idx++] = low_byte(artNetVersion);
  
  payload.payload[idx++] = NetSwitch;
  payload.payload[idx++] = SubSwitch;
  
  payload.payload[idx++] = high_byte(oem);
  payload.payload[idx++] = low_byte(oem);
  
  payload.payload[idx++] = UBEAVersion;
  
  payload.payload[idx++] = status1;
  
  payload.payload[idx++] = low_byte(ESTAManCode);
  payload.payload[idx++] = high_byte(ESTAManCode);

  for(uint8_t i = 0;i < 18;i++){
    payload.payload[idx++] = shortName[i];
  }
  
  for(uint8_t i = 0;i < 64;i++){
    payload.payload[idx++] = longName[i];
  }
  
  for(uint8_t i = 0;i < 64;i++){
    payload.payload[idx++] = nodeReport[i];
  }
  
  payload.payload[idx++] = high_byte(numPorts);
  payload.payload[idx++] = low_byte(numPorts);

  for(uint8_t i = 0;i < 4;i++){
    payload.payload[idx++] = portTypes[i];
  }

  for(uint8_t i = 0;i < 4;i++){
    payload.payload[idx++] = goodInput[i];
  }

  for(uint8_t i = 0;i < 4;i++){
    payload.payload[idx++] = goodOutput[i];
  }

  for(uint8_t i = 0;i < 4;i++){
    payload.payload[idx++] = swIn[i];
  }

  for(uint8_t i = 0;i < 4;i++){
    payload.payload[idx++] = swOut[i];
  }
  idx = 200;
  
  payload.payload[idx++] = style;

  for(uint8_t i = 0;i < 6;i++){
    payload.payload[idx++] = mymac[i];
  }

  for(uint8_t i = 0;i < 4;i++){
    payload.payload[idx++] = bindIP[i];
  }

  payload.payload[idx++] = bindIndex;

  payload.payload[idx++] = status2;
  
  return payload;
}

void processArtNetPacket(uint16_t dest_port, uint8_t src_ip[IP_LEN], uint16_t src_port, const char *data, uint16_t len){
#if DEBUG
  Serial.print(F("UDP packet with length "));
  Serial.print(len);
  Serial.println(F(" received."));
#endif
  if((len > 13) && (len <= 576)){ // checks if packet has the correct length
    //bool isArtNet = true;
    for(uint8_t i = 0;i < 8;i++){ // checks if the packet is an Art-Net packet
      if(data[i] != ArtNetHeader[i]){
        //isArtNet = false;
        return;
      }
    }
    //if(isArtNet){
      uint16_t OpCode = ((data[9] << 8) & 0xFF00) + (data[8] & 0xFF);
#if DEBUG
      Serial.print(F("Opcode: "));
      Serial.println(OpCode,HEX);
#endif
      if(OpCode == 0x5000){ // checks if Art-Net packet is an ArtDMX packet
#if DEBUG
        Serial.println(F("ArtDMX received."));
#endif
        if((data[14] == SubSwitch) && (data[15] == NetSwitch)){ // checks if Art-Net packets is addressed to this node
          updateNeoPixel(dmxChannel, data, len);
#if DEBUG
          Serial.println(F("NeoPixel updated."));
#endif
        }
      }
      if(OpCode == 0x2000){ // checks if Art-Net packet is an ArtPoll packet
#if DEBUG
        Serial.println(F("ArtPoll received."));
#endif
        sendArtPollReply();
#if DEBUG
        Serial.println(F("ArtPollReply sent."));
#endif
      }
    //}
  }
}

void updateNeoPixel(uint16_t dmxStartChannel, const char *data, uint16_t len){
  uint8_t dmxData[3 * NUMPIXELS];                  // initialises array that holds DMX values (0-255) for 3 channels (RGB)
  uint16_t index;                                  // and sets it according to the received data (beginning at index 18)
  for(uint8_t i = 0;i < 3 * NUMPIXELS;i++){        //
    index = 18 + dmxStartChannel + i;              //
    if(len >= index){                              //
      dmxData[i] = data[index];                    //
      continue;                                    //
    }                                              //
    dmxData[i] = 0;                                //
  }                                                // 
#if DEBUG
  Serial.println(F("\nData for first NeoPixel: "));
  Serial.print(dmxData[0],HEX);
  Serial.print(F(" "));
  Serial.print(dmxData[1],HEX);
  Serial.print(F(" "));
  Serial.println(dmxData[2],HEX);
#endif
  for(uint8_t i = 0;i < NUMPIXELS;i++){                                                   // sets the color according to the received data for each and displays it
    pixels.setPixelColor(i, dmxData[3 * i + 0], dmxData[3 * i + 1], dmxData[3 * i + 2]);  //
    pixels.show();                                                                        //
  }                                                                                       //
}

void sendArtPollReply(){
#if DEBUG
  Serial.println(F("Sending ArtPollReply..."));
#endif
  struct ArtPollReply payload = ComposeArtPollReply();
  ether.sendUdp(payload.payload, sizeof(payload.payload), srcPort, ether.broadcastip, dstPort); // sends UDP packet with Art-Net (ArtPoll) data
}

void loop () {
  ether.packetLoop(ether.packetReceive()); // waits for incoming UDP packets
}
