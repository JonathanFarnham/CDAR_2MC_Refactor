#include "config.h"
#include "grid_control.h"
#include "drive_system.h" 
#include "mpu_handler.h"  
#include <Arduino.h>

//Grid Params
float grid_len_ft = 0;
float grid_width_ft = 0;
int total_passes = 0;
bool turn_right_first = true;
float target_heading = 0.0;

//Heading Data Timer Variables
unsigned long lastHeadingSendTime = 0;
const int HEADING_SEND_INTERVAL = 100; //Match calc interval time

//Flag for Grid Active
bool isAutoPilotActive = false;

//State Machine Variables
enum State { IDLE, DRIVING_LONG, TURNING_1, DRIVING_SHORT, TURNING_2, FINISHED };
State currentState = IDLE;

int current_pass = 0;
long target_ticks = 0;
bool current_turn_right = true; //tracks next turn direction

//Configuration
void configureGrid(float len, float width, int passes, bool start_right)
{
    grid_len_ft = len;
    grid_width_ft = width;
    total_passes = passes;
    turn_right_first = start_right;
}

void startGridRun()
{
    if (grid_len_ft == 0 || total_passes == 0) return;
    if (isAutoPilotActive) return; //prevent from running if already running

    isAutoPilotActive = true; //lock
    currentState = DRIVING_LONG;
    current_pass = 1;
    current_turn_right = turn_right_first;

    //Lock in permanent grid orientation
    resetYaw();
    target_heading = 0.0;

    //setup first move (length)
    resetTickCount();
    target_ticks = grid_len_ft * TICKS_PER_FOOT;
    
    //Use RPM instead of PWM
    setTargetRPM(SPEED_GRID_RPM, SPEED_GRID_RPM); 
    
    Serial.println("Grid Started");
}

void stopGridRun()
{
    currentState = IDLE;
    stopAll(); // UPDATED
    isAutoPilotActive = false; //Unlock
    Serial.println("Grid Stopped/Finished");
}

//Helper for other modules to check status
bool isGridRunning()
{
    return isAutoPilotActive;
}

//main loop logic
void handleGrid()
{
    if (currentState == IDLE || currentState == FINISHED)
    {
        if (currentState == FINISHED)
        {
            stopGridRun(); 
            currentState = IDLE;
        }
        return;
    }

    bool targetReached = false;

    //Movement and Heading Logic
    if (currentState == DRIVING_LONG || currentState == DRIVING_SHORT) 
    {
        long current_dist = (abs(getTicksLeft()) + abs(getTicksRight())) / 2;
        if (current_dist >= target_ticks) 
        {
            targetReached = true;
        } 
        else 
        {
            if (millis() - lastHeadingSendTime >= HEADING_SEND_INTERVAL)
            {
                //Active Heading Control
                float headingError = target_heading - getYawAngle(); 
            
                float heading_Kp = 5.0; 
                float rpmCorrection = headingError * heading_Kp;
                rpmCorrection = constrain(rpmCorrection, -40, 40); 
            
                float newLeftRPM = SPEED_GRID_RPM - rpmCorrection;
                float newRightRPM = SPEED_GRID_RPM + rpmCorrection;
            
                setTargetRPM(newLeftRPM, newRightRPM);
            }
        }
    } 
    else if (currentState == TURNING_1 || currentState == TURNING_2) 
    {
        //Check degree left to get to target
        //Shoot for 86 degrees to acount for turning inertia
        float degreesRemaining = abs(target_heading - getYawAngle());
        if (degreesRemaining <= 4.0) 
        {
            targetReached = true;
        }
    }

    //State Transition Logic
    if (targetReached)
    {
        stopAll();
        unsigned long brakeTimer = millis();
        while (millis() - brakeTimer < 200)
        {
            updateMPU(); //keep integrating uaw during inertial coast
            delay(1);
        } 
        resetTickCount();

        switch (currentState)
        {
            case DRIVING_LONG:
                if (current_pass >= total_passes) 
                {
                    currentState = FINISHED;
                } else 
                {
                    currentState = TURNING_1;
                    
                    //Set the new absolute target heading for the turn
                    if (current_turn_right) 
                    {
                        target_heading -= 90.0; //Right turns go negative
                        setTargetRPM(SPEED_TURN_RPM, -SPEED_TURN_RPM); 
                    } else 
                    {
                        target_heading += 90.0; //Left turns go positive
                        setTargetRPM(-SPEED_TURN_RPM, SPEED_TURN_RPM);
                    }
                }
                break;
            
            case TURNING_1:
                currentState = DRIVING_SHORT;
                {
                    float spacing = grid_width_ft / (total_passes - 1);
                    target_ticks = spacing * TICKS_PER_FOOT;
                    setTargetRPM(SPEED_GRID_RPM, SPEED_GRID_RPM);
                }
                break;

            case DRIVING_SHORT:
                currentState = TURNING_2;
                
                if (current_turn_right) 
                {
                    target_heading -= 90.0; 
                    setTargetRPM(SPEED_TURN_RPM, -SPEED_TURN_RPM);
                } else 
                {
                    target_heading += 90.0; 
                    setTargetRPM(-SPEED_TURN_RPM, SPEED_TURN_RPM);
                }
                break;

            case TURNING_2:
                currentState = DRIVING_LONG;
                current_pass++;
                current_turn_right = !current_turn_right;

                target_ticks = grid_len_ft * TICKS_PER_FOOT;
                setTargetRPM(SPEED_GRID_RPM, SPEED_GRID_RPM);
                break;
        }
    }
}