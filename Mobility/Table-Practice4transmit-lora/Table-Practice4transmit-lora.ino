#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  digitalWrite(RFM95_RST, LOW);  delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);

  if (!rf95.init()) while (1);
  rf95.setFrequency(868.1);

  // === 1. TRANSMIT POWER (Uncomment ONE line) ===
  // rf95.setTxPower(20, false); // +20 dBm
  // rf95.setTxPower(14, false); // +14 dBm
   rf95.setTxPower(8, false);  // +8 dBm

  // === 2. SPREADING FACTOR (Uncomment ONE line) ===
   rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // SF7
  // rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); rf95.spiWrite(0x1E, (rf95.spiRead(0x1E) & 0x0F) | (8 << 4));  // SF8
  // rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); rf95.spiWrite(0x1E, (rf95.spiRead(0x1E) & 0x0F) | (10 << 4)); // SF10
  // rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096); // SF12
}

void loop() {
  // === 3. PAYLOAD SIZE (Uncomment ONE line) ===
  // uint8_t data[32];  // 32 bytes
   uint8_t data[256]; // 256 bytes

  memset(data, 'A', sizeof(data));

  digitalWrite(LED_BUILTIN, HIGH);
  unsigned long startTime = millis();

  rf95.send(data, sizeof(data));
  rf95.waitPacketSent();

  digitalWrite(LED_BUILTIN, LOW);
  unsigned long duration = millis() - startTime;

  Serial.print("Transmission Time: ");
  Serial.print(duration / 1000.0);
  Serial.println(" seconds");

  delay(2000);
}