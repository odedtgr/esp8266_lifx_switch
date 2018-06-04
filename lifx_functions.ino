const uint8_t SIZE_OF_MAC = 6;
// Port for talking to LIFX devices
int lxPort = 56700;

IPAddress bcastAddr(192, 168, 1, 255);

// Timing data
unsigned long timeoutInterval = 500;

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
#define LIFX_DEVICE_SETPOWER 117
#define LIFX_DEVICE_STATEPOWER 22
#define LIFX_DEVICE_SETCOLOR 102

// Packet buffer size
#define LIFX_INCOMING_PACKET_BUFFER_LEN 300

void SetPower(uint8_t *dest, uint16_t level) {
  lx_protocol_header_t header;
  lx_set_power_t payload;

  // Initialise both structures
  memset(&header, 0, sizeof(header));
  memset(&payload, 0, sizeof(payload));

  // Set the target the nice way
  memcpy(header.target, dest, sizeof(uint8_t) * SIZE_OF_MAC);

  // Setup the header
  header.size = sizeof(lx_protocol_header_t)+sizeof(lx_set_power_t); // Size of header + payload
  header.tagged = 0;
  header.addressable = 1;
  header.protocol = 1024;
  header.source = 123;
  header.ack_required = 0;
  header.res_required = 0;
  header.sequence = 100;
  header.type = LIFX_DEVICE_SETPOWER;

  // Setup the payload
  payload.level = level;
  payload.duration = 2000;

  // Send a packet on startup
  UDP.beginPacket(bcastAddr, lxPort);
  UDP.write((char *) &header, sizeof(lx_protocol_header_t));
  UDP.write((char *) &payload, sizeof(lx_set_power_t));
  UDP.endPacket();
}

uint16_t GetPower(uint8_t *dest) {
  uint16_t power = 1;

  lx_protocol_header_t header;

  // Initialise both structures
  memset(&header, 0, sizeof(header));

  // Set the target the nice way
  memcpy(header.target, dest, sizeof(uint8_t) * SIZE_OF_MAC);

  // Setup the header
  header.size = sizeof(lx_protocol_header_t); // Size of header + payload
  header.tagged = 0;
  header.addressable = 1;
  header.protocol = 1024;
  header.source = 123;
  header.ack_required = 0;
  header.res_required = 0;
  header.sequence = 100;
  header.type = LIFX_DEVICE_GETPOWER;

  // Send a packet on startup
  UDP.beginPacket(bcastAddr, lxPort);
  UDP.write((char *) &header, sizeof(lx_protocol_header_t));
  UDP.endPacket();

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
