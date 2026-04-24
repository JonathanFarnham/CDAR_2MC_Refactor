#pragma once

void initUART();

bool uart_receive_targets(float* targetL, float* targetR);
bool uart_receive_spool_target(float* distance_mm);

void uart_send_telemetry(long ticksL, long ticksR);
