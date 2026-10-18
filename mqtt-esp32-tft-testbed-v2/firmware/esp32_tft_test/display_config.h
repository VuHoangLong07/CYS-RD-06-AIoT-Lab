#pragma once
#include <SPI.h>
// Earlier GMT130-V1.0 240x240 ST7789 wiring. No CS and no MISO.
const int TFT_SCK = 18;
const int TFT_MOSI = 23;
const int TFT_DC = 21;
const int TFT_RESET = 22;
const int TFT_CS = -1;
// Your saved standalone test used MODE3. If MODE0 is the working setting on
// your actual module, change this in BOTH the test and combined sketch folders.
const uint8_t TFT_SPI_MODE = SPI_MODE3;
const uint32_t TFT_SPI_HZ = 10000000;
const uint8_t TFT_ROTATION = 0;
