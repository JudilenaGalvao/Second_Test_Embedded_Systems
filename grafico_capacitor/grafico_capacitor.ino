/********* WEBSERVER/WEBSOCKET
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-websocket-server-sensor/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

/******** RC CIRCUIT IDENTIFICATION/TIMERS/RTOS
  Josenalde Oliveira
**********/

#include <WiFi.h>
#include <AsyncTCP.h> // Sketch - Libraries Add from ZIP
#include <ESPAsyncWebServer.h> // Sketch - Libraries Add from ZIP

//#include "LittleFS.h" original
#include <LittleFS.h>
#include <Arduino_JSON.h> // Tools - Manage Libraries (install Arduino JSON by Arduino)

#include <freertos/FreeRTOS.h>

// Replace with your network credentials
const char* ssid = "Silva";
const char* password = "tb010203@";

// Settings according to resistor value and capacitor value
static const float R = 20000; //ohms 20k
static const float C = 0.0001; //faraday 100 uF
static const float timeConstant = R*C;
static const TickType_t samplingInterval = ((timeConstant*1000)/10.) / portTICK_PERIOD_MS; // 200ms = 0.2s
static const TickType_t timeToApplyMV = (timeConstant*4*1000) / portTICK_PERIOD_MS; 

// Globals
static const uint8_t PV_PIN = 2;
static const uint8_t SENSOR_PIN = 34;
static const uint8_t MV_PIN = 25;
volatile float VCC = 0.0; //initial value applied from t=0 to t=timeToApplyMV
volatile uint16_t sensorReadingInt;
volatile float sensorReadingVoltage;
String jsonString;

static TimerHandle_t getSensorReadingTimer = NULL;
static TimerHandle_t stepInputStartTimer = NULL;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;
unsigned long refreshInterval = ((timeConstant*1000)/10.); 

// Callbacks

// get a new sample
void getSensorReadingCallback(TimerHandle_t xTimer) {
  readings["time"] = String(xTaskGetTickCount() / 1000.);
  sensorReadingInt = analogRead(SENSOR_PIN);
  sensorReadingVoltage = (sensorReadingInt) * (VCC/4096.);
  readings["MV"] =  String(VCC);
  readings["PV"] =  String(sensorReadingVoltage);
  Serial.print(xTaskGetTickCount() / 1000., 1);
  Serial.print(",");
  Serial.print(VCC);
  Serial.print(",");
  Serial.println(sensorReadingVoltage, 1);
  jsonString = JSON.stringify(readings);
  notifyClients(jsonString);
  //ws.cleanupClients(); // in order to avoid exceeding maximum clients - a timer for this is necessary
}

// send step after <timeToStartInterval> seconds
void setStepInputReadingCallback(TimerHandle_t xTimer) {
  digitalWrite(MV_PIN, HIGH);
  VCC = 3.3;
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      notifyClients(jsonString);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      Serial.println("handle");
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  initLittleFS();
  initWebSocket();

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.serveStatic("/", LittleFS, "/");
  server.begin();
  //---------------------

  // Configure I/O direction
  pinMode(MV_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);
    // Create a auto-reload timer for sensor readings
  getSensorReadingTimer = xTimerCreate(
                      "getSensorReadingTimer",     // Name of timer
                      samplingInterval,            // Period of timer (in ticks)
                      pdTRUE,              // Auto-reload TRUE, one_shot FALSE
                      (void *)0,            // Timer ID
                      getSensorReadingCallback);  // Callback function
  // Create a one shot timer for step output
  stepInputStartTimer  = xTimerCreate(
                      "stepInputStartTimer ",     // Name of timer
                      timeToApplyMV,            // Period of timer (in ticks)
                      pdFALSE,              // Auto-reload TRUE, one_shot FALSE
                      (void *)1,            // Timer ID
                      setStepInputReadingCallback);  // Callback function

  xTimerStart(getSensorReadingTimer, 0);
  xTimerStart(stepInputStartTimer, 0); 
}

void loop() {
  vTaskSuspend(NULL);
}
