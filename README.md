# ostrijh

ESP32-CAM kartı için PlatformIO ve Arduino framework ile yazılmış firmware projesi.

Kaynak kod `src/main.cpp` içindedir. Şu an PlatformIO şablonundan gelen başlangıç kodunu içerir.

## Donanım

- Kart: [AI-Thinker ESP32-CAM](https://docs.platformio.org/en/latest/boards/espressif32/esp32cam.html) (`board = esp32cam`)
- Platform: Espressif 32
- Framework: Arduino
- Sensör: MPU6050, 6 eksen (3 eksen ivmeölçer + 3 eksen jiroskop), I2C

### MPU6050 bağlantısı

SD kart kullanılmıyor. Kamera GPIO 0, 4, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36 ve 39’u tutar. I2C bu pinlere bağlanmaz.

| MPU6050 | ESP32-CAM |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 13 |
| SCL | GPIO 14 |
| AD0 | GND (adres `0x68`) |
| INT | GPIO 2 (isteğe bağlı) |

GPIO 12’ye bağlama. Üzerindeki pull-up, açılışta flash gerilimini 1.8 V seçer ve kart açılmaz. GPIO 1 ve GPIO 3 seri porttur; programlama için boş kalır. GPIO 16 ve GPIO 17 PSRAM’e aittir.

## Gereksinimler

- [PlatformIO](https://platformio.org/) (CLI veya VS Code / Cursor eklentisi)
- USB-UART adaptörü (ESP32-CAM üzerinde yerleşik USB yoktur)

## Derleme ve yükleme

Proje kökünden:

```bash
pio run
pio run -t upload
pio device monitor
```

Ortam adı `esp32cam` olarak `platformio.ini` içinde tanımlıdır. Yükleme portu otomatik bulunamazsa:

```bash
pio run -t upload --upload-port /dev/ttyUSB0
```

Port adını `pio device list` ile kontrol edin. ESP32-CAM’i programlama moduna almak için yükleme sırasında GPIO0’ı GND’ye çekip resetleyin; yükleme bitince GPIO0’ı serbest bırakıp kartı yeniden başlatın.

## Dizin yapısı

| Dizin | Amaç |
| --- | --- |
| `src/` | Uygulama kaynak kodu |
| `include/` | Proje başlık dosyaları |
| `lib/` | Projeye özel kütüphaneler |
| `test/` | PlatformIO birim testleri |
