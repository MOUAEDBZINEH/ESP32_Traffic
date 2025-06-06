#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define LIGHT_PIN 4
#define PWMFreq 1000
#define PWMResolution 8
#define PWMLightChannel 3
#define PWMSpeedChannel 2

const char* apssid = "ESP32-CAM";
const char* appassword = "12345678";

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsControl("/Control");

uint32_t cameraClientId = 0;

const char* htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>ESP32-CAM</title></head>
<body style="text-align:center;">
<h2>ESP32-CAM is running</h2>
</body>
</html>
)rawliteral";

void handleRoot(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", htmlPage);
}

void handleNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

// ✅ التعديل الصحيح للدالة باستخدام AsyncWebServerRequest
void handleControl(AsyncWebServerRequest *request) {
  if (!request->hasArg("cmd")) {
    request->send(400, "text/plain", "Missing command");
    return;
  }

  String cmd = request->arg("cmd");
  int i1 = cmd.indexOf(';');
  int i2 = cmd.indexOf(';', i1 + 1);
  String label = cmd.substring(i1 + 1, i2);

  Serial.println("Received Label: " + label);

  if (label == "GO") {
    digitalWrite(12, HIGH);
    digitalWrite(13, LOW);
    ledcWrite(PWMLightChannel, 0);
  } else if (label == "STOP" || label == "PEDESTRIAN") {
    digitalWrite(12, LOW);
    digitalWrite(13, LOW);
    ledcWrite(PWMLightChannel, 200);
  } else {
    digitalWrite(12, LOW);
    digitalWrite(13, LOW);
    ledcWrite(PWMLightChannel, 0);
  }

  request->send(200, "text/plain", "Command received");
}

void onCameraWebSocket(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    cameraClientId = client->id();
  } else if (type == WS_EVT_DISCONNECT) {
    cameraClientId = 0;
  }
}

void sendCameraFrame() {
  if (cameraClientId == 0) return;
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;
  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;   config.pin_d1 = 18;
  config.pin_d2 = 19;  config.pin_d3 = 21;
  config.pin_d4 = 36;  config.pin_d5 = 39;
  config.pin_d6 = 34;  config.pin_d7 = 35;
  config.pin_xclk = 0; config.pin_pclk = 22;
  config.pin_vsync = 25; config.pin_href = 23;
  config.pin_sscb_sda = 26; config.pin_sscb_scl = 27;
  config.pin_pwdn = 32; config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  esp_camera_init(&config);
}

void setup() {
  Serial.begin(115200);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);

  ledcSetup(PWMSpeedChannel, PWMFreq, PWMResolution);
  ledcSetup(PWMLightChannel, PWMFreq, PWMResolution);
  ledcAttachPin(LIGHT_PIN, PWMLightChannel);
  ledcAttachPin(12, PWMSpeedChannel);

  WiFi.softAP(apssid, appassword);
  Serial.println(WiFi.softAPIP());

  setupCamera();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/control", HTTP_GET, handleControl);
  server.onNotFound(handleNotFound);
  wsCamera.onEvent(onCameraWebSocket);
  server.addHandler(&wsCamera);
  server.begin();
}

void loop() {
  wsCamera.cleanupClients();
  sendCameraFrame();
}
