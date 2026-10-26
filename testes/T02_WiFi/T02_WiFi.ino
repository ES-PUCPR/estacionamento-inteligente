
#include <WiFi.h>

const char* WIFI_NOME  = "Waked 2.4g"; 
const char* WIFI_SENHA = "libano0712";  

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  Serial.println("Procurando redes...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    Serial.printf("  %2d) %-32s sinal %d dBm\n", i + 1, WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i));
  }

  Serial.printf("\nConectando em \"%s\"", WIFI_NOME);
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado!");
    Serial.print("IP da placa: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNAO conectou. Confira nome/senha e se a rede e 2,4 GHz.");
  }
}

void loop() {}
