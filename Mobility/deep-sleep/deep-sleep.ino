#include <SPI.h>
#include <RH_RF95.h>
#include <RTCZero.h>

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RTCZero rtc;

void alarmMatch() {
  // Interrupt handler for RTC alarm
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Initialize Real-Time Clock
  rtc.begin();
  rtc.enableAlarm(rtc.MATCH_SS);
  rtc.attachInterrupt(alarmMatch);

  // Reset and initialize LoRa module
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!rf95.init()) {
    while (1);
  }

  rf95.setFrequency(868.1);
  rf95.setTxPower(8, false);
}

void loop() {
  // Double blink LED (100ms) to visualize wake-up
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);

  // Transmit data
  uint8_t data[] = "Deep Sleep Packet";
  rf95.send(data, sizeof(data));
  rf95.waitPacketSent();

  // Put radio to sleep
  rf95.sleep();

  // Set alarm for 5 seconds into the future and enter MCU standby
  rtc.setAlarmSeconds((rtc.getSeconds() + 5) % 60);
  rtc.standbyMode(); // Deep sleep until next alarm match
}