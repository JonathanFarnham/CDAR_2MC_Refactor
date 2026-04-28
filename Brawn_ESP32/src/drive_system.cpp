#include "drive_system.h"
#include "motor_hardware.h"
#include "pid_controller.h"
#include "config.h"

// State Variables
float targetRPM_L = 0; 
float targetRPM_R = 0;
float activeTargetRPM_L = 0; // The current speed PID is chasing
float activeTargetRPM_R = 0;
float currentRPM_L = 0;
float currentRPM_R = 0;

// Ramp settings
const float RAMP_STEP_RPM = 30.0;

// Straight Line Assist Tracking
bool wasDrivingStraight = false;
float syncTickOffset = 0;

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

        // Calculate raw RPM
        float rawRPM_L = (deltaL / dt_sec) * 60.0 / COUNTS_PER_REV;
        float rawRPM_R = (deltaR / dt_sec) * 60.0 / COUNTS_PER_REV;

        // Low Pass Filter
        currentRPM_L = (currentRPM_L * 0.7) + (rawRPM_L * 0.3);
        currentRPM_R = (currentRPM_R * 0.7) + (rawRPM_R * 0.3);

        // Ramping Logic
        if (abs(targetRPM_L - activeTargetRPM_L) <= RAMP_STEP_RPM) 
        {
            activeTargetRPM_L = targetRPM_L;
        } else if (targetRPM_L > activeTargetRPM_L) 
        {
            activeTargetRPM_L += RAMP_STEP_RPM;
        } else 
        {
            activeTargetRPM_L -= RAMP_STEP_RPM;
        }

        if (abs(targetRPM_R - activeTargetRPM_R) <= RAMP_STEP_RPM) 
        {
            activeTargetRPM_R = targetRPM_R;
        } else if (targetRPM_R > activeTargetRPM_R) 
        {
            activeTargetRPM_R += RAMP_STEP_RPM;
        } else 
        {
            activeTargetRPM_R -= RAMP_STEP_RPM;
        }

        float pidTarget_L = activeTargetRPM_L;
        float pidTarget_R = activeTargetRPM_R;

        /* Remooved in Favor of active heading control
        // Cross Coupling to Reduce Straight Line Drift
        if (targetRPM_L == targetRPM_R && targetRPM_L > 0) 
        {
            if(!wasDrivingStraight) {
                syncTickOffset = (getTicksLeft() * WHEEL_TRIM) - getTicksRight();
                wasDrivingStraight = true;
            }

            float scaledTicksLeft = getTicksLeft() * WHEEL_TRIM;
            float tickDiff = (scaledTicksLeft - getTicksRight()) - syncTickOffset;

            float syncKp = 0.5;
            float syncAdjustment = tickDiff * syncKp;
            syncAdjustment = constrain(syncAdjustment, -40, 40);

            pidTarget_L -= syncAdjustment;
            pidTarget_R += syncAdjustment;
        } else 
        {
            wasDrivingStraight = false;
        }
        */

        // PID Control
        int outputL = 0;
        int outputR = 0;

        if (targetRPM_L == 0) 
        {
            outputL = 0;
            activeTargetRPM_L = 0;
            pidL.reset();
        } else 
        {
            outputL = (int)pidL.compute(pidTarget_L, currentRPM_L, dt_sec);
        }

        if (targetRPM_R == 0) 
        {
            outputR = 0;
            activeTargetRPM_R = 0;
            pidR.reset();
        } else 
        {
            outputR = (int)pidR.compute(pidTarget_R, currentRPM_R, dt_sec);
        }

        outputL = constrain(outputL, -200, 200);
        outputR = constrain(outputR, -200, 200);

        Serial.printf("PWM out: L=%d R=%d (target L=%.1f R=%.1f, current L=%.1f R=%.1f)\n",
              outputL, outputR, activeTargetRPM_L, activeTargetRPM_R,
              currentRPM_L, currentRPM_R);

        setMotorRaw(outputL, outputR);
    }
}

//Stop All
void stopAll()
{
    targetRPM_L = 0;
    targetRPM_R = 0;
    activeTargetRPM_L = 0;
    activeTargetRPM_R = 0;
    pidL.reset();
    pidR.reset();
    setMotorRaw(0, 0);
}

//Telemetry Functions
float getCurrentRPMLeft() {return currentRPM_L;}
float getCurrentRPMRight() {return currentRPM_R;}

float getTargetRPMLeft() {return activeTargetRPM_L;}
float getTargetRPMRight() {return activeTargetRPM_R;}