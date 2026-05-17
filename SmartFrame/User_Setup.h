// TFT_eSPI User_Setup for ESP32 + ILI9341 2.8" (240x320), board pinout from seller.
// COPY THIS FILE INTO: <Arduino>/libraries/TFT_eSPI/User_Setup.h
// (overwrite the default), OR create a custom setup in User_Setup_Select.h.

#define USER_SETUP_INFO "SmartFrame_ESP32_ILI9341"

#define ILI9341_DRIVER

// Display SPI pins (HSPI on ESP32)
#define TFT_MISO  12
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4

// Backlight is controlled manually in sketch via GPIO21 (PWM)
// (do not define TFT_BL here to avoid TFT_eSPI taking it over)

// Fonts (enable what you need; smaller = less flash)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT

// SPI frequency
#define SPI_FREQUENCY        40000000
#define SPI_READ_FREQUENCY   20000000

// TFT_eSPI on ESP32 uses VSPI by default. We need HSPI because VSPI is used by SD.
#define USE_HSPI_PORT
