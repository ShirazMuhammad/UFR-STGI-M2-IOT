#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {
  Serial.begin(115200); // Configured for 115200 baud
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }

  rf95.setFrequency(868.1);

  // --- 1. SET TRANSMIT POWER (+20, +14, or +8 dBm) ---
  rf95.setTxPower(8, false); // Set power to +8dBm

 
// --- 2. SET SPREADING FACTOR (Uncomment ONLY ONE line below) ---
//rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // SF7
// rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);  // SF8
// rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);  // SF10
 rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096); // SF12
}

void loop() {
  // --- 3. SET PACKET SIZE (32, 128, or 240 bytes) ---
  uint8_t data[32]; 
  memset(data, 'A', sizeof(data));

  digitalWrite(LED_BUILTIN, HIGH); // Turn LED ON

  unsigned long startTime = millis();
  rf95.send(data, sizeof(data));
  rf95.waitPacketSent();
  
  digitalWrite(LED_BUILTIN, LOW); // Turn LED OFF

  unsigned long duration = millis() - startTime; 

  Serial.print("Transmission Time: ");
  Serial.print(duration / 1000.0);
  Serial.println(" seconds");

  delay(2000); // Wait 2 seconds between packets
}