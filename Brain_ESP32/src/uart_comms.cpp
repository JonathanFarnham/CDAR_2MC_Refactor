#include "uart_comms.h"
#include "config.h"
#include <Arduino.h>

static QueueHandle_t target_queue = nullptr;
static QueueHandle_t spool_queue  = nullptr;

void initUART()
{
    Serial2.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial2.setTimeout(10);

    // Length-1 queues with xQueueOverwrite = "keep only the latest command"
    target_queue = xQueueCreate(1, sizeof(RPMTarget));
    spool_queue  = xQueueCreate(1, sizeof(SpoolTarget));
}

// ---- Core 0 (web task) ---------------------------------------------------
void uart_enqueue_targets(float leftRPM, float rightRPM)
{
    RPMTarget t = { leftRPM, rightRPM };
    xQueueOverwrite(target_queue, &t);
}

void uart_enqueue_spool_target(float distance_mm)
{
    SpoolTarget s = { distance_mm };
    xQueueOverwrite(spool_queue, &s);
}

// ---- Core 1 (main loop) --------------------------------------------------
void uart_flush_targets()
{
    // Flush drive targets
    RPMTarget t;
    if (xQueueReceive(target_queue, &t, 0) == pdTRUE)
    {
        Serial2.print("T,");
        Serial2.print(t.left);
        Serial2.print(",");
        Serial2.println(t.right);
    }

    // Flush spool targets
    SpoolTarget s;
    if (xQueueReceive(spool_queue, &s, 0) == pdTRUE)
    {
        Serial2.print("P,");
        Serial2.println(s.distance_mm);
    }
}

bool uart_receive_telemetry(long* ticksL, long* ticksR)
{
    bool received = false;
    while (Serial2.available())
    {
        String msg = Serial2.readStringUntil('\n');
        if (msg.length() < 2) continue;

        // Only telemetry ('K') currently comes back from Brawn. Dispatch on
        // first char so future message types don't require rewriting this.
        if (msg.charAt(0) == 'K')
        {
            int firstComma  = msg.indexOf(',');
            int secondComma = msg.indexOf(',', firstComma + 1);
            if (firstComma > 0 && secondComma > firstComma)
            {
                *ticksL  = msg.substring(firstComma + 1, secondComma).toInt();
                *ticksR  = msg.substring(secondComma + 1).toInt();
                received = true;
            }
        }
    }
    return received;
}