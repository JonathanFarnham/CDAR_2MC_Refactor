#include <Arduino.h>
#include "uart_comms.h"
#include "config.h"

void initUART() 
{
    Serial2.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
}

bool uart_receive_targets(float* targetL, float* targetR) 
{
    bool received = false;
    while (Serial2.available()) {
        String msg = Serial2.readStringUntil('\n');
        if (msg.startsWith("T,")) { // 'T' for Targets
            int firstComma = msg.indexOf(',');
            int secondComma = msg.indexOf(',', firstComma + 1);
            
            if (firstComma > 0 && secondComma > 0) {
                *targetL = msg.substring(firstComma + 1, secondComma).toFloat();
                *targetR = msg.substring(secondComma + 1).toFloat();
                received = true;
            }
        }
    }
    return received;
}

void uart_send_telemetry(long ticksL, long ticksR) {
    // Format: K,LeftTicks,RightTicks\n
    Serial2.print("K,");
    Serial2.print(ticksL);
    Serial2.print(",");
    Serial2.println(ticksR);
}