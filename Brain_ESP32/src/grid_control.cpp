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

    //Evaluate if the current movement is finished based on state type
    if (currentState == DRIVING_LONG || currentState == DRIVING_SHORT) 
    {
        long current_dist = (abs(getTicksLeft()) + abs(getTicksRight())) / 2;
        if (current_dist >= target_ticks) {
            targetReached = true;
        }
    } 
    else if (currentState == TURNING_1 || currentState == TURNING_2) 
    {
        //Check if integrated yaw has reached 90 degrees
        if (abs(getYawAngle()) >= 90.0) {
            targetReached = true;
        }
    }

    //Transition State if target reached
    if (targetReached)
    {
        stopAll();
        delay(200);
        resetTickCount();
        resetYaw(); //Reset the gyro angle to 0 for the next movement

        //State transition logic
        switch (currentState)
        {
            case DRIVING_LONG:
                if (current_pass >= total_passes) {
                    currentState = FINISHED;
                } else {
                    currentState = TURNING_1;
                    
                    if (current_turn_right) {
                        setTargetRPM(SPEED_TURN_RPM, -SPEED_TURN_RPM); 
                    } else {
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
                
                if (current_turn_right) {
                    setTargetRPM(SPEED_TURN_RPM, -SPEED_TURN_RPM);
                } else {
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