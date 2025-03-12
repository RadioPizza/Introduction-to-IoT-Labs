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

// Настройки сети
#define SSID "Matrix controller 1A22"
#define PASSWORD "12345678"

// Встроенный светодиод
#define STATUS_LED_PIN 2

// Настройки матрицы
#define LED_PIN     4
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define LED_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)
#define MATRIX_ZIGZAG

// Настройки анимации
#define NUM_COLUMNS 4
#define DEFAULT_SPEED 100
#define MIN_SPEED 50
#define MAX_SPEED 1000

// Валидация количества логических столбцов
#if NUM_COLUMNS < 2 || NUM_COLUMNS > MATRIX_WIDTH
  #error "NUM_COLUMNS must be between 2 and MATRIX_WIDTH"
#endif

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

IPAddress local_ip(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
ESP8266WebServer server(80); // порт 80 - для HTTP сервера

uint8_t LED_status = 0;
bool isAnimating = false;
bool animationDirection = true; // true = вправо, false = влево
uint32_t animationSpeed = DEFAULT_SPEED;
int32_t currentOffset = 0;
uint32_t previousMillis = 0;
uint32_t columnColors[NUM_COLUMNS] = { 
  strip.Color(255, 0, 0),   // Красный столбец
  strip.Color(0, 255, 0),   // Зеленый столбец
};

// Прототипы функций
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
  Serial.println("HTTP сервер запущен");
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
  // Проходим по всем пикселям матрицы
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    // Вычисляем координаты текущего пикселя в матрице
    // row - номер строки (от 0 до MATRIX_HEIGHT - 1)
    // col - номер столбца (от 0 до MATRIX_WIDTH - 1)
    uint8_t row = i / MATRIX_WIDTH;    // Номер строки
    uint8_t col = i % MATRIX_WIDTH;    // Номер столбца

    // Коррекция направления для зигзагообразных LED-матриц
    #ifdef MATRIX_ZIGZAG
      if (row % 2 != 0) {
        col = (MATRIX_WIDTH - 1) - col;  // Инвертируем столбец для нечетных строк
      }
    #endif

    // Определяем принадлежность текущего столбца к логическому столбцу
    // Логический столбец - это группа физических столбцов, объединенных для анимации
    uint8_t logicalCol = col / (MATRIX_WIDTH / NUM_COLUMNS);

    // Вычисляем индекс цвета для текущего логического столбца с учетом смещения (анимации)
    // currentOffset - это текущее смещение анимации
    // NUM_COLUMNS - количество логических столбцов
    uint8_t colorIndex = (logicalCol + currentOffset) % NUM_COLUMNS;

    // Транспонирование матрицы: преобразуем координаты (row, col) в (col, row)
    // Это нужно для правильного отображения
    // newIndex - это индекс пикселя в линейном массиве strip, соответствующий новым координатам
    uint16_t newIndex = col * MATRIX_HEIGHT + row;

    // Устанавливаем цвет для пикселя с индексом newIndex
    // columnColors[colorIndex] - цвет из массива, соответствующий текущему логическому столбцу
    strip.setPixelColor(newIndex, columnColors[colorIndex]);
  }

  // Обновляем матрицу, чтобы применить изменения
  strip.show();
}

