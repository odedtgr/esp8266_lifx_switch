#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char *ssid = "TAGAR";
const char *password = "mayahers";

const int BUTTON = 0;

boolean light_state = 0;

const uint8_t SIZE_OF_MAC = 6;

// Port for talking to LIFX devices
int lxPort = 56700;

// Port we listen to
unsigned int localPort = 8888;

// Holds the current button state.
volatile int state;

// Holds the last time debounce was evaluated (in millis).
volatile long lastDebounceTime = 0;

// The delay threshold for debounce checking.
const int debounceDelay = 50;

IPAddress bcastAddr(192, 168, 1, 255);
IPAddress ip(192, 168, 1, 73);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiUDP UDP;

#pragma pack(push, 1)
typedef struct {
  /* frame */
  uint16_t size;
  uint16_t protocol : 12;
  uint8_t addressable : 1;
  uint8_t tagged : 1;
  uint8_t origin : 2;
  uint32_t source;
  /* frame address */
  uint8_t target[8];
  uint8_t reserved[6];
  uint8_t res_required : 1;
  uint8_t ack_required : 1;
  uint8_t : 6;
  uint8_t sequence;
  /* protocol header */
  uint64_t : 64;
  uint16_t type;
  uint16_t : 16;
  /* variable length payload follows */
} lx_protocol_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  /* set power */
  uint16_t level;
  uint32_t duration;
} lx_set_power_t;
#pragma pack(pop)

// Device::StatePower Payload
#pragma pack(push, 1)
typedef struct {
  uint16_t level;
} lx_state_power_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  uint8_t reserved;
  uint16_t hue;
  uint16_t saturation;
  uint16_t brightness;
  uint16_t kelvin;
  uint32_t duration;
} lx_set_color_t;
#pragma pack(pop)


// Payload types
#define LIFX_DEVICE_GETPOWER 20
#define LIFX_DEVICE_SETPOWER 21
#define LIFX_DEVICE_STATEPOWER 22
#define LIFX_DEVICE_SETCOLOR 102

unsigned long timeoutInterval = 500;
// Packet buffer size
#define LIFX_INCOMING_PACKET_BUFFER_LEN 300


void lxMakeFrame(lx_protocol_header_t *lxHead, uint8_t extraSize, uint8_t tagged, uint8_t *target, uint16_t message) {
  /* frame */
  lxHead->size = (uint8_t)36 + extraSize;
  lxHead->protocol = (uint16_t)1024;
  lxHead->addressable = (uint8_t)1;
  lxHead->tagged = tagged;
  lxHead->origin = (uint8_t)0;
  lxHead->source = (uint32_t)3549;

  /* frame address */
  uint8_t i = 0;
  for (i = 0; i < 8; i++) {
    lxHead->target[i] = (uint8_t)target[i];
  }
  lxHead->res_required = (uint8_t)0;
  lxHead->ack_required = (uint8_t)0;
  lxHead->sequence = (uint8_t)0;

  /* protocol header */
  lxHead->type = message;
}

void SetColor(uint8_t *target_addr) {

  /* Build payload */

  lx_protocol_header_t *lxHead;
  lxHead = (lx_protocol_header_t *)calloc(1, sizeof(lx_protocol_header_t));
  lxMakeFrame(lxHead, sizeof(lx_set_color_t), 0, target_addr, LIFX_DEVICE_SETCOLOR);

  lx_set_color_t *lxSetColor;
  lxSetColor = (lx_set_color_t *)calloc(1, sizeof(lx_set_color_t));
  lxSetColor->duration = 2000;
  lxSetColor->hue = 0;
  lxSetColor->saturation = 0;
  lxSetColor->brightness = 65535;
  lxSetColor->kelvin = 2500;

  UDP.beginPacket(bcastAddr, lxPort);
  byte *b = (byte *)lxHead;
  UDP.write(b, sizeof(lx_protocol_header_t));
  b = (byte *)lxSetColor;
  UDP.write(b, sizeof(lx_set_color_t));
  UDP.endPacket();

  free(lxSetColor);
  free(lxHead);
}

