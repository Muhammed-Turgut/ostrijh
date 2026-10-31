#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"

namespace {

constexpr char AP_SSID[] = "ostrijh-bench";
constexpr char AP_PASSWORD[] = "benchcam32";
constexpr int I2C_SDA_PIN = 13;
constexpr int I2C_SCL_PIN = 14;
constexpr uint8_t MPU6050_ADDRESS = 0x68;
constexpr uint8_t MOTOR_PINS[] = {2, 4, 12, 15};

httpd_handle_t server = nullptr;
httpd_handle_t streamServer = nullptr;
bool cameraReady = false;
bool mpu6050Ready = false;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="tr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ostrijh | Masaüstü Test</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    body { max-width: 1050px; margin: 0 auto; padding: 20px; background: #10151d; color: #e8edf4; }
    h1 { margin-bottom: 4px; } p { color: #aab6c5; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit,minmax(280px,1fr)); gap: 16px; }
    section { background: #1a2330; border: 1px solid #2b394a; border-radius: 12px; padding: 16px; }
    img { width: 100%; min-height: 180px; background: #090c10; border-radius: 8px; object-fit: contain; }
    .status { color: #55d68b; font-weight: 700; }
    .error { color: #ff7777; } .motors { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .motor { padding: 12px; background: #121923; border-radius: 8px; }
    table { width: 100%; border-collapse: collapse; } td { padding: 6px 2px; border-bottom: 1px solid #2b394a; }
    td:last-child { text-align: right; font-variant-numeric: tabular-nums; }
    small { color: #98a6b7; }
  </style>
</head>
<body>
  <h1>ostrijh masaüstü test paneli</h1>
  <p>Kamera, MPU6050 telemetrisi ve güvenli motor çıkışı izleme.</p>
  <div class="grid">
    <section><h2>Kamera</h2><img alt="Kamera görüntüsü" id="camera">
      <div id="cameraState" class="status">Bağlanıyor…</div></section>
    <section><h2>MPU6050</h2><div id="imuState">Veri bekleniyor…</div>
      <table>
        <tr><td>İvme X / Y / Z</td><td id="accel">—</td></tr>
        <tr><td>Jiroskop X / Y / Z</td><td id="gyro">—</td></tr>
        <tr><td>Sıcaklık</td><td id="temp">—</td></tr>
        <tr><td>Çalışma süresi</td><td id="uptime">—</td></tr>
      </table></section>
    <section><h2>Motor çıkışları</h2>
      <p class="status">GÜVENLİ MOD: Motorlar yazılımla sürülmüyor.</p>
      <div class="motors" id="motors"></div>
      <small>GPIO çıkışları açılışta LOW seviyesine çekilir. Pervaneleri sökün.</small>
    </section>
  </div>
  <script>
    const fmt = (v, n = 2) => Number(v).toFixed(n);
    const streamUrl = new URL('/stream', location.href);
    streamUrl.port = '81';
    document.getElementById('camera').src = streamUrl;
    async function refresh() {
      try {
        const r = await fetch('/api/status', { cache: 'no-store' });
        const s = await r.json();
        document.getElementById('cameraState').textContent = s.camera ? 'Kamera hazır' : 'Kamera başlatılamadı';
        document.getElementById('cameraState').className = s.camera ? 'status' : 'error';
        const imu = document.getElementById('imuState');
        imu.textContent = s.mpu6050 ? 'MPU6050 bağlı' : 'MPU6050 bulunamadı';
        imu.className = s.mpu6050 ? 'status' : 'error';
        document.getElementById('accel').textContent = s.mpu6050
          ? `${fmt(s.accel_g.x)} / ${fmt(s.accel_g.y)} / ${fmt(s.accel_g.z)} g` : '—';
        document.getElementById('gyro').textContent = s.mpu6050
          ? `${fmt(s.gyro_dps.x)} / ${fmt(s.gyro_dps.y)} / ${fmt(s.gyro_dps.z)} °/s` : '—';
        document.getElementById('temp').textContent = s.mpu6050 ? `${fmt(s.temp_c)} °C` : '—';
        document.getElementById('uptime').textContent = `${Math.floor(s.uptime_ms / 1000)} sn`;
        document.getElementById('motors').innerHTML = s.motors.map(m =>
          `<div class="motor">Motor ${m.number}<br><span class="${m.active ? 'error' : 'status'}">` +
          `${m.active ? 'AKTİF' : 'KAPALI'}</span> <small>(GPIO ${m.pin}, LOW kilitli)</small></div>`
        ).join('');
      } catch (_) {
        document.getElementById('imuState').textContent = 'Kartla bağlantı kesildi';
        document.getElementById('imuState').className = 'error';
      }
    }
    refresh();
    setInterval(refresh, 500);
  </script>
</body>
</html>
)HTML";

bool writeMpuRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU6050_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readMpuData(int16_t values[7]) {
  Wire.beginTransmission(MPU6050_ADDRESS);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(MPU6050_ADDRESS, static_cast<uint8_t>(14)) != 14) {
    return false;
  }

  for (size_t i = 0; i < 7; ++i) {
    const uint8_t high = Wire.read();
    const uint8_t low = Wire.read();
    values[i] = static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | low);
  }
  return true;
}

void initializeMotorSafety() {
  for (const int pin : MOTOR_PINS) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
}

bool initializeCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = psramFound() ? 2 : 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  return esp_camera_init(&config) == ESP_OK;
}

esp_err_t indexHandler(httpd_req_t *request) {
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  return httpd_resp_send(request, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t statusHandler(httpd_req_t *request) {
  int16_t raw[7] = {};
  const bool sensorReadOk = mpu6050Ready && readMpuData(raw);
  char json[768];
  const int length = snprintf(
      json, sizeof(json),
      "{\"camera\":%s,\"mpu6050\":%s,\"uptime_ms\":%lu,"
      "\"accel_g\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
      "\"temp_c\":%.2f,"
      "\"gyro_dps\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
      "\"motors\":["
      "{\"number\":1,\"pin\":%d,\"active\":%s},"
      "{\"number\":2,\"pin\":%d,\"active\":%s},"
      "{\"number\":3,\"pin\":%d,\"active\":%s},"
      "{\"number\":4,\"pin\":%d,\"active\":%s}]}",
      cameraReady ? "true" : "false",
      sensorReadOk ? "true" : "false",
      static_cast<unsigned long>(millis()),
      sensorReadOk ? raw[0] / 16384.0 : 0.0,
      sensorReadOk ? raw[1] / 16384.0 : 0.0,
      sensorReadOk ? raw[2] / 16384.0 : 0.0,
      sensorReadOk ? (raw[3] / 340.0) + 36.53 : 0.0,
      sensorReadOk ? raw[4] / 131.0 : 0.0,
      sensorReadOk ? raw[5] / 131.0 : 0.0,
      sensorReadOk ? raw[6] / 131.0 : 0.0,
      MOTOR_PINS[0], digitalRead(MOTOR_PINS[0]) == HIGH ? "true" : "false",
      MOTOR_PINS[1], digitalRead(MOTOR_PINS[1]) == HIGH ? "true" : "false",
      MOTOR_PINS[2], digitalRead(MOTOR_PINS[2]) == HIGH ? "true" : "false",
      MOTOR_PINS[3], digitalRead(MOTOR_PINS[3]) == HIGH ? "true" : "false");

  if (length < 0 || static_cast<size_t>(length) >= sizeof(json)) {
    return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "Status response too large");
  }
  httpd_resp_set_type(request, "application/json");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, json, length);
}

esp_err_t streamHandler(httpd_req_t *request) {
  if (!cameraReady) {
    httpd_resp_set_status(request, "503 Service Unavailable");
    return httpd_resp_send(request, "Camera is not available", HTTPD_RESP_USE_STRLEN);
  }

  static const char boundary[] = "\r\n--ostrijhframe\r\n";
  httpd_resp_set_type(request, "multipart/x-mixed-replace;boundary=ostrijhframe");

  while (true) {
    camera_fb_t *frame = esp_camera_fb_get();
    if (frame == nullptr) {
      return ESP_FAIL;
    }

    char partHeader[96];
    const int headerLength = snprintf(partHeader, sizeof(partHeader),
                                      "Content-Type: image/jpeg\r\n"
                                      "Content-Length: %u\r\n\r\n",
                                      static_cast<unsigned int>(frame->len));
    const esp_err_t boundaryResult =
        httpd_resp_send_chunk(request, boundary, sizeof(boundary) - 1);
    const esp_err_t headerResult =
        boundaryResult == ESP_OK
            ? httpd_resp_send_chunk(request, partHeader, headerLength)
            : boundaryResult;
    const esp_err_t frameResult =
        headerResult == ESP_OK
            ? httpd_resp_send_chunk(request,
                                    reinterpret_cast<const char *>(frame->buf),
                                    frame->len)
            : headerResult;
    esp_camera_fb_return(frame);

    if (frameResult != ESP_OK) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }
  return ESP_OK;
}

bool startWebServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_uri_handlers = 3;
  config.stack_size = 8192;

  if (httpd_start(&server, &config) != ESP_OK) {
    return false;
  }

  const httpd_uri_t indexUri = {
      .uri = "/", .method = HTTP_GET, .handler = indexHandler, .user_ctx = nullptr};
  const httpd_uri_t statusUri = {
      .uri = "/api/status", .method = HTTP_GET, .handler = statusHandler, .user_ctx = nullptr};
  if (httpd_register_uri_handler(server, &indexUri) != ESP_OK ||
      httpd_register_uri_handler(server, &statusUri) != ESP_OK) {
    return false;
  }

  httpd_config_t streamConfig = HTTPD_DEFAULT_CONFIG();
  streamConfig.server_port = 81;
  streamConfig.ctrl_port = config.ctrl_port + 1;
  streamConfig.stack_size = 8192;
  if (httpd_start(&streamServer, &streamConfig) != ESP_OK) {
    return false;
  }
  const httpd_uri_t streamUri = {
      .uri = "/stream", .method = HTTP_GET, .handler = streamHandler, .user_ctx = nullptr};
  return httpd_register_uri_handler(streamServer, &streamUri) == ESP_OK;
}

}  // namespace

void setup() {
  initializeMotorSafety();

  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  mpu6050Ready = writeMpuRegister(0x6B, 0x00);

  cameraReady = initializeCamera();

  WiFi.mode(WIFI_AP);
  const bool accessPointReady = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, 2);
  const bool webServerReady = accessPointReady && startWebServer();

  Serial.println();
  Serial.println("ostrijh masaustu test firmware");
  Serial.printf("Kamera: %s\n", cameraReady ? "hazir" : "baslatilamadi");
  Serial.printf("MPU6050: %s\n", mpu6050Ready ? "hazir" : "bulunamadi");
  Serial.printf("Wi-Fi AP: %s\n", accessPointReady ? AP_SSID : "baslatilamadi");
  Serial.printf("Panel: %s\n", webServerReady ? "http://192.168.4.1/" : "baslatilamadi");
  Serial.println("Motor pinleri guvenlik icin LOW seviyesinde tutuluyor.");
}

void loop() {
  delay(1000);
}
