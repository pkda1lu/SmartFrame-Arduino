// SD card pin sanity test — try the seller's pinout, then list files.
// Watch Serial Monitor at 115200.

#include <SPI.h>
#include <SD.h>

#define SD_SCK   18
#define SD_MISO  19
#define SD_MOSI  23
#define SD_CS    5

SPIClass sdSPI(VSPI);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== SD test ===");
  Serial.printf("Pins: SCK=%d MISO=%d MOSI=%d CS=%d\n", SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  if (!SD.begin(SD_CS, sdSPI, 1000000)) {
    Serial.println("SD.begin FAILED at 1 MHz");
    Serial.println("-> check: card inserted, FAT32, pins correct");
    return;
  }

  uint8_t type = SD.cardType();
  const char* t = type == CARD_NONE ? "NONE" :
                  type == CARD_MMC  ? "MMC"  :
                  type == CARD_SD   ? "SD"   :
                  type == CARD_SDHC ? "SDHC" : "UNKNOWN";
  Serial.printf("OK. Card type: %s, size: %llu MB\n",
    t, SD.cardSize() / (1024ULL*1024ULL));

  File root = SD.open("/");
  File f;
  Serial.println("Root listing:");
  while ((f = root.openNextFile())) {
    Serial.printf("  %s  %u bytes%s\n", f.name(), f.size(), f.isDirectory() ? "  <DIR>" : "");
    f.close();
  }
}

void loop() {}
