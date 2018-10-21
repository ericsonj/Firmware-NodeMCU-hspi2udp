#include<SPI.h>
#include<ESP8266WiFi.h>
#include <WiFiUdp.h>

typedef enum {
    WAIT_DATA,
    WAIT_TYPE,
    WAIT_LEN,
    READ_BYTES,
    TRANSMIT
} txState_t;

typedef enum {
  RX_WAIT_DATA,
  RX_TRANSMIT
} rxState_t;

uint8_t bufferTx[64];
uint8_t *bufferTxUDP = (uint8_t *)malloc(200);
uint8_t bufferTxIdx;

uint8_t *bufferRx = (uint8_t *)malloc(64);
uint8_t bufferRxUDP[200];
uint8_t bufferRxIdx;
uint8_t bufferRxUDPIdx;

txState_t txState = WAIT_DATA;
rxState_t rxState = RX_WAIT_DATA;

uint8_t txDateLen;
uint8_t txDateLenCount;
int rxDateLenCount;

// WiFi Connection info
const char *ssid = "WA901ND";
const char *pw = "4QR6X6V5B5";

IPAddress ip(192, 168, 88, 70);
IPAddress gateway(192, 168, 88, 1);
IPAddress netmask(255, 255, 255, 0);
const int localPort = 19000;

// WiFi Server info
WiFiUDP udp;
IPAddress serverIp(192, 168, 88, 64);
//IPAddress serverIp(207, 246, 65, 130); //ericsonj.net

void setup() {

  Serial.begin(115200);
  
  SPI.begin();
  SPI.setHwCs(true);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  pinMode(D4, OUTPUT);
  digitalWrite(D4, HIGH);

  WiFi.config(ip, gateway, netmask);
  WiFi.begin(ssid, pw);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  
  txDateLen = 0;
  txDateLenCount = 0;
  bufferRxIdx = 0;
  bufferTxIdx = 0;
  bufferRxUDPIdx = 0;
  
  udp.begin(localPort);

  Serial.println("");
  Serial.print(WiFi.localIP());
  Serial.println(" UDP OK");

}

void loop() {
  
  int packetSize;
  memset(bufferTx, 0, 64);
  
  switch (rxState) {
  case RX_WAIT_DATA:
    packetSize = udp.parsePacket();
    if(packetSize > 0){
      bufferRxIdx = 0;
      bufferRxUDPIdx = 0;
      rxDateLenCount = packetSize;
      udp.read(bufferRxUDP, packetSize);
      bufferTx[bufferRxIdx++] = 0x84;
      bufferTx[bufferRxIdx++] = 0x00;
      bufferTx[bufferRxIdx++] = (uint8_t)packetSize;
      while(bufferRxIdx < 64 && rxDateLenCount > 0){
        bufferTx[bufferRxIdx++] = bufferRxUDP[bufferRxUDPIdx];
        bufferRxUDPIdx++;
        rxDateLenCount--;
      }
      rxState = RX_TRANSMIT;
    }
    break;
  case RX_TRANSMIT:
    bufferRxIdx = 0;
    while(bufferRxIdx < 64 && rxDateLenCount > 0){
      bufferTx[bufferRxIdx++] = bufferRxUDP[bufferRxUDPIdx];
      bufferRxUDPIdx++;
      rxDateLenCount--;
    }
    if(rxDateLenCount <= 0){
      rxState = RX_WAIT_DATA;
    }
    break;
  }

  SPI.transferBytes(bufferTx, bufferRx, 64);

  for (int i = 0; i < 64; i++) {
    switch (txState) {
    case WAIT_DATA:
      if(bufferRx[i] == 0x84){
        txState = WAIT_TYPE;
      }
      break;
    case WAIT_TYPE:
      if(bufferRx[i] == 0x00){
        txState = WAIT_LEN;
      }else{
        txState = WAIT_DATA;
      }
      break;
    case WAIT_LEN:
      txDateLenCount = bufferRx[i];
      txDateLen = txDateLenCount;
      if(txDateLenCount == 0){
        txState = WAIT_DATA;
      }else{
        bufferTxIdx = 0;
        txState = READ_BYTES;
      }
      break;
    case READ_BYTES:
      bufferTxUDP[bufferTxIdx] = bufferRx[i];
      bufferTxIdx++;
      txDateLenCount--;
      if(txDateLenCount == 0){
        txState = TRANSMIT;
      }
      break;
    case TRANSMIT:
      udp.beginPacket(serverIp, localPort);
      udp.write(bufferTxUDP, txDateLen);
      udp.endPacket();
      txState = WAIT_DATA;
      break;
    }

  }

}
