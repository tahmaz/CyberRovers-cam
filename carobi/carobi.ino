#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include "variables.h"
#include "webpage.h"

// Wi-Fi credentials
const char* WIFI_SSID = "SEON";
const char* WIFI_PASS = "asd123567";

// I2C pins for OLED
#define I2C_SDA 15
#define I2C_SCL 14
TwoWire I2Cbus = TwoWire(0);

// Servo pins
Servo headServo;   // GPIO12
Servo leftServo;   // GPIO13
Servo rightServo;  // GPIO4
#define HEAD_SERVO_PIN 12
#define LEFT_SERVO_PIN 13
#define RIGHT_SERVO_PIN 2

// Servo position limits and tracking
#define HEAD_MIN_ANGLE 0
#define HEAD_MAX_ANGLE 180
int currentHeadAngle = 30;  // Starting at middle position

// Display defines
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2Cbus, OLED_RESET);

// Web server
WebServer server(80);

// Camera resolution
static auto hiRes = esp32cam::Resolution::find(800, 600);

// Emoji bitmaps from variables.h
const int bitmaps_len = 4;
const unsigned char* bitmaps[bitmaps_len] = {
    neutral,
    neutral_right,
    neutral_left,
    blink_low
};

bool isMoving = false;
unsigned long moveStartTime = 0;
int moveduration = 300;
volatile bool streamActive = false;

// MJPEG stream task
void streamTask(void *pvParameters) {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected() && streamActive) {
    auto frame = esp32cam::capture();
    if (frame == nullptr) {
      Serial.println("STREAM CAPTURE FAIL");
      continue;
    }

    String header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
    header += frame->size();
    header += "\r\n\r\n";
    client.print(header);
    client.write(frame->data(), frame->size());
    client.print("\r\n");
    delay(50); // ~20 FPS, adjust as needed
  }
  streamActive = false;
  vTaskDelete(NULL);
}

void startStream() {
  if (!streamActive) {
    streamActive = true;
    xTaskCreate(streamTask, "StreamTask", 8192, NULL, 1, NULL);
  }
}

void handleSnapshot() {
  // Capture the frame
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("SNAPSHOT CAPTURE FAIL");
    server.send(500, "text/plain", "Failed to capture image");
    return;
  }

  // Ensure client is still connected
  WiFiClient client = server.client();
  if (!client.connected()) {
    Serial.println("Client disconnected before sending snapshot");
    return;
  }

  // Send HTTP response headers
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: image/jpeg\r\n");
  client.print("Content-Length: ");
  client.println(frame->size());
  client.print("Content-Disposition: inline; filename=\"snapshot.jpg\"\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");

  // Send the image data
  size_t bytesWritten = client.write(frame->data(), frame->size());
  if (bytesWritten != frame->size()) {
    Serial.printf("Error: Only wrote %d of %d bytes\n", bytesWritten, frame->size());
  } else {
    Serial.println("Snapshot captured and sent successfully");
  }
// Flush the client buffer and close connection
  client.flush();
  client.stop();
}

void serveHtml() {
  server.send(200, "text/html", htmlPage);
}

void handleOled1() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Emoji 1 :)");
  display.display();
  Serial.println("Emoji 1 Command Received");
  server.send(200, "text/plain", "OLED updated: Emoji 1");
}

void handleOled2() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Emoji 2 :(");
  display.display();
  Serial.println("Emoji 2 Command Received");
  server.send(200, "text/plain", "OLED updated: Emoji 2");
}

void handleSendText() {
  if (server.method() == HTTP_POST) {
    String text = server.arg("text");
    if (text.length() > 0) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println(text.substring(0, 21));
      if (text.length() > 21) {
        display.println(text.substring(21, 42));
      }
      display.display();
      Serial.println("Text Command Received: " + text);
      server.send(200, "text/plain", "OLED updated: " + text);
    } else {
      server.send(400, "text/plain", "No text provided");
    }
  } else {
    server.send(405, "text/plain", "Method not allowed");
  }
}

