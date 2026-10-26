#include <ESP32Servo.h>

Servo cancela;

void setup() {
  Serial.begin(115200);
  cancela.setPeriodHertz(50);       
  cancela.attach(13, 500, 2400);    
  Serial.println("Teste do servo");
}

void loop() {
  Serial.println("0 graus (fechada)");
  cancela.write(0);
  delay(1500);

  Serial.println("90 graus (aberta)");
  cancela.write(90);
  delay(1500);
}