void SetPower(uint8_t *target_addr, uint16_t level) {

  /* Build payload */

  lx_protocol_header_t *lxHead;
  lxHead = (lx_protocol_header_t *)calloc(1, sizeof(lx_protocol_header_t));
  lxMakeFrame(lxHead, sizeof(lx_set_power_t), 0, target_addr, LIFX_DEVICE_SETPOWER);

  lx_set_power_t *lxSetPower;
  lxSetPower = (lx_set_power_t *)calloc(1, sizeof(lx_set_power_t));
  lxSetPower->duration = 2000;
  lxSetPower->level = level;

  UDP.beginPacket(bcastAddr, lxPort);
  byte *b = (byte *)lxHead;
  UDP.write(b, sizeof(lx_protocol_header_t));
  b = (byte *)lxSetPower;
  UDP.write(b, sizeof(lx_set_power_t));
  UDP.endPacket();

  free(lxSetPower);
  free(lxHead);
}

uint16_t GetPower(uint8_t *target_addr) {
  uint16_t power = 1;
  
  /* Build payload */

  lx_protocol_header_t *lxHead;
  lxHead = (lx_protocol_header_t *)calloc(1, sizeof(lx_protocol_header_t));
  lxMakeFrame(lxHead, 0, 0, target_addr, LIFX_DEVICE_GETPOWER);


  UDP.beginPacket(bcastAddr, lxPort);
  byte *b = (byte *)lxHead;
  UDP.write(b, sizeof(lx_protocol_header_t));
  UDP.endPacket();
  free(lxHead);
  
  unsigned long started = millis();
  while (millis() - started < timeoutInterval) {
    int packetLen = UDP.parsePacket();
    byte packetBuffer[LIFX_INCOMING_PACKET_BUFFER_LEN];
    if (packetLen && packetLen < LIFX_INCOMING_PACKET_BUFFER_LEN) {
      UDP.read(packetBuffer, sizeof(packetBuffer));
      
      if (((lx_protocol_header_t *)packetBuffer)->type == LIFX_DEVICE_STATEPOWER) {
        power = ((lx_state_power_t *)(packetBuffer + sizeof(lx_protocol_header_t)))->level;
        return power;
      } else {
        Serial.print("Unexpected Packet type: ");
        Serial.println(((lx_protocol_header_t *)packetBuffer)->type);
      }
    }
  }
  return power;
}



void startWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.persistent(false);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wifi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void startUDP() {
  UDP.begin(localPort);
  Serial.println("Local port:\t");
  Serial.println(UDP.localPort());
  Serial.println();
}
void setup() {
  Serial.begin(115200);

  startWifi();
  startUDP();

  /* Setup Interupts */
  pinMode(BUTTON, INPUT_PULLUP);
  attachInterrupt(BUTTON, onChange, CHANGE);
}

// Gets called by the interrupt.
void onChange() {
  noInterrupts();
  // Get the pin reading.
  boolean reading = digitalRead(BUTTON);

  // Ignore dupe readings.
  if (reading == state) {
    interrupts();
    return;
  }

  // Check to see if the change is within a debounce delay threshold.
  if ((millis() - lastDebounceTime) <= debounceDelay) {
    interrupts();
    return;
  }
 
  // This update to the last debounce check is necessary regardless of debounce state.
  //Serial.println("time: " + String(millis() - lastDebounceTime));
  lastDebounceTime = millis();

  // All is good, persist the reading as the state.
  state = reading;

  // Work with the value now.
  Serial.println("button: " + String(reading));

  //detachInterrupt(BUTTON);
  if (reading) {
    uint8_t leftLight []= {0xd0, 0x73, 0xd5, 0x24, 0xd4, 0x27};
    uint8_t rightLight [] = {0xd0, 0x73, 0xd5, 0x24, 0xd3, 0xf9};
    uint16_t power = GetPower(leftLight);
    Serial.println("GetPower: " + String(power));
//    if (light_state) {
//      SetPower(rightLight, 0);
//      SetPower(leftLight, 0);
//      light_state = 0;
//    } else {
//      SetPower(rightLight, 65535);
//      SetPower(leftLight, 65535);
//      light_state = 1;
//    }
  }
  interrupts();
}

void loop() {
}