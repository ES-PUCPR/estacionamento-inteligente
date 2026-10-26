
#include <SPI.h>
#include <MFRC522.h>

#define RFID_SS  5
#define RFID_RST 4

MFRC522 rfid(RFID_SS, RFID_RST);

void setup() {
  Serial.begin(115200);
  SPI.begin(18, 19, 23, RFID_SS);  // SCK, MISO, MOSI, SS
  rfid.PCD_Init();
  delay(50);
  byte versao = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  if (versao == 0x00 || versao == 0xFF) {
    Serial.println("Leitor NAO respondeu. Confira os fios (e a solda dos pinos!).");
  } else {
    Serial.printf("Leitor OK (versao 0x%02X). Aproxime um cartao...\n", versao);
  }
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  Serial.print("UID do cartao:");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.printf(" %02X", rfid.uid.uidByte[i]);
  }
  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}
