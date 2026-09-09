int counter = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT); // Controls the red pin 13 LED
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH); // Turn LED ON
  
  Serial.print("Hello World | Count: ");
  Serial.println(counter);
  counter++; // Increment counter
  
  delay(500);                      // Keep LED on for 0.5 seconds
  digitalWrite(LED_BUILTIN, LOW);  // Turn LED OFF
  delay(2500);                     // Wait remaining 2.5 seconds
}