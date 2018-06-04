#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char *ssid = "TAGAR";
const char *password = "mayahers";

const int BUTTON = 13;

boolean toggle_now = 0;

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

unsigned long lastSend = 0;

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
  attachInterrupt(BUTTON, onButtonChange, CHANGE);
}

void lxToggle() {
  uint8_t leftLight [] = {0xd0, 0x73, 0xd5, 0x24, 0xd4, 0x27};
  uint8_t rightLight [] = {0xd0, 0x73, 0xd5, 0x24, 0xd3, 0xf9};
  uint16_t power = GetPower(leftLight);
  //Serial.println("GetPower: " + String(power));
  if (power) {
    SetPower(rightLight, 0);
    SetPower(leftLight, 0);
  } else {
    SetPower(rightLight, 65535);
    SetPower(leftLight, 65535);
  }
}

// Gets called by the interrupt.
void onButtonChange() {
  // Get the pin reading.
  boolean reading = digitalRead(BUTTON);

  // Ignore dupe readings.
  if (reading == state) {
    return;
  }

  // Check to see if the change is within a debounce delay threshold.
  if ((millis() - lastDebounceTime) <= debounceDelay) {
    return;
  }

  // This update to the last debounce check is necessary regardless of debounce state.
  //Serial.println("time: " + String(millis() - lastDebounceTime));
  lastDebounceTime = millis();

  // All is good, persist the reading as the state.
  state = reading;

  // Work with the value now.
  //Serial.println("button: " + String(reading));

  if (!reading) {
    toggle_now = 1;
  }
}

void loop() {
  if(toggle_now){
    toggle_now = 0;
    lxToggle();
  }
}