void handleHeadUp() {
  int newAngle = currentHeadAngle + 10;
  if (newAngle <= HEAD_MAX_ANGLE) {
    currentHeadAngle = newAngle;
    headServo.write(currentHeadAngle);
    Serial.printf("Head Up to %d degrees\n", currentHeadAngle);
    server.send(200, "text/plain", "Head tilted up to " + String(currentHeadAngle) + " degrees");
  } else {
    Serial.println("Head at maximum angle");
    server.send(200, "text/plain", "Head at maximum angle (" + String(HEAD_MAX_ANGLE) + " degrees)");
  }
}

void handleHeadDown() {
  int newAngle = currentHeadAngle - 10;
  if (newAngle >= HEAD_MIN_ANGLE) {
    currentHeadAngle = newAngle;
    headServo.write(currentHeadAngle);
    Serial.printf("Head Down to %d degrees\n", currentHeadAngle);
    server.send(200, "text/plain", "Head tilted down to " + String(currentHeadAngle) + " degrees");
  } else {
    Serial.println("Head at minimum angle");
    server.send(200, "text/plain", "Head at minimum angle (" + String(HEAD_MIN_ANGLE) + " degrees)");
  }
}

void handleMoveForward() {
  moveduration = 200;
  isMoving = true;
  moveStartTime = millis();
  leftServo.attach(LEFT_SERVO_PIN);
  rightServo.attach(RIGHT_SERVO_PIN);
  leftServo.write(0);
  rightServo.write(180);
  Serial.println("Move Forward Command Received");
  server.send(200, "text/plain", "Moving forward");
}

void handleMoveBackward() {
  moveduration = 200;
  isMoving = true;
  moveStartTime = millis();
  leftServo.attach(LEFT_SERVO_PIN);
  rightServo.attach(RIGHT_SERVO_PIN);
  leftServo.write(180);
  rightServo.write(0);
  Serial.println("Move Backward Command Received");
  server.send(200, "text/plain", "Moving backward");
}

void handleRotateLeft() {
  moveduration = 30;
  isMoving = true;
  moveStartTime = millis();
  leftServo.attach(LEFT_SERVO_PIN);
  rightServo.attach(RIGHT_SERVO_PIN);
  leftServo.write(0);
  rightServo.write(0);
  Serial.println("Rotate Left Command Received");
  server.send(200, "text/plain", "Rotating left");
}

void handleRotateRight() {
  moveduration = 30;
  isMoving = true;
  moveStartTime = millis();
  leftServo.attach(LEFT_SERVO_PIN);
  rightServo.attach(RIGHT_SERVO_PIN);
  leftServo.write(180);
  rightServo.write(180);
  Serial.println("Rotate Right Command Received");
  server.send(200, "text/plain", "Rotating right");
}

void setup() {
  Serial.begin(115200);

  // Initialize I2C
  I2Cbus.begin(I2C_SDA, I2C_SCL, 100000);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 OLED display failed to initialize.");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Starting...");
  display.display();

  // Camera initialization
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);
    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
    // Display camera status
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("Camera: ");
    display.println(ok ? "CAMERA OK" : "CAMERA FAIL");
    display.display();
  }

  // Wi-Fi initialization
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Display IP address
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(3000);

  Serial.print("http://");
  Serial.println(WiFi.localIP());

  // Web server routes
  server.on("/", serveHtml);
  server.on("/stream", startStream);
  server.on("/snapshot", handleSnapshot);
  server.on("/oled1", handleOled1);
  server.on("/oled2", handleOled2);
  server.on("/sendText", HTTP_POST, handleSendText);
  server.on("/up", handleHeadUp);
  server.on("/down", handleHeadDown);
  server.on("/movef", handleMoveForward);
  server.on("/moveb", handleMoveBackward);
  server.on("/movel", handleRotateLeft);
  server.on("/mover", handleRotateRight);

  server.begin();

  // Servo initialization
  headServo.attach(HEAD_SERVO_PIN);
  leftServo.attach(LEFT_SERVO_PIN);
  rightServo.attach(RIGHT_SERVO_PIN);
  
  // Set initial head position
  headServo.write(currentHeadAngle);
}

void loop() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    display.clearDisplay();
    int randomIndex = random(bitmaps_len);
    display.drawBitmap(0, 0, bitmaps[randomIndex], SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();
  }

  server.handleClient();

  if (isMoving && millis() - moveStartTime >= moveduration) {
    leftServo.detach();
    rightServo.detach();
    isMoving = false;
  }
}