

struct Led { int pino; const char* nome; };

Led leds[] = {
  {25, "Vaga 1 - VERMELHO"}, {26, "Vaga 1 - VERDE"},
  {27, "Vaga 2 - VERMELHO"}, {14, "Vaga 2 - VERDE"},
  {32, "Vaga 3 - VERMELHO"}, {33, "Vaga 3 - VERDE"},
  {15, "Corredor - AZUL 1"}, {16, "Corredor - AZUL 2"}, {17, "Corredor - AZUL 3"},
};
const int QTD = sizeof(leds) / sizeof(leds[0]);

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < QTD; i++) {
    pinMode(leds[i].pino, OUTPUT);
    digitalWrite(leds[i].pino, LOW);
  }
}

void loop() {
  for (int i = 0; i < QTD; i++) {
    Serial.printf("Aceso agora: %s (GPIO %d)\n", leds[i].nome, leds[i].pino);
    digitalWrite(leds[i].pino, HIGH);
    delay(1000);
    digitalWrite(leds[i].pino, LOW);
  }
  Serial.println("---- recomecando ----");
}
