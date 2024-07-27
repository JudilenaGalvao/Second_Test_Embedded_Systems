/* Alterado por Josenalde Oliveira em 19.07.2024 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>



// servidor
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>



// Replace with your network credentials
const char* ssid = "Silva";
const char* password = "tb010203@";

long static rpmSetpoint;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Json Variable to Hold Sensor Readings
JSONVar readings;
String jsonString;

long rotacao = 0;

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
    Serial.println("Não conectado");
    delay(1000);
  }
  Serial.println("conectado no ip: ");
  Serial.println(WiFi.localIP());
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; // Null-terminate the string
    String message = (char*)data;
    rotacao = message.toInt();
    rpmSetpoint = rotacao;
    Serial.printf("Received rotation value: %ld\n", rotacao);
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


// ======== servidor============

// Settings
static const TickType_t rpsInterval = 1000 / portTICK_PERIOD_MS;  // 1000ms = 1s (timer for getting pulses) - rotation per second
static const TickType_t controlSampleTime = 100 / portTICK_PERIOD_MS;

static SemaphoreHandle_t mutex;

int rpmInput;
// long static rpmSetpoint;
long static rpmSetpointInit;

volatile int pulse; //Variable for counting rotating pulses

double VCC = 3.3; // For ESP32 3.3
int yPin = 15; //25 //GPIO25 PWM
//for sensor pin (pv sensor)
int sensorPin = 23; //27
int in1Pin = 2;//12
int in2Pin = 4;//14

double u, kp;

long rpmMax = 2940; //it requires validation - apply 255 to ENABLE A and measure speed RPM

// ISR fan rotations
void IRAM_ATTR countPulses() {
   pulse++;//increments each RISING edge.
}

// Globals
static TimerHandle_t getRPMTimer = NULL;
static TimerHandle_t generatePIDControlTimer = NULL;

//***************************
// Callbacks

// get a new sample
void getRPMCallback(TimerHandle_t xTimer) {
  rpmInput = (pulse/2) * 60; // two pulses each rotation - hall effect sensor
  pulse = 0;
}

void generatePIDControlCallback(TimerHandle_t xTimer) {
    kp = (255.0 / rpmMax);
    u = kp * rpmSetpoint;
}

void taskPrintGraph(void *parameters) {
  while (true) {
    Serial.print(rpmMax);
    Serial.print(" ");
    Serial.print(0);
    Serial.print(" ");
    Serial.print(rpmSetpoint);
    Serial.println(); 
    //Serial.println(rpmInput);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}



void taskGetSetPoint(void *parameters) {
  while (true) {
     if (Serial.available() > 0) {
        xSemaphoreTake(mutex, portMAX_DELAY);
          rpmSetpoint = Serial.parseInt();
        xSemaphoreGive(mutex);
        if (rpmSetpoint != 0) {
            rpmSetpointInit = rpmSetpoint;
        } else if (rpmSetpoint == 0) {
            rpmSetpoint = rpmSetpointInit;
        }
     }
     vTaskDelay(pdMS_TO_TICKS(100));
  }
}
             
void setup() {
  Serial.begin(115200);
  // servidor
  initWiFi();
  initLittleFS();
  initWebSocket();

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.serveStatic("/", LittleFS, "/");
  server.begin();

  // =========
  
  getRPMTimer = xTimerCreate(
    "getRPMTimer",    // Name of timer
    rpsInterval,           // Period of timer (in ticks)
    pdTRUE,                     // Auto-reload TRUE, one_shot FALSE
    (void *)0,                  // Timer ID
    getRPMCallback);  // Callback function

  generatePIDControlTimer = xTimerCreate(
    "generatePIDControlTimer",    // Name of timer
    controlSampleTime,           // Period of timer (in ticks)
    pdTRUE,                     // Auto-reload TRUE, one_shot FALSE
    (void *)1,                  // Timer ID
    generatePIDControlCallback);  // Callback function

  mutex = xSemaphoreCreateMutex();
      
  // ---------- configurations --------------------
  // configure fan speed sensor reading
  attachInterrupt(sensorPin, countPulses, RISING);
  
  // configure pwm output
  analogWriteResolution(yPin, 8);
  analogWriteFrequency(yPin, 30000);

  //ledcAttach(yPin, 25000, 8); //(pin 25) ledcAttachPin DEPRACTED - Arduino-ESP32 LEDC API
    
  // set pin direction
  pinMode(sensorPin, INPUT);
  pinMode(yPin, OUTPUT);
  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
      
  // set forward movement
  digitalWrite(in1Pin, HIGH);
  digitalWrite(in2Pin, LOW);
  
  rpmSetpoint = 2000; // considering 0-4095 - USER ENTERS THIS VALUE UI
  
  // Start Printing task
  xTaskCreatePinnedToCore(taskPrintGraph,
                          "taskPrintGraph",
                          1024,
                          NULL,
                          2,
                          NULL,
                          0);

xTaskCreatePinnedToCore(taskGetSetPoint,
                        "taskGetSetPoint",
                        1024,
                        NULL,
                        1,
                        NULL,
                        0);

  xTimerStart(getRPMTimer, 0);
  xTimerStart(generatePIDControlTimer, 0);

}  
                            
void loop() { 
  analogWrite(yPin, (int)u); //ESP32 PWM - ENABLE A L298D
  // analogWrite(yPin, 255); //ESP32 PWM - ENABLE A L298D
}