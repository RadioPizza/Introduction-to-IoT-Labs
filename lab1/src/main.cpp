#include <Arduino.h>

#define ESP8266

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#else
#include <WiFi.h>
#include <WebServer.h>
#endif


#define SSID "MDO"
#define PASSWORD "12345678"

#include <Adafruit_NeoPixel.h>

// Настройки для матрицы 8x8
#define LED_PIN    4      // Пин, к которому подключена матрица
#define LED_COUNT  256      // Количество светодиодов (8x8 = 64)

// Создаем объект для управления матрицей
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);



IPAddress local_ip(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
ESP8266WebServer server(80);

// Пин для встроенного светодиода
#define LED_PIN 2  // GPIO2 (D4 на NodeMCU)

uint8_t LED_status = 0;  // Переменная для состояния светодиода (0 — выключен, 1 — включен)

// Прототипы функций
void handle_OnConnect();
void handle_led_on();
void handle_led_off();
void handle_NotFound();
String SendHTML(uint8_t led_stat);

void setup() {

  strip.begin();           // Инициализация матрицы
  strip.show();            // Очистка матрицы (все светодиоды выключены)
  strip.setBrightness(50); // Установка яркости (от 0 до 255)

  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Выключаем светодиод при старте

  // Установка режима Wi-Fi в режим точки доступа
  WiFi.mode(WIFI_AP);

  // Запуск точки доступа
  bool result = WiFi.softAP(SSID, PASSWORD);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);

  if (result) {
    Serial.println("Точка доступа запущена успешно!");
    Serial.print("IP адрес: ");
    Serial.println(WiFi.softAPIP());  // Вывод IP-адреса точки доступа
  } else {
    Serial.println("Ошибка запуска точки доступа!");
  }

  // Регистрация обработчиков запросов
  server.on("/", handle_OnConnect);      // Главная страница
  server.on("/led_on", handle_led_on);   // Включение светодиода
  server.on("/led_off", handle_led_off); // Выключение светодиода
  server.onNotFound(handle_NotFound);    // Обработка неизвестных запросов

  // Запуск сервера
  server.begin();
  Serial.println("HTTP server started");
}
// Функция для последовательного заполнения матрицы цветом
void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color); // Устанавливаем цвет для каждого светодиода
    strip.show();                  // Обновляем матрицу
    delay(wait);                  // Задержка между светодиодами
  }
}

// Функция для эффекта "бегущий пиксель"
void theaterChase(uint32_t color, int wait) {
  for (int a = 0; a < 10; a++) {  // Повторяем эффект 10 раз
    for (int b = 0; b < 3; b++) { // 3 фазы эффекта
      for (int i = 0; i < strip.numPixels(); i += 3) {
        strip.setPixelColor(i + b, color); // Включаем каждый 3-й светодиод
      }
      strip.show();
      delay(wait);
      for (int i = 0; i < strip.numPixels(); i += 3) {
        strip.setPixelColor(i + b, 0);     // Выключаем каждый 3-й светодиод
      }
    }
  }
}

void loop() {

  // Пример 1: Заполнение матрицы красным цветом
  colorWipe(strip.Color(255, 0, 0), 5); // Красный
  delay(1000);

  // Пример 2: Заполнение матрицы зелёным цветом
  colorWipe(strip.Color(0, 255, 0), 5); // Зелёный
  delay(1000);

  // Пример 3: Заполнение матрицы синим цветом
  colorWipe(strip.Color(0, 0, 255), 5); // Синий
  delay(1000);


  server.handleClient();  // Обработка клиентских запросов
  digitalWrite(LED_PIN, LED_status ? HIGH : LOW);  // Управление светодиодом
}

// Функция для генерации HTML-страницы
String SendHTML(uint8_t led_stat) {
  String html = "<!DOCTYPE html>\
  <html>\
  <head>\
    <meta charset=\"UTF-8\">\
    <title>LED Control server</title>\
    <style>\
      html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; }\
      body { margin-top: 50px; }\
      h1 { color: #444444; margin: 50px auto 30px; }\
      h3 { color: #444444; margin-bottom: 50px; }\
      .button { display: block; width: 80px; background-color: #3498db; border: none; color: white; padding: 13px 30px; text-decoration: none; font-size: 18px; margin: 0px auto 35px; cursor: pointer; border-radius: 4px; }\
      .button-on { background-color: #3498db; }\
      .button-on:active { background-color: #2980b9; }\
      .button-off { background-color: #34495e; }\
      .button-off:active { background-color: #2c3e50; }\
      p { font-size: 14px; color: #888; margin-bottom: 10px; }\
    </style>\
  </head>\
  <body>\
    <h1>ESP8266 Веб сервер</h1>\
    <h3>Режим точка доступа WiFi (AP)</h3>";

  if (led_stat) {
    html += "<p>Состояние LED: ВКЛ.</p><a class=\"button button-off\" href=\"/led_off\">ВЫКЛ.</a>";
  } else {
    html += "<p>Состояние LED: ВЫКЛ.</p><a class=\"button button-on\" href=\"/led_on\">ВКЛ.</a>";
  }

  html += "</body></html>";
  return html;
}

// Обработчик главной страницы
void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(LED_status));
}

// Обработчик для включения светодиода
void handle_led_on() {
  LED_status = 1;
  server.send(200, "text/html", SendHTML(LED_status));
}

// Обработчик для выключения светодиода
void handle_led_off() {
  LED_status = 0;
  server.send(200, "text/html", SendHTML(LED_status));
}

// Обработчик для неизвестных запросов
void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

