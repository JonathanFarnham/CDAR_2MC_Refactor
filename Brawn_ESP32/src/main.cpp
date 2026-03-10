//Brawn Main
#include <Arduino.h>
#include "config.h"
#include "drive_system.h"
#include "uart_comms.h"
#include "motor_hardware.h" // Needed to get the raw ticks to send back

unsigned long lastTelemetryTime = 0;

void setup() {
    Serial.begin(115200); // For PC Debugging
    
    initUART();        // Start the Serial2 link to the Brain
    initDriveSystem(); // Setup pins, interrupts, and PID
    
    Serial.println("BRAWN ESP32 INITIALIZED.");
}

void loop() {
    // 1. Check if the Brain sent new speed targets
    float newTargetL, newTargetR;
    if (uart_receive_targets(&newTargetL, &newTargetR)) {
        setTargetRPM(newTargetL, newTargetR);
    }

    // 2. Run the PID motor control loop (Executes precisely every 20ms)
    updateDriveSystem();

    // 3. Constantly broadcast our physical encoder ticks back to the Brain
    if (millis() - lastTelemetryTime >= CALC_INTERVAL) {
        lastTelemetryTime = millis();
        uart_send_telemetry(getTicksLeft(), getTicksRight());
    }
}