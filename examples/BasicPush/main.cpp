#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#ifdef ESP32
#include <WiFi.h>
#endif // ESP32
#endif // ESP8266

#include "PrometheusPushClient.h"

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Prometheus Push Gateway configuration
const char* pushGatewayHost = "push.example.com";
const int pushGatewayPort = 80;

WiFiClient cli;

// Example telemetry data
float temperature = 25.5;
float humidity = 60.0;
float pressure = 1013.25;
float vcc = 3.3;

// Define Strings in Flash to save RAM
const char job_name[] PROGMEM = "demo";
const char n_board_info[] PROGMEM = "board_info";
const char h_board_info[] PROGMEM = "Information about the board";
const char l_build[] PROGMEM = "build";
const char l_room[] PROGMEM = "room";
const char n_temp[] PROGMEM = "temperature";
const char h_temp[] PROGMEM = "Temperature in Celsius";
const char n_humidity[] PROGMEM = "humidity";
const char h_humidity[] PROGMEM = "Relative air humidity in percent";
const char n_pressure[] PROGMEM = "pressure";
const char h_pressure[] PROGMEM = "Pressure in Pascals";
const char n_vcc[] PROGMEM = "vcc";
const char h_vcc[] PROGMEM = "Supply voltage in millivolts";
const char r_kitchen[] PROGMEM = "Kitchen";

// Create an instance of PrometheusPushClient with 
// - up to 5 metrics, 
// - up to 2 metric-specific labels, and
// - 1 common label {room="Kitchen"} which will be included in all metrics pushed to the gateway
PrometheusPushClient<5, 2, 1> prom(&cli, {{{l_room, r_kitchen}}});

void setup() {
  Serial.begin(115200);
  Serial.println("Setting up Prometheus Push Client example...");

  prom.addMetric(n_board_info, h_board_info, {{{l_build, __DATE__ " " __TIME__}}}, MetricType::Gauge);
  prom.addMetric(n_temp, h_temp, MetricType::Gauge);
  prom.addMetric(n_humidity, h_humidity, MetricType::Gauge);
  prom.addMetric(n_pressure, h_pressure, MetricType::Gauge);
  prom.addMetric(n_vcc, h_vcc, MetricType::Gauge);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
}

void loop() {
  Serial.println("Pushing metrics to Prometheus Push Gateway...");
  prom.push(pushGatewayHost, pushGatewayPort, job_name, WiFi.localIP().toString().c_str());

  delay(30e3); // Push metrics every 30 seconds
}