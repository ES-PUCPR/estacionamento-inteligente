
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // SDA, SCL
}

void loop() {
  int achados = 0;
  Serial.println("Procurando dispositivos I2C...");
  for (byte end = 1; end < 127; end++) {
    Wire.beginTransmission(end);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Encontrado dispositivo em 0x%02X\n", end);
      achados++;
    }
  }
  if (achados == 0) Serial.println("  Nada encontrado. Confira SDA/SCL e alimentacao.");
  delay(3000);
}
