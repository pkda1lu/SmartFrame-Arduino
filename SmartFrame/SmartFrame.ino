// Smart Frame — ESP32 + ILI9341 2.8" 240x320 + XPT2046 touch
// NO-SD VARIANT: GIFs stored in LittleFS (internal flash).
// Partition Scheme: "No OTA (2MB APP / 2MB SPIFFS)".

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <AnimatedGIF.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <XPT2046_Bitbang.h>
#include <vector>

// ---------- Pins ----------
#define TFT_BL_PIN 21
#define T_SCK  25
#define T_MOSI 32
#define T_MISO 39
#define T_CS   33
#define T_IRQ  36

// ---------- Globals ----------
TFT_eSPI tft;
AnimatedGIF gif;
XPT2046_Bitbang touch(T_MOSI, T_MISO, T_SCK, T_CS);
AsyncWebServer server(80);
Preferences prefs;

const char* GIF_DIR  = "/gifs";
const char* AP_NAME  = "SmartFrame-Setup";
const char* HOSTNAME = "smartframe";

String   currentGif  = "";
uint32_t slideMillis = 15000;
uint32_t lastSwitch  = 0;
uint8_t  brightness  = 255;

// ---------- TFT bridge ----------
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *usPalette = pDraw->pPalette;
  int x, y = pDraw->iY + pDraw->y;
  static uint16_t lineBuf[320];
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < pDraw->iWidth; x++)
      if (s[x] == pDraw->ucTransparent) s[x] = pDraw->ucBackground;
    pDraw->ucHasTransparency = 0;
  }
  if (pDraw->ucHasTransparency) {
    const uint8_t tr = pDraw->ucTransparent;
    int iCount = 0, x0 = 0;
    while (x0 < pDraw->iWidth) {
      while (x0 < pDraw->iWidth && s[x0] == tr) x0++;
      int xs = x0;
      while (x0 < pDraw->iWidth && s[x0] != tr) {
        lineBuf[iCount++] = usPalette[s[x0]];
        x0++;
      }
      if (iCount) { tft.pushImage(pDraw->iX + xs, y, iCount, 1, lineBuf); iCount = 0; }
    }
  } else {
    for (x = 0; x < pDraw->iWidth; x++) lineBuf[x] = usPalette[s[x]];
    tft.pushImage(pDraw->iX, y, pDraw->iWidth, 1, lineBuf);
  }
}

void *gifOpen(const char *fname, int32_t *pSize) {
  File *f = new File();
  *f = LittleFS.open(fname);
  if (!*f) { delete f; return nullptr; }
  *pSize = f->size();
  return (void*)f;
}
void gifClose(void *p) { File *f = (File*)p; if (f) { f->close(); delete f; } }
int32_t gifRead(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  File *f = (File*)pFile->fHandle;
  int32_t r = f->read(pBuf, iLen);
  pFile->iPos = f->position();
  return r;
}
int32_t gifSeek(GIFFILE *pFile, int32_t iPos) {
  File *f = (File*)pFile->fHandle;
  f->seek(iPos);
  pFile->iPos = iPos;
  return iPos;
}

// ---------- File helpers ----------
String listGifsJson() {
  String out = "[";
  File dir = LittleFS.open(GIF_DIR);
  if (!dir || !dir.isDirectory()) return out + "]";
  bool first = true;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String n = String(f.name());
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      if (n.endsWith(".gif") || n.endsWith(".GIF")) {
        if (!first) out += ",";
        first = false;
        out += "{\"name\":\"" + n + "\",\"size\":" + String(f.size()) + "}";
      }
    }
    f = dir.openNextFile();
  }
  return out + "]";
}

void playGif(const String &name) {
  String path = String(GIF_DIR) + "/" + name;
  Serial.printf("playGif: %s\n", path.c_str());
  if (!LittleFS.exists(path)) { Serial.println("  not found in LittleFS"); return; }
  tft.fillScreen(TFT_BLACK);
  if (gif.open(path.c_str(), gifOpen, gifClose, gifRead, gifSeek, GIFDraw)) {
    Serial.printf("  opened %dx%d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    while (gif.playFrame(true, NULL)) { yield(); }
    gif.close();
    currentGif = name;
    prefs.putString("cur", currentGif);
  } else {
    Serial.printf("  gif.open FAILED, lastError=%d\n", gif.getLastError());
  }
}

void nextGif() {
  std::vector<String> names;
  File dir = LittleFS.open(GIF_DIR);
  if (!dir) return;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String n = f.name();
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      if (n.endsWith(".gif") || n.endsWith(".GIF")) names.push_back(n);
    }
    f = dir.openNextFile();
  }
  if (names.empty()) return;
  int idx = 0;
  for (size_t i = 0; i < names.size(); i++)
    if (names[i] == currentGif) { idx = (i + 1) % names.size(); break; }
  playGif(names[idx]);
}

