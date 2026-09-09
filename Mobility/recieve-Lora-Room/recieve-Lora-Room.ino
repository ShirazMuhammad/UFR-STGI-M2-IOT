#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

RH_RF95 rf95(RFM95_CS, RFM95_INT);

uint8_t buf[255];

void setup() {
  Serial.begin(115200);
  
  // Reset radio module on startup
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
  
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // BW=125kHz, CR=4/5, SF=7
  rf95.setFrequency(868.1);                     // Set frequency to 868.1 MHz
}

void loop() {
  uint8_t len = sizeof(buf);
  
  // Wait for incoming packet up to 4 seconds
  if (rf95.waitAvailableTimeout(4000)) {
    if (rf95.recv(buf, &len)) {
      buf[len] = 0; // End string cleanly
      Serial.print("Message: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
      Serial.print("SNR: ");
      Serial.println(rf95.lastSNR(), DEC);
    }
  } else {
    Serial.println("No reception");
  }
}