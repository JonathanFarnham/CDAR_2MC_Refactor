#include "uart_comms.h"
#include "config.h"
#include <Arduino.h>

static QueueHandle_t target_queue = nullptr;

void initUART()
{
    Serial2.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial2.setTimeout(10);
    target_queue = xQueueCreate(1, sizeof(RPMTarget));
}

// Called from Core 0 (web task) — only touches the queue, never Serial2
void uart_enqueue_targets(float leftRPM, float rightRPM)
{
    RPMTarget t = { leftRPM, rightRPM };
    // Overwrite if the queue is already full so STOP commands always get through
    xQueueOverwrite(target_queue, &t);
}

// Called from Core 1 (main loop) — same core as uart_receive_telemetry
void uart_flush_targets()
{
    RPMTarget t;
    if (xQueueReceive(target_queue, &t, 0) == pdTRUE)
    {
        Serial2.print("T,");
        Serial2.print(t.left);
        Serial2.print(",");
        Serial2.println(t.right);
    }
}

bool uart_receive_telemetry(long* ticksL, long* ticksR)
{
    bool received = false;
    while (Serial2.available())
    {
        String msg = Serial2.readStringUntil('\n');
        if (msg.startsWith("K"))
        {
            int firstComma  = msg.indexOf(',');
            int secondComma = msg.indexOf(',', firstComma + 1);
            if (firstComma > 0 && secondComma > 0)
            {
                *ticksL   = msg.substring(firstComma + 1, secondComma).toInt();
                *ticksR   = msg.substring(secondComma + 1).toInt();
                received  = true;
            }
        }
    }
    return received;
}