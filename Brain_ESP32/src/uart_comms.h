#pragma once

void initUART();
void uart_send_targets(float leftRPM, float rightRPM);
bool uart_receive_telemetry(long* ticksL, long* ticksR);