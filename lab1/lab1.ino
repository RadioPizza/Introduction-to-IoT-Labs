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

IPAddress local_ip(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
ESP8266WebServer server(80);

void handle_index() {
  server.send(200, "text/html", "Hello!");
}

void handle_not_found() {
  server.send(404, "text/plain", "Not Found");
}

void setup() {
  // Инициализация Wi-Fi
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(SSID, PASSWORD);

  // Настройка сервера
  server.on("/", handle_index);
  server.onNotFound(handle_not_found);
  server.begin();
}

void loop() {
  // Обработка клиентских запросов
  server.handleClient();
}