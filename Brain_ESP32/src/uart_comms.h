#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct RPMTarget { float left; float right; };
struct SpoolTarget { float distance_mm; };

void initUART();

void uart_enqueue_targets(float leftRPM, float rightRPM); // called from Core 0
void uart_enqueue_spool_target(float distance_mm);

void uart_flush_targets();    
                             // called from Core 1
bool uart_receive_telemetry(long* ticksL, long* ticksR);  // called from Core 1
