#include "electrode_servo.h"
#include "config.h"
#include <ESP32Servo.h>

static Servo electrodeServo;

void initServo()
{
    electrodeServo.attach(SERVO_PIN);
    wheelDown();
}

void wheelUp()   { electrodeServo.write(SERVO_WHEEL_UP);   }
void wheelDown() { electrodeServo.write(SERVO_WHEEL_DOWN); }