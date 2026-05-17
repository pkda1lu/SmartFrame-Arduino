# Smart Frame — ESP32 + ILI9341 2.8"

Wi-Fi цифровая рамка. Картинки (GIF) на microSD, веб-морда для управления, OTA обновления.

## Распиновка платы

| Назначение | Пин ESP32 |
|---|---|
| TFT SCK | GPIO14 |
| TFT MOSI | GPIO13 |
| TFT MISO | GPIO12 |
| TFT CS | GPIO15 |
| TFT DC | GPIO2 |
| TFT RST | GPIO4 |
| TFT BL (подсветка) | GPIO21 |
| Touch SCK | GPIO25 |
| Touch MOSI | GPIO32 |
| Touch MISO | GPIO39 |
| Touch CS | GPIO33 |
| Touch IRQ | GPIO36 |
| SD SCK | GPIO18 |
| SD MOSI | GPIO23 |
| SD MISO | GPIO19 |
| SD CS | GPIO5 |

Дисплей сидит на **HSPI**, SD на **VSPI**, тач — софтовый SPI (bitbang), потому что его пины не совпадают ни с одним аппаратным SPI.

## 1. Установка Arduino IDE

1. Arduino IDE 2.x
2. Добавь поддержку ESP32: File → Preferences → Additional boards URL:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Tools → Board → Boards Manager → найди **esp32** by Espressif → установи.
4. Выбери плату: **Tools → Board → ESP32 Dev Module**
   - Partition Scheme: **Default 4MB with spiffs** (или **Huge APP** если не хватит места — но тогда LittleFS урежется)
   - Flash Size: 4MB
   - PSRAM: Disabled (если на твоей плате нет PSRAM)

## 2. Библиотеки (Library Manager)

Поставь через Sketch → Include Library → Manage Libraries:

- **TFT_eSPI** (Bodmer)
- **AnimatedGIF** (Larry Bank)
- **WiFiManager** (tzapu)
- **ESP Async WebServer** (ESP32Async / lacamera) и **AsyncTCP** (ESP32Async)
- **XPT2046_Bitbang_Slim** (или `XPT2046_Bitbang` от Nitek — API одинаковый: `getTouch()` возвращает `TouchPoint`)

`SD`, `LittleFS`, `Preferences`, `ArduinoOTA`, `WiFi`, `SPI`, `FS` — уже в ESP32 core.

## 3. Настройка TFT_eSPI

**Самый частый источник граблей.** Библиотека TFT_eSPI читает конфигурацию из `User_Setup.h` внутри своей папки, а не из проекта.

Скопируй `SmartFrame/User_Setup.h` (из этого проекта) поверх:

```
Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
```

Ключевое там:
- драйвер ILI9341,
- пины SCK=14/MOSI=13/MISO=12/CS=15/DC=2/RST=4,
- `#define USE_HSPI_PORT` — обязательно, иначе TFT_eSPI попробует взять VSPI и подерётся с SD.
- `TFT_BL` намеренно не определён — подсветкой управляем сами через `analogWrite(21, …)`.

## 4. Заливка веб-морды в LittleFS

Файлы из папки `data/` нужно залить в LittleFS отдельно от скетча.

Вариант A — плагин **arduino-littlefs-upload** для Arduino IDE 2.x:
1. Скачай VSIX отсюда: <https://github.com/earlephilhower/arduino-littlefs-upload/releases>
2. В Arduino IDE → Ctrl+Shift+P → `Install From VSIX` → выбери файл.
3. Закрой Serial Monitor, открой проект, Ctrl+Shift+P → `Upload LittleFS to Pico/ESP8266/ESP32`.

Вариант B — собрать образ вручную утилитой `mklittlefs` и залить `esptool.py` (если плагин не зашёл — напиши, дам команды).

## 5. Прошивка

1. Открой `SmartFrame/SmartFrame.ino`.
2. Tools → Port → выбери COM-порт платы.
3. Upload (Ctrl+U).

На первом запуске ESP не знает Wi-Fi → поднимет точку доступа **`SmartFrame-Setup`**. С телефона подключись к ней, откроется портал, выбери свою сеть, введи пароль. После этого ESP перезагрузится и подключится сам.

На дисплее выведется IP. Открой `http://<ip>` (или `http://smartframe.local` если в системе есть mDNS).

## 6. Подготовка GIF

Дисплей 240×320. В горизонтальной ориентации (как в коде, `setRotation(1)`) — 320×240.

Чтобы было быстро и красиво:
- Размер кадра ≤ **320×240**.
- Не больше **~30 кадров** на гифку, иначе на SD долго грузится.
- Палитра 256 цветов (стандарт GIF).
- Конвертация на ПК:
  ```
  ffmpeg -i input.mp4 -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2,fps=12" -loop 0 output.gif
  ```

Загружай через веб-морду (drag&drop). Файлы кладутся на SD в `/gifs/`.

## 7. OTA

После первой прошивки и подключения к Wi-Fi плата появляется в Arduino IDE как сетевой порт (`smartframe at 192.168.x.x`). Просто выбираешь его как порт и Upload — без USB.

## 8. Что в MVP пока не сделано (TODO)

- Центровка GIF меньше 320×240 (сейчас рисуется от 0,0).
- JPEG/PNG (только GIF).
- Калибровка тача (сейчас просто «тап → следующая картинка»).
- Расписание (день/ночь, режим сна).
- Темы оформления веб-морды.
- Внешний вид UI — дорабатываем потом.

## Структура проекта

```
SmartFrame/
  SmartFrame.ino   — основной скетч
  User_Setup.h     — конфиг TFT_eSPI (копируется в папку библиотеки!)
  data/
    index.html     — веб-морда (загружается в LittleFS)
  README.md
```
