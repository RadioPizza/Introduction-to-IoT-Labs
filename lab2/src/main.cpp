#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EncButton.h>
#include <stdint.h>

#define LED_PIN D4            // Встроенный светодиод
#define BUTTON_PIN D0         // Физическая кнопка
#define AP_SSID "NodeMCU-LED" // Имя точки доступа
#define AP_PASS "12345678"    // Пароль точки доступа

ESP8266WebServer server(80);
Button btn(BUTTON_PIN, INPUT_PULLUP);

volatile uint8_t ledState = false;
uint16_t lastUpdate = 0;

const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>Управление светодиодом</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial; text-align: center; margin-top: 50px; }
      .button {
        padding: 20px 40px;
        font-size: 24px;
        border: none;
        border-radius: 8px;
        cursor: pointer;
        transition: 0.3s;
      }
      .on { background-color: #4CAF50; color: white; }
      .off { background-color: #ff4444; color: white; }
      .status {
        margin-top: 20px;
        padding: 10px;
        border-radius: 5px;
        background-color: #f0f0f0;
      }
    </style>
  </head>
  <body>
    <h1>Управление светодиодом</h1>
    <button class="button %STATE%" onclick="toggleLED()">%STATE%</button>
    <div class="status">
      IP: %IP%<br>
      Подключено устройств: %CLIENTS%
    </div>
    <script>
      function toggleLED() {
        fetch('/toggle').then(response => updateState());
      }
  
      function updateState() {
        fetch('/state')
          .then(response => response.text())
          .then(state => {
            const btn = document.querySelector('.button');
            btn.className = 'button ' + state;
            btn.innerHTML = state;
          });
      }
      setInterval(updateState, 500);
    </script>
  </body>
  </html>
  )rawliteral";

String processor(const String &var)
{
  if (var == "STATE")
    return ledState ? "on" : "off";
  if (var == "IP")
    return WiFi.softAPIP().toString();
  if (var == "CLIENTS")
    return String(WiFi.softAPgetStationNum());
  return String();
}

void handleRoot()
{
  String html = index_html;
  html.replace("%STATE%", processor("STATE"));
  html.replace("%IP%", processor("IP"));
  html.replace("%CLIENTS%", processor("CLIENTS"));
  server.send(200, "text/html; charset=utf-8", html);
}

void handleToggle()
{
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  server.send(200, "text/plain", "OK");
}

void handleState()
{
  server.send(200, "text/plain", ledState ? "on" : "off");
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Настройка точки доступа
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  Serial.print("Создана сеть: ");
  Serial.println(AP_SSID);
  Serial.print("IP адрес: ");
  Serial.println(WiFi.softAPIP());

  // Настройка веб-сервера
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/state", handleState);
  server.begin();
}

void loop()
{
  server.handleClient();
  btn.tick();

  // Обработка физической кнопки
  if (btn.click())
  {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    Serial.println("Кнопка нажата - светодиод переключен");
  }

  // Периодический вывод информации
  if (millis() - lastUpdate > 5000)
  {
    lastUpdate = millis();
    Serial.printf("Подключено устройств: %d\n", WiFi.softAPgetStationNum());
  }
}