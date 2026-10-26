
#define LED_PLACA 2  

void setup() {
  Serial.begin(115200);
  pinMode(LED_PLACA, OUTPUT);
  Serial.println("ESP32 funcionando!");
}

void loop() {
  digitalWrite(LED_PLACA, HIGH);
  delay(500);
  digitalWrite(LED_PLACA, LOW);
  delay(500);
  Serial.println("Pisca!");
}
