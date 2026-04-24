#include <Arduino.h>
#include "uart_comms.h"
#include "config.h"

//Latest Buffers for each message type
static float buf_targetL = 0.0f;
static float buf_targetR = 0.0f;
static bool bof_target_fresh = false;

static float bud_spool_mm = 0.0f;
static bool buf_spool_fresh = false;

void initUART() 
{
    Serial2.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial2.setTimeout(10);
}

/ Drains the UART buffer once per call. Any complete '\n'-terminated message
// is parsed and stashed in the appropriate static buffer. Multiple messages
// in one call are all processed; only the most recent of each type is kept.
static void drain_and_parse()
{
    while (Serial2.available())
    {
        String msg = Serial2.readStringUntil('\n');
        if (msg.length() < 2) continue;

        // Dispatch on first character. Saves parsing cost and keeps each
        // message type's handling self-contained.
        char type = msg.charAt(0);

        if (type == 'T')
        {
            // Drive targets: T,<leftRPM>,<rightRPM>
            int firstComma  = msg.indexOf(',');
            int secondComma = msg.indexOf(',', firstComma + 1);
            if (firstComma > 0 && secondComma > firstComma)
            {
                buf_targetL      = msg.substring(firstComma + 1, secondComma).toFloat();
                buf_targetR      = msg.substring(secondComma + 1).toFloat();
                buf_target_fresh = true;
            }
        }
        else if (type == 'P')
        {
            // Spool position target: P,<distance_mm>
            int firstComma = msg.indexOf(',');
            if (firstComma > 0)
            {
                buf_spool_mm    = msg.substring(firstComma + 1).toFloat();
                buf_spool_fresh = true;
            }
        }
        // Unknown message types are silently ignored so future additions
        // are backward-compatible.
    }
}

bool uart_receive_targets(float* targetL, float* targetR)
{
    drain_and_parse();
    if (!buf_target_fresh) return false;

    *targetL = buf_targetL;
    *targetR = buf_targetR;
    buf_target_fresh = false;   // consumed
    return true;
}

bool uart_receive_spool_target(float* distance_mm)
{
    // Don't drain here — drain happens in uart_receive_targets() above.
    // Calling drain from both would double-read the buffer. Since main.cpp
    // calls uart_receive_targets() first every loop iteration, by the time
    // this function runs the buffer is already parsed.
    if (!buf_spool_fresh) return false;

    *distance_mm = buf_spool_mm;
    buf_spool_fresh = false;
    return true;
}

void uart_send_telemetry(long ticksL, long ticksR)
{
    Serial2.print("K,");
    Serial2.print(ticksL);
    Serial2.print(",");
    Serial2.println(ticksR);
}