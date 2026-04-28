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
    BaseType_t r = xQueueOverwrite(target_queue, &t);
    Serial.printf(" enqueue: q=%p result=%d\n", target_queue, (int)r);
}

// Called from Core 1 (main loop) — same core as uart_receive_telemetry
void uart_flush_targets()
{
    RPMTarget t;
    BaseType_t r = xQueueReceive(target_queue, &t, 0);

    // Log the handle + receive result once per second, regardless of outcome
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 1000) {
        lastLog = millis();
        Serial.printf("flush: q=%p r=%d\n", target_queue, (int)r);
    }

    if (r == pdTRUE)
    {
        Serial.printf("TX -> T,%.1f,%.1f\n", t.left, t.right);
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