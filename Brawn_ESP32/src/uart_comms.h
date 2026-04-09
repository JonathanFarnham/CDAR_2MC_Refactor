#pragma once

void initUART();
bool uart_receive_targets(float* targetL, float* targetR);
void uart_send_telemetry(long ticksL, long ticksR);