#pragma once
#include <Arduino.h>

void initMotorHardware();
void setMotorRaw(int pwmL, int pwmR);
long getTicksLeft();
long getTicksRight();
void resetTickCount();