#include "KIM.h"

#define INTERVAL_MS 60000

const char BAND[] = "B1";
const char FRQ[] = "300";
const char PWR[] = "500";
const char TCXOWU[] = "5000";
const char data[] = "BB7572A3C6D5D417D61E148D29C3110860B2C050E672A3C6D5D417D61E148D";

// Use SAMD21 Hardware Serial1 (Pins 0/RX and 1/TX)
HardwareSerial &kserial = Serial1;
KIM kim(&kserial);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Brief wait for USB Serial Monitor

  Serial.println("\n--- Initializing KIM1 Shield ---");

  // Manual hardware wake-up pulse on Pin 5 (ON/OFF pin)
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  delay(100);
  digitalWrite(5, LOW);
  delay(1000);

  if (kim.check()) {
    Serial.println("KIM -- Check SUCCESS!");
  } else {
    Serial.println("KIM -- Check FAIL! Swap TX1 and RX1 wires if this persists.");
  }

  // Send configuration parameters to KIM module
  kim.set_BAND((char *)BAND, sizeof(BAND) - 1);
  kim.set_FRQ((char *)FRQ, sizeof(FRQ) - 1);
  kim.set_PWR((char *)PWR, sizeof(PWR) - 1);
  kim.set_TCXOWU((char *)TCXOWU, sizeof(TCXOWU) - 1);

  Serial.print("KIM -- Get ID: ");
  Serial.println(kim.get_ID());
  Serial.print("KIM -- Get SN: ");
  Serial.println(kim.get_SN());
  Serial.print("KIM -- Get FW: ");
  Serial.println(kim.get_FW());
}

void loop() {
  kim.set_sleepMode(false);
  delay(500);

  Serial.print("KIM -- Sending data ... ");
  if (kim.send_data((char *)data, sizeof(data) - 1) == OK_KIM) {
    Serial.println("Message sent SUCCESS!");
  } else {
    Serial.println("Error sending data");
  }

  Serial.println("KIM -- Entering Sleep Mode");
  kim.set_sleepMode(true);
  delay(INTERVAL_MS);
}