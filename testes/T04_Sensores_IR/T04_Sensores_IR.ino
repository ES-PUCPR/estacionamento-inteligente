

const int PINOS[] = {34, 35, 36, 39};
const char* NOMES[] = {"Vaga 1", "Vaga 2", "Vaga 3", "Corredor"};

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) pinMode(PINOS[i], INPUT);
}

void loop() {
  for (int i = 0; i < 4; i++) {
    bool detectou = (digitalRead(PINOS[i]) == LOW);
    Serial.printf("%s: %-9s  ", NOMES[i], detectou ? "OBJETO" : "livre");
  }
  Serial.println();
  delay(300);
}
