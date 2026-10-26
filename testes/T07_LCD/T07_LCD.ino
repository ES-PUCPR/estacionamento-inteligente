
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LCD_ENDERECO 0x27  

LiquidCrystal_I2C lcd(LCD_ENDERECO, 16, 2);

void setup() {
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Estacionamento");
  lcd.setCursor(0, 1);
  lcd.print("Inteligente :)");
}

void loop() {}
