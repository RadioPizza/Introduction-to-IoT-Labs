#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHTesp.h>

// Пины подключения
#define RED_LED_PIN D0
#define GREEN_LED_PIN D1
#define BLUE_LED_PIN D2
#define SWITCH_PIN D3
#define DHT11_PIN D7
#define FAN_PIN D8
#define LIGHT_SENSOR_PIN A0

// Настройки точки доступа
#define AP_SSID "NodeMCU"
#define AP_PASS "12345678"

ESP8266WebServer server(80);
DHTesp dht;

// Глобальные переменные
float currentTemp = 0.0;
int lightPercent = 0;
String ledStatus = "green";
unsigned long lastDHTUpdate = 0;
const unsigned long DHT_UPDATE_INTERVAL = 1000;
unsigned long lastSerialOutput = 0;
const unsigned long SERIAL_OUTPUT_INTERVAL = 2000;

// HTML страница с веб-интерфейсом
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <title>Система контроля температуры</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { 
      font-family: Arial, sans-serif; 
      text-align: center; 
      margin: 20px;
      background-color: #f5f5f5;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0,0,0,0.1);
    }
    .data-block {
      margin: 20px 0;
      padding: 15px;
      border-radius: 8px;
      background-color: #f0f0f0;
    }
    .temp-value {
      font-size: 28px;
      font-weight: bold;
      margin: 10px 0;
    }
    .status-indicator {
      width: 100px;
      height: 100px;
      border-radius: 50%;
      margin: 0 auto;
      transition: background-color 0.3s;
    }
    .green { background-color:rgb(0, 255, 0); }
    .blue { background-color:rgb(0, 0, 255); }
    .red { background-color:rgb(255, 0, 0); }
    .switch-info {
      margin-top: 15px;
      font-style: italic;
      color: #000000;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Контроль температуры</h1>
    
    <div class="data-block">
      <h2>Текущая температура</h2>
      <div class="temp-value" id="temperature">-- °C</div>
    </div>
    
    <div class="data-block">
      <h2>Освещенность</h2>
      <div class="temp-value" id="light">-- %</div>
    </div>
    
    <div class="data-block">
      <h2>Статус системы</h2>
      <div class="status-indicator" id="statusLed"></div>
      <div class="switch-info">Переключатель кулера: <span id="switchState">активен</span></div>
    </div>
  </div>
  
  <script>
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('temperature').textContent = data.temp + ' °C';
          document.getElementById('light').textContent = data.light + ' %';
          
          // Обновление индикатора статуса
          const indicator = document.getElementById('statusLed');
          indicator.className = 'status-indicator ' + data.status;
          
          // Обновление состояния переключателя
          document.getElementById('switchState').textContent = 
            data.switch_state ? "активен" : "отключен";
        });
    }
    setInterval(updateData, 1000);
  </script>
</body>
</html>
)rawliteral";

void printDebugInfo() {
  bool switchState = !digitalRead(SWITCH_PIN); // Инвертированное состояние
  
  Serial.println("\n===== СИСТЕМНАЯ ИНФОРМАЦИЯ =====");
  Serial.print("Температура: ");
  Serial.print(currentTemp);
  Serial.println(" °C");
  
  Serial.print("Освещенность: ");
  Serial.print(lightPercent);
  Serial.println("%");
  
  Serial.print("Состояние RGB: ");
  Serial.println(ledStatus);
  
  Serial.print("Состояние переключателя: ");
  Serial.println(switchState ? "АКТИВЕН" : "ОТКЛЮЧЕН");
  
  Serial.print("Состояние вентилятора: ");
  if (!switchState) {
    Serial.println("ОТКЛЮЧЕН");
  } else {
    int fanSpeed = 0;
    if (ledStatus == "blue") fanSpeed = 512;
    else if (ledStatus == "red") fanSpeed = 1023;
    Serial.print(fanSpeed);
    Serial.println("/1023");
  }
  
  Serial.print("WiFi клиентов: ");
  Serial.println(WiFi.softAPgetStationNum());
  
  Serial.print("Свободная память: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" байт");
  
  Serial.println("===============================\n");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Ожидание инициализации последовательного порта
  }
  Serial.println("\n\nИнициализация системы...");
  
  // Инициализация пинов
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);

  // Настройка диапазона
  analogWriteRange(1023);
  
  // Выключаем светодиод (общий анод)
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(BLUE_LED_PIN, HIGH);
  
  // Инициализация датчика DHT11
  dht.setup(DHT11_PIN, DHTesp::DHT11);
  Serial.println("Датчик DHT11 инициализирован");
  
  // Настройка точки доступа
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  
  Serial.print("Создана сеть: ");
  Serial.println(AP_SSID);
  Serial.print("IP адрес: ");
  Serial.println(WiFi.softAPIP());

  // Настройка веб-сервера
  server.on("/", []() {
    server.send(200, "text/html; charset=utf-8", index_html);
  });
  
  server.on("/data", []() {
    bool switchState = !digitalRead(SWITCH_PIN);
    
    String json = "{\"temp\":";
    json += String(currentTemp, 1);
    json += ",\"light\":";
    json += String(lightPercent);
    json += ",\"status\":\"";
    json += ledStatus;
    json += "\",\"switch_state\":";
    json += switchState ? "true" : "false";
    json += "}";
    server.send(200, "application/json", json);
  });
  
  server.begin();
  Serial.println("HTTP сервер запущен");
  
  Serial.println("Система готова к работе");
  printDebugInfo();
}

void updateSystemState() {
  bool switchState = !digitalRead(SWITCH_PIN);
  float newTemp = dht.getTemperature();
  
  if (!isnan(newTemp)) {
    currentTemp = newTemp;
    
    // Чтение и преобразование показаний фоторезистора
    int lightValue = analogRead(LIGHT_SENSOR_PIN);
    lightPercent = map(lightValue, 0, 1023, 0, 100);

    // Определение состояния системы
    if (currentTemp <= 32.0) {
      // Режим 1: <=32°C - ЗЕЛЕНЫЙ
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(BLUE_LED_PIN, HIGH);
      ledStatus = "green";
      
      analogWrite(FAN_PIN, 0); // Всегда выключаем
    } 
    else if (currentTemp > 32.0 && currentTemp <= 42.0) {
      // Режим 2: 32-42°C - СИНИЙ
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(BLUE_LED_PIN, LOW);
      ledStatus = "blue";
      
      // Включаем вентилятор на 50% только если переключатель активен
      if (switchState) {
        analogWrite(FAN_PIN, 512);
      } else {
        analogWrite(FAN_PIN, 0);
      }
    } 
    else {
      // Режим 3: >42°C - КРАСНЫЙ
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, HIGH);
      ledStatus = "red";
      
      // Включаем вентилятор на 100% только если переключатель активен
      if (switchState) {
        analogWrite(FAN_PIN, 1023);
      } else {
        analogWrite(FAN_PIN, 0);
      }
    }
  }
}

void loop() {
  server.handleClient();
  
  unsigned long currentMillis = millis();
  
  // Обновление данных датчиков не чаще чем раз в 1 секунду
  if (currentMillis - lastDHTUpdate > DHT_UPDATE_INTERVAL) {
    updateSystemState();
    lastDHTUpdate = currentMillis;
  }
  
  // Вывод отладочной информации каждые 2 секунды
  if (currentMillis - lastSerialOutput > SERIAL_OUTPUT_INTERVAL) {
    printDebugInfo();
    lastSerialOutput = currentMillis;
  }
}