// ---------- Touch ----------
uint32_t lastTouchMs = 0;
void pollTouch() {
  if (digitalRead(T_IRQ) == HIGH) return;
  if (millis() - lastTouchMs < 400) return;
  TouchPoint p = touch.getTouch();
  if (p.zRaw > 100) {
    lastTouchMs = millis();
    nextGif();
    lastSwitch = millis();
  }
}

// ---------- Web ----------
void setupWeb() {
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", listGifsJson());
  });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    String j = "{\"current\":\"" + currentGif + "\",\"slide_ms\":" + slideMillis +
               ",\"brightness\":" + brightness + ",\"ip\":\"" + WiFi.localIP().toString() +
               "\",\"rssi\":" + WiFi.RSSI() +
               ",\"fs_total\":" + total + ",\"fs_used\":" + used + "}";
    req->send(200, "application/json", j);
  });
  server.on("/api/select", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!req->hasParam("name", true)) { req->send(400); return; }
    playGif(req->getParam("name", true)->value());
    lastSwitch = millis();
    req->send(200, "text/plain", "ok");
  });
  server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!req->hasParam("name", true)) { req->send(400); return; }
    String path = String(GIF_DIR) + "/" + req->getParam("name", true)->value();
    if (LittleFS.exists(path)) LittleFS.remove(path);
    req->send(200, "text/plain", "ok");
  });
  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *req){
    if (req->hasParam("slide_ms", true)) {
      slideMillis = req->getParam("slide_ms", true)->value().toInt();
      if (slideMillis < 1000) slideMillis = 1000;
      prefs.putUInt("slide", slideMillis);
    }
    if (req->hasParam("brightness", true)) {
      brightness = req->getParam("brightness", true)->value().toInt();
      analogWrite(TFT_BL_PIN, brightness);
      prefs.putUChar("bri", brightness);
    }
    req->send(200, "text/plain", "ok");
  });
  server.on("/api/wifi_reset", HTTP_POST, [](AsyncWebServerRequest *req){
    req->send(200, "text/plain", "ok");
    delay(200);
    WiFiManager wm; wm.resetSettings();
    ESP.restart();
  });

  // Upload GIF to LittleFS /gifs/
  server.on("/api/upload", HTTP_POST,
    [](AsyncWebServerRequest *req){ req->send(200, "text/plain", "ok"); },
    [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final){
      String path = String(GIF_DIR) + "/" + filename;
      if (index == 0) {
        if (!LittleFS.exists(GIF_DIR)) LittleFS.mkdir(GIF_DIR);
        if (LittleFS.exists(path)) LittleFS.remove(path);
      }
      File f = LittleFS.open(path, index == 0 ? "w" : "a");
      if (f) { f.write(data, len); f.close(); }
    });

  server.begin();
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL_PIN, OUTPUT);
  analogWrite(TFT_BL_PIN, 255);
  pinMode(T_IRQ, INPUT);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Smart Frame booting...", 10, 10, 2);

  if (!LittleFS.begin(true)) {
    tft.drawString("LittleFS fail", 10, 30, 2);
  } else {
    if (!LittleFS.exists(GIF_DIR)) LittleFS.mkdir(GIF_DIR);
    tft.drawString("FS: " + String(LittleFS.usedBytes()/1024) + "/" +
                   String(LittleFS.totalBytes()/1024) + " KB", 10, 30, 2);
  }

  touch.begin();

  prefs.begin("frame", false);
  slideMillis = prefs.getUInt("slide", 15000);
  brightness  = prefs.getUChar("bri", 255);
  currentGif  = prefs.getString("cur", "");
  analogWrite(TFT_BL_PIN, brightness);

  WiFi.setHostname(HOSTNAME);
  WiFiManager wm;
  wm.setConfigPortalTimeout(600);  // 10 minutes
  wm.setDebugOutput(true);
  tft.drawString("AP: " + String(AP_NAME), 10, 50, 2);
  tft.drawString("Connect & open 192.168.4.1", 10, 70, 2);
  if (!wm.autoConnect(AP_NAME)) {
    tft.drawString("WiFi timeout, reboot", 10, 90, 2);
    delay(2000);
    ESP.restart();
  }
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());

  tft.fillScreen(TFT_BLACK);
  tft.drawString("IP: " + WiFi.localIP().toString(), 10, 10, 2);
  tft.drawString("http://" + String(HOSTNAME) + ".local", 10, 30, 2);

  gif.begin(BIG_ENDIAN_PIXELS);
  setupWeb();

  if (currentGif != "") playGif(currentGif);
  lastSwitch = millis();
}

void loop() {
  pollTouch();
  if (millis() - lastSwitch > slideMillis) {
    nextGif();
    lastSwitch = millis();
  } else if (currentGif != "") {
    String path = String(GIF_DIR) + "/" + currentGif;
    if (gif.open(path.c_str(), gifOpen, gifClose, gifRead, gifSeek, GIFDraw)) {
      unsigned long start = millis();
      while (gif.playFrame(true, NULL) && millis() - start < slideMillis - 200) {
        pollTouch();
        yield();
      }
      gif.close();
    } else {
      delay(50);
    }
  } else {
    delay(50);
  }
}
