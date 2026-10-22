# ostrijh

ESP32-CAM kartı için PlatformIO ve Arduino framework ile yazılmış firmware projesi.

Kaynak kod `src/main.cpp` içindedir. Şu an PlatformIO şablonundan gelen başlangıç kodunu içerir.

## Donanım

- Kart: [AI-Thinker ESP32-CAM](https://docs.platformio.org/en/latest/boards/espressif32/esp32cam.html) (`board = esp32cam`)
- Platform: Espressif 32
- Framework: Arduino
- Sensör: MPU6050, 6 eksen (3 eksen ivmeölçer + 3 eksen jiroskop), I2C

### MPU6050 bağlantısı

SD kart kullanılmıyor. Kamera GPIO 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36 ve 39’u kullanır. GPIO 16 PSRAM’e aittir. GPIO 1 ve GPIO 3 seri porttur; programlama ve log için boş kalır.

| MPU6050 | ESP32-CAM |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 13 |
| SCL | GPIO 14 |
| AD0 | GND (adres `0x68`) |
| INT | Bağlı değil (veri I2C üzerinden okunur) |

### Motor sürücüleri (MOSFET)

Dört motor, MOSFET’li sürücü devrelerinin gate girişlerinden PWM ile sürülür.

| Motor | ESP32-CAM |
| --- | --- |
| Motor 1 | GPIO 2 |
| Motor 2 | GPIO 4 |
| Motor 3 | GPIO 12 |
| Motor 4 | GPIO 15 |

Bu pinlerin hepsinde açılışta durum önemlidir:

- Her gate ile GND arasına 10 kΩ pull-down direnç takılır. GPIO 2 ve GPIO 12 açılışta LOW olmalıdır; aksi halde kart yüklenemez (GPIO 2) ya da flash 1.8 V seçilip açılmaz (GPIO 12). Pull-down, açılışta motorların dönmesini de engeller.
- GPIO 4 kart üzerindeki flaş LED’ine bağlıdır. Motor 2 çalışırken LED de yanar.
- GPIO 15 LOW tutulduğu için açılış logu seri porta basılmaz.
- MOSFET 3.3 V gate ile tam açılan logic-level bir tip olmalıdır (ör. AO3400, IRLML6344). Her motorun uçlarına ters yönde flyback diyot takılır. Motor beslemesinin GND’si ESP32-CAM GND’sine bağlanır.
- Kamera XCLK için LEDC timer 0 / kanal 0’ı kullanır. Motor PWM’i başka timer ve kanallarda kurulur.

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
