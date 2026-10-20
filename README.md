# ostrijh

ESP32-CAM kartı için PlatformIO ve Arduino framework ile yazılmış firmware projesi.

Kaynak kod `src/main.cpp` içindedir. Şu an PlatformIO şablonundan gelen başlangıç kodunu içerir.

## Donanım

- Kart: [ESP32-CAM](https://docs.platformio.org/en/latest/boards/espressif32/esp32cam.html) (`board = esp32cam`)
- Platform: Espressif 32
- Framework: Arduino

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
