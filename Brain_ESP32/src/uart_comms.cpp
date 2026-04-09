#include <Arduino.h>
#include "uart_comms.h"
#include "config.h"

void initUART()
{
    //Initialize Serial2 for ESP-to-ESP communication
    Serial2.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
}

void uart_send_targets(float leftRPM, float rightRPM)
{
    //Format: T,LeftRPM,RightRPM\n
    Serial2.print("T,"); //
    Serial2.print(leftRPM);
    Serial2.print(",");
    Serial2.println(rightRPM);
}

bool uart_receive_telemetry(long* ticksL, long* ticksR)
{
    bool received = false;
    while (Serial2.available())
    {
        String msg = Serial2.readStringUntil('\n');
        if (msg.startsWith("K")) //'K' is for Kinematics and contains Tick data
        {
            int firstComma = msg.indexOf(',');
            int secondComma = msg.indexOf(',', firstComma + 1);

            if (firstComma > 0 && secondComma > 0)
            {
                *ticksL = msg.substring(firstComma + 1, secondComma).toInt();
                *ticksR = msg.substring(secondComma + 1).toInt();
                received = true;
            }
        }
    }
    return received;
}