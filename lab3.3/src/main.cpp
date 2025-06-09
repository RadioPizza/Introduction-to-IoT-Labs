#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DHTesp.h>

// Пины подключения
#define RED_LED_PIN D0
#define GREEN_LED_PIN D1
#define BLUE_LED_PIN D2
#define SWITCH_PIN D3
#define DHT11_PIN D7
#define FAN_PIN D8
#define LIGHT_SENSOR_PIN A0

// Настройки Wi-Fi
const char* WIFI_SSID = "ssid";
const char* WIFI_PASS = "password";

// Настройки Node-RED
const char* SERVER_URL = "http://192.168.2.38:1880/sensors";

DHTesp dht;

// Глобальные переменные
float currentTemp = 0.0;
int lightPercent = 0;
String ledStatus = "green";
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 2000;

void setup() {
  Serial.begin(115200);
  delay(10);
  
  // Инициализация пинов
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  // Выключаем светодиод (общий анод)
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(BLUE_LED_PIN, HIGH);
  
  // Инициализация датчиков
  dht.setup(DHT11_PIN, DHTesp::DHT11);
  
  // Подключение к Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Подключение к ");
  Serial.print(WIFI_SSID);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nПодключено! IP адрес: ");
  Serial.println(WiFi.localIP());
}

void updateSystemState() {
  bool switchState = !digitalRead(SWITCH_PIN);
  float newTemp = dht.getTemperature();
  
  if (!isnan(newTemp)) {
    currentTemp = newTemp;
    
    // Чтение освещенности
    int lightValue = analogRead(LIGHT_SENSOR_PIN);
    lightPercent = map(lightValue, 0, 1023, 0, 100);

    // Логика управления
    if (currentTemp <= 32.0) {
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(BLUE_LED_PIN, HIGH);
      ledStatus = "green";
      analogWrite(FAN_PIN, 0);
    } 
    else if (currentTemp > 32.0 && currentTemp <= 42.0) {
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(BLUE_LED_PIN, LOW);
      ledStatus = "blue";
      analogWrite(FAN_PIN, switchState ? 512 : 0);
    } 
    else {
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, HIGH);
      ledStatus = "red";
      analogWrite(FAN_PIN, switchState ? 1023 : 0);
    }
  }
}

void sendDataToServer() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  WiFiClient client;
  HTTPClient http;
  http.begin(client, SERVER_URL);  // Исправленная строка
  http.addHeader("Content-Type", "application/json");
  
  bool switchState = !digitalRead(SWITCH_PIN);
  int fanSpeed = 0;
  
  if (ledStatus == "blue" && switchState) fanSpeed = 50;
  else if (ledStatus == "red" && switchState) fanSpeed = 100;
  
  String json = "{";
  json += "\"temperature\":" + String(currentTemp, 1) + ",";
  json += "\"light\":" + String(lightPercent) + ",";
  json += "\"status\":\"" + ledStatus + "\",";
  json += "\"fan_speed\":" + String(fanSpeed) + ",";
  json += "\"switch_state\":";
  json += switchState ? "true" : "false";
  json += "}";

  int httpCode = http.POST(json);
  
  if (httpCode > 0) {
    Serial.printf("Данные отправлены. Код: %d\n", httpCode);
  } else {
    Serial.printf("Ошибка отправки: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastUpdate > UPDATE_INTERVAL) {
    updateSystemState();
    sendDataToServer();
    lastUpdate = currentMillis;
    
    // Вывод в монитор порта для отладки
    bool switchState = !digitalRead(SWITCH_PIN);
    int fanPercent = 0;
    if (ledStatus == "blue" && switchState) fanPercent = 50;
    else if (ledStatus == "red" && switchState) fanPercent = 100;
    
    Serial.printf("Temp: %.1fC | Light: %d%% | Status: %s | Fan: %d%%\n", 
                 currentTemp, lightPercent, ledStatus.c_str(), fanPercent);
  }
}