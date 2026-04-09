//Brawn ESP32 Drive System
#include "drive_system.h"
#include "motor_hardware.h"
#include "pid_controller.h"
#include "config.h"

float targetRPM_L = 0;
float targetRPM_R = 0;
float currentRPM_L = 0;
float currentRPM_R = 0;

PIDController pidL(PID_KP, PID_KI, PID_KD);
PIDController pidR(PID_KP, PID_KI, PID_KD);

unsigned long lastCalcTime = 0;
long lastTicks_L = 0;
long lastTicks_R = 0;

void initDriveSystem()
{
    initMotorHardware();
    pidL.reset();
    pidR.reset();
}

void setTargetRPM(float leftRPM, float rightRPM)
{
    targetRPM_L = leftRPM;
    targetRPM_R = rightRPM;
}

void updateDriveSystem() 
{
    unsigned long now = millis();
    long dt_ms = now - lastCalcTime;

    if (dt_ms >= CALC_INTERVAL) 
    {
        lastCalcTime = now;
        float dt_sec = dt_ms / 1000.0;

        long currTicksL = getTicksLeft();
        long currTicksR = getTicksRight();

        long deltaL = currTicksL - lastTicks_L;
        long deltaR = currTicksR - lastTicks_R;

        lastTicks_L = currTicksL;
        lastTicks_R = currTicksR;

        float rawRPM_L = (deltaL / dt_sec) * 60.0 / COUNTS_PER_REV;
        float rawRPM_R = (deltaR / dt_sec) * 60.0 / COUNTS_PER_REV;

        currentRPM_L = (currentRPM_L * 0.7) + (rawRPM_L * 0.3);
        currentRPM_R = (currentRPM_R * 0.7) + (rawRPM_R * 0.3);

        int outputL = (targetRPM_L == 0) ? 0 : (int)pidL.compute(targetRPM_L, currentRPM_L, dt_sec);
        int outputR = (targetRPM_R == 0) ? 0 : (int)pidR.compute(targetRPM_R, currentRPM_R, dt_sec);

        if (targetRPM_L == 0) pidL.reset();
        if (targetRPM_R == 0) pidR.reset();

        outputL = constrain(outputL, -255, 255);
        outputR = constrain(outputR, -255, 255);

        setMotorRaw(outputL, outputR);
    }
}