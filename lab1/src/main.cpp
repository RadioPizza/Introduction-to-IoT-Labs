#include <Arduino.h>
#include <stdint.h>

#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
#else
  #include <WiFi.h>
  #include <WebServer.h>
#endif

#include <Adafruit_NeoPixel.h>

#define SSID "MDO"
#define PASSWORD "12345678"

#define STATUS_LED_PIN 2

// LED Matrix settings
#define LED_PIN     4
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define LED_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

// Animation settings
#define NUM_COLUMNS 4
#define DEFAULT_SPEED 100
#define MIN_SPEED 50
#define MAX_SPEED 1000

#if NUM_COLUMNS < 2 || NUM_COLUMNS > MATRIX_WIDTH
  #error "NUM_COLUMNS must be between 2 and MATRIX_WIDTH"
#endif

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

IPAddress local_ip(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
ESP8266WebServer server(80);

// Control variables
uint8_t LED_status = 0;
uint32_t columnColors[NUM_COLUMNS] = { 
  strip.Color(255, 0, 0),   // Red
  strip.Color(0, 255, 0),   // Green
};

uint32_t animationSpeed = DEFAULT_SPEED;
bool isAnimating = false;
bool animationDirection = true; // true = right, false = left
int32_t currentOffset = 0;
uint32_t previousMillis = 0;

// Function prototypes
void handleRoot();
void handleUpdate();
void handleRun();
void handleStop();
void handleNotFound();
String generateHTML();
void updateMatrix();
uint32_t parseColor(const String& colorStr);
String colorToString(uint32_t color);
String byteToHex(uint8_t value);

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  strip.begin();
  strip.setBrightness(50);
  updateMatrix();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(SSID, PASSWORD);

  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.on("/run", handleRun);
  server.on("/stop", handleStop);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  digitalWrite(STATUS_LED_PIN, LED_status ? HIGH : LOW);

  uint32_t currentMillis = millis();
  if (isAnimating) {
    uint32_t currentDelay = 50000 / animationSpeed; // Преобразуем скорость в задержку
    if (currentMillis - previousMillis >= currentDelay) {
      previousMillis = currentMillis;
      currentOffset = (currentOffset + (animationDirection ? 1 : -1) + NUM_COLUMNS) % NUM_COLUMNS;
      updateMatrix();
    }
  }
}

void updateMatrix() {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    // Определяем координаты пикселя
    uint8_t row = i / MATRIX_WIDTH;    // Номер строки
    uint8_t col = i % MATRIX_WIDTH;    // Номер столбца

    // Коррекция направления для зигзагообразных матриц
    if (row % 2 != 0) {
      col = (MATRIX_WIDTH - 1) - col;
    }

    // Определяем принадлежность к логическому столбцу
    uint8_t logicalCol = col / (MATRIX_WIDTH / NUM_COLUMNS);
    
    // Применяем анимацию с учетом смещения
    uint8_t colorIndex = (logicalCol + currentOffset) % NUM_COLUMNS;
    
    // Рассчитываем новый индекс пикселя для вертикального отображения
    uint16_t newIndex = col * MATRIX_HEIGHT + row;
    
    strip.setPixelColor(newIndex, columnColors[colorIndex]);
  }
  strip.show();
}

// HTTP Handlers
void handleRoot() {
  server.send(200, "text/html", generateHTML());
}

void handleUpdate() {
  for (uint8_t i = 0; i < NUM_COLUMNS; i++) {
    String param = "c" + String(i);
    if (server.hasArg(param)) {
      columnColors[i] = parseColor(server.arg(param));
    }
  }
  
  if (server.hasArg("speed")) {
    animationSpeed = constrain(server.arg("speed").toInt(), MIN_SPEED, MAX_SPEED);
  }
  
  if (server.hasArg("brightness")) {
    uint8_t brightness = server.arg("brightness").toInt();
    brightness = constrain(brightness, 0, 255); // Ограничиваем значение от 0 до 255
    strip.setBrightness(brightness);
  }

  if (server.hasArg("direction")) {
    animationDirection = (server.arg("direction") == "right");
  }
  
  updateMatrix();
  handleRoot();
}

void handleRun() {
  isAnimating = true;
  handleRoot();
}

void handleStop() {
  isAnimating = false;
  handleRoot();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

String generateHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><title>LED Matrix Control</title>";
  html += "</head><body>";
  html += "<h1>LED Matrix Controller</h1>";
  
  html += "<form action='/update' method='GET'>";
  for (uint8_t i = 0; i < NUM_COLUMNS; i++) {
    html += "<div>";
    html += "Column " + String(i+1) + ": ";
    html += "<input type='color' name='c" + String(i) + "'";
    html += " value='" + colorToString(columnColors[i]) + "'>";
    html += "</div>";
  }
  
  html += "<div>";
  html += "Speed: <input type='range' name='speed'";
  html += " min='" + String(MIN_SPEED) + "' max='" + String(MAX_SPEED) + "'";
  html += " value='" + String(animationSpeed) + "'>";
  html += "</div>";
  
  html += "<div>";
  html += "Brightness: <input type='range' name='brightness'";
  html += " min='0' max='255'";
  html += " value='" + String(strip.getBrightness()) + "'>";
  html += "</div>";
  
  html += "<div>";
  html += "Direction: ";
  html += "<label><input type='radio' name='direction' value='right' " + String(animationDirection ? "checked" : "") + "> Right</label>";
  html += "<label><input type='radio' name='direction' value='left' " + String(!animationDirection ? "checked" : "") + "> Left</label>";
  html += "</div>";
  
  html += "<input type='submit' value='Update'>";
  html += "</form>";
  
  html += "<button onclick=\"location.href='/run'\">Run</button>";
  html += "<button onclick=\"location.href='/stop'\">Stop</button>";
  
  html += "<p>Status: " + String(isAnimating ? "Running" : "Stopped") + "</p>";
  html += "<p>Speed Level: " + String(animationSpeed) + " / 1000</p>";
  html += "</body></html>";
  
  return html;
}

// Color conversion utilities
uint32_t parseColor(const String& colorStr) {
  if (colorStr.length() != 7 || colorStr[0] != '#') return 0;
  
  uint32_t rgb = strtoul(colorStr.substring(1).c_str(), NULL, 16);
  return strip.Color(
    (rgb >> 16) & 0xFF,  // Red
    (rgb >> 8) & 0xFF,   // Green
    rgb & 0xFF           // Blue
  );
}

String colorToString(uint32_t color) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  return "#" + byteToHex(r) + byteToHex(g) + byteToHex(b);
}

String byteToHex(uint8_t value) {
  const char* hexDigits = "0123456789ABCDEF";
  return String(hexDigits[value >> 4]) + String(hexDigits[value & 0x0F]);
}