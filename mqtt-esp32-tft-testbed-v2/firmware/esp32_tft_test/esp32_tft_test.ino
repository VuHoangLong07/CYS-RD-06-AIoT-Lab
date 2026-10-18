// Standalone screen test, based on the earlier working GMT130-V1.0 wiring.
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "display_config.h"
Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RESET);
unsigned long count = 0;
void setup() {
  Serial.begin(115200);
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 240, TFT_SPI_MODE);
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.setRotation(TFT_ROTATION);
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(12, 12); tft.setTextSize(2); tft.setTextColor(ST77XX_GREEN);
  tft.println("TFT TEST OK");
  tft.setTextSize(1); tft.println("240x240 / 10 MHz / no CS");
  tft.print("SPI mode: "); tft.println(TFT_SPI_MODE);
  tft.fillRect(12, 80, 65, 40, ST77XX_RED);
  tft.fillRect(87, 80, 65, 40, ST77XX_GREEN);
  tft.fillRect(162, 80, 65, 40, ST77XX_BLUE);
  Serial.println("TFT initialized before any networking. Check colour bars and counter.");
}
void loop() {
  delay(1000);
  tft.fillRect(12, 145, 216, 20, ST77XX_BLACK);
  tft.setCursor(12, 145); tft.setTextSize(2); tft.setTextColor(ST77XX_WHITE);
  tft.print("Count "); tft.println(++count);
  Serial.print("TFT count "); Serial.println(count);
}
