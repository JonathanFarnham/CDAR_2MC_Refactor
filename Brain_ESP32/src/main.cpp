#include <Arduino.h>
#include "config.h"
#include "drive_system.h"
#include "web_server.h"
#include "grid_control.h"

TaskHandle_t TaskWeb;

unsigned long lastDebugTime = 0;
const int DEBUG_INTERVAL = 500; // print every 500ms so we don't spam the console

// Core 0: Dedicated entirely to handling WiFi and HTTP Requests
void TaskWebCode(void * pvParameters) {
  for(;;) {
    handleClient();
    vTaskDelay(2 / portTICK_PERIOD_MS); 
  }
}

// Core 1: Setup
void setup() {
  Serial.begin(115200);

  initDriveSystem(); // Initializes UART bridge
  initWebServer();   // Mounts LittleFS and starts AP

  // Launch Web Server on Core 0
  xTaskCreatePinnedToCore(
    TaskWebCode, "TaskWeb", 10000, NULL, 1, &TaskWeb, 0
  );

  Serial.println("BRAIN ESP32 INITIALIZED.");
}

// Core 1: Main Logic Loop
void loop() {
  updateDriveSystem(); // Grab latest encoder ticks via UART
  handleGrid();        // Process grid navigation state machine

  // Print telemetry to verify connection with the Brawn
  if (millis() - lastDebugTime >= DEBUG_INTERVAL) {
    lastDebugTime = millis();
    Serial.printf("Virtual Ticks L: %ld | R: %ld\n", getTicksLeft(), getTicksRight());
  }

  delay(1); 
}