// Обработчики HTTP-запросов
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
  String html;
  html.reserve(1024); // Резервируем память
  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><title>LED Matrix Control</title>";
  html += "</head><body>";
  html += "<h1>LED Matrix Controller</h1>";
  
  // Форма для обновления параметров
  html += "<form action='/update' method='GET'>";
  for (uint8_t i = 0; i < NUM_COLUMNS; i++) {
    html += "<div>";
    html += "Column " + String(i+1) + ": ";
    html += "<input type='color' name='c" + String(i) + "'";
    html += " value='" + colorToString(columnColors[i]) + "'>";
    html += "</div>";
  }
  
  // Поле для настройки скорости анимации
  html += "<div>";
  html += "Speed: <input type='range' name='speed'";
  html += " min='" + String(MIN_SPEED) + "' max='" + String(MAX_SPEED) + "'";
  html += " value='" + String(animationSpeed) + "'>";
  html += "</div>";
  
  // Поле для настройки яркости
  html += "<div>";
  html += "Brightness: <input type='range' name='brightness'";
  html += " min='0' max='255'";
  html += " value='" + String(strip.getBrightness()) + "'>";
  html += "</div>";
  
  // Поле для выбора направления анимации
  html += "<div>";
  html += "Direction: ";
  html += "<label><input type='radio' name='direction' value='right' " + String(animationDirection ? "checked" : "") + "> Right</label>";
  html += "<label><input type='radio' name='direction' value='left' " + String(!animationDirection ? "checked" : "") + "> Left</label>";
  html += "</div>";
  
  // Кнопка для отправки формы
  html += "<input type='submit' value='Update'>";
  html += "</form>";
  
  // Кнопки для запуска и остановки анимации
  html += "<button onclick=\"location.href='/run'\">Run</button>";
  html += "<button onclick=\"location.href='/stop'\">Stop</button>";
  
  // Отображение текущего статуса и скорости анимации
  html += "<p>Status: " + String(isAnimating ? "Running" : "Stopped") + "</p>";
  html += "<p>Speed Level: " + String(animationSpeed) + " / 1000</p>";
  html += "</body></html>";
  
  return html;
}

// Утилиты для работы с цветом

// Преобразует строку с цветом в формате "#RRGGBB" в 32-битное значение цвета
uint32_t parseColor(const String& colorStr) {
  // Проверяем, что строка имеет правильный формат:
  // Длина строки должна быть 7 символов (включая '#'), и первый символ должен быть '#'
  if (colorStr.length() != 7 || colorStr[0] != '#') return 0;

  // Преобразуем строку в 32-битное число:
  // Убираем первый символ '#' с помощью substring(1)
  // Преобразуем оставшуюся строку в число с помощью strtoul (строка в unsigned long)
  // Основание системы счисления — 16
  uint32_t rgb = strtoul(colorStr.substring(1).c_str(), NULL, 16);

  // Разделяем число на каналы R, G, B и создаем цвет с помощью strip.Color:
  // - (rgb >> 16) & 0xFF: извлекаем красный канал (первые два символа после '#')
  // - (rgb >> 8) & 0xFF: извлекаем зеленый канал (средние два символа)
  // - rgb & 0xFF: извлекаем синий канал (последние два символа)
  return strip.Color(
    (rgb >> 16) & 0xFF,  // Красный канал
    (rgb >> 8) & 0xFF,   // Зеленый канал
    rgb & 0xFF           // Синий канал
  );
}

// Преобразует 32-битное значение цвета в строку формата "#RRGGBB"
String colorToString(uint32_t color) {
  // Извлекаем каналы R, G, B из 32-битного значения цвета:
  // - (color >> 16) & 0xFF: извлекаем красный канал
  // - (color >> 8) & 0xFF: извлекаем зеленый канал
  // - color & 0xFF: извлекаем синий канал
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  // Преобразуем каждый канал в шестнадцатеричную строку и объединяем их:
  // - byteToHex(r): преобразует красный канал в шестнадцатеричный формат
  // - byteToHex(g): преобразует зеленый канал в шестнадцатеричный формат
  // - byteToHex(b): преобразует синий канал в шестнадцатеричный формат
  return "#" + byteToHex(r) + byteToHex(g) + byteToHex(b);
}

// Преобразует 8-битное значение в шестнадцатеричную строку из двух символов
String byteToHex(uint8_t value) {
  // Массив символов для шестнадцатеричных цифр
  const char* hexDigits = "0123456789ABCDEF";

  // Преобразуем значение в шестнадцатеричный формат:
  // - value >> 4: извлекаем старшую половину байта (первые 4 бита)
  // - value & 0x0F: извлекаем младшую половину байта (последние 4 бита)
  // - Используем массив hexDigits для преобразования чисел в символы
  return String(hexDigits[value >> 4]) + String(hexDigits[value & 0x0F]);
}