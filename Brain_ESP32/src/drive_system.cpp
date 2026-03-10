#include "drive_system.h"
#include "uart_comms.h"

// Variables to hold the ticks sent from the Brawn over UART
long virtual_ticks_l = 0;
long virtual_ticks_r = 0;

// Variables to handle "resetting" the distance for grid passes
long offset_ticks_l = 0;
long offset_ticks_r = 0;

void initDriveSystem() {
    initUART(); // Start the Serial2 bridge
}

void updateDriveSystem() {
    // Continuously poll UART for telemetry updates from the Brawn
    long temp_l, temp_r;
    if (uart_receive_telemetry(&temp_l, &temp_r)) {
        virtual_ticks_l = temp_l;
        virtual_ticks_r = temp_r;
    }
}

void setTargetRPM(float leftRPM, float rightRPM) {
    // Send targets to Brawn instead of calculating PID locally
    uart_send_targets(leftRPM, rightRPM); 
}

void stopAll() {
    uart_send_targets(0, 0);
}

// Return the true distance by subtracting the offset
long getTicksLeft() { 
    return virtual_ticks_l - offset_ticks_l; 
}
long getTicksRight() { 
    return virtual_ticks_r - offset_ticks_r; 
}

// Instead of resetting the Brawn's physical hardware, we just zero our local math
void resetTickCount() {
    offset_ticks_l = virtual_ticks_l;
    offset_ticks_r = virtual_ticks_r;
}