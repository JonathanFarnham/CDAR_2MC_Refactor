#include "motor_hardware.h"
#include "config.h"

volatile long ticks_l = 0;
volatile long ticks_r = 0;

void IRAM_ATTR isr_l()
{
    if (digitalRead(ENCODER_LEFT_B) == LOW)
    {
        ticks_l++;
    } else 
    {
        ticks_l--;
    }
}

void IRAM_ATTR isr_r()
{
    if (digitalRead(ENCODER_RIGHT_B) == LOW)
    {
        ticks_r--;
    } else
    {
        ticks_r++;
    }
}

void initMotorHardware()
{
    pinMode(MOTOR_LEFT_EN, OUTPUT);
    pinMode(MOTOR_LEFT_IN1, OUTPUT);
    pinMode(MOTOR_LEFT_IN2, OUTPUT);
    pinMode(MOTOR_RIGHT_EN, OUTPUT);
    pinMode(MOTOR_RIGHT_IN1, OUTPUT);
    pinMode(MOTOR_RIGHT_IN2, OUTPUT);
   
    pinMode(ENCODER_LEFT_A, INPUT_PULLUP);
    pinMode(ENCODER_LEFT_B, INPUT_PULLUP);

    pinMode(ENCODER_RIGHT_A, INPUT_PULLUP);
    pinMode(ENCODER_RIGHT_B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT_A), isr_l, RISING);
    attachInterrupt(digitalPinToInterrupt(ENCODER_RIGHT_A), isr_r, RISING);
}

void setMotorRaw(int pwmL, int pwmR)
{
    //Left Motor
    if (MOTOR_LEFT_INVERT) pwmL = -pwmL;

    if (pwmL > 0)
    {
        digitalWrite(MOTOR_LEFT_IN1, HIGH); digitalWrite(MOTOR_LEFT_IN2, LOW);
        analogWrite(MOTOR_LEFT_EN, pwmL);
    } 
    else if (pwmL < 0)
    {
        digitalWrite(MOTOR_LEFT_IN1, LOW); digitalWrite(MOTOR_LEFT_IN2, HIGH);
        analogWrite(MOTOR_LEFT_EN, -pwmL); 
    } 
    else
    {
        digitalWrite(MOTOR_LEFT_IN1, LOW); digitalWrite(MOTOR_LEFT_IN2, LOW);
        analogWrite(MOTOR_LEFT_EN, 0); 
    }

    //Right Motor
    if (MOTOR_RIGHT_INVERT) pwmR = -pwmR;

    if (pwmR > 0)
    {
        digitalWrite(MOTOR_RIGHT_IN1, HIGH); digitalWrite(MOTOR_RIGHT_IN2, LOW);
        analogWrite(MOTOR_RIGHT_EN, pwmR);
    } 
    else if (pwmR < 0)
    {
        digitalWrite(MOTOR_RIGHT_IN1, LOW); digitalWrite(MOTOR_RIGHT_IN2, HIGH);
        analogWrite(MOTOR_RIGHT_EN, -pwmR);
    } 
    else
    {
        digitalWrite(MOTOR_RIGHT_IN1, LOW); digitalWrite(MOTOR_RIGHT_IN2, LOW);
        analogWrite(MOTOR_RIGHT_EN, 0);
    }
}

long getTicksLeft() { return ticks_l; }
long getTicksRight() { return ticks_r; }
void resetTickCount() { ticks_l = 0; ticks_r = 0;}