#include "config.h"
#include "grid_control.h"
#include "drive_system.h"
#include "mpu_handler.h"
#include "electrode_servo.h"
#include "voltage_reading.h"
#include <Arduino.h>
#include <string.h>

// Grid Params
float grid_len_ft = 0;
float grid_width_ft = 0;
int total_passes = 0;
bool turn_right_first = true;
float target_heading = 0.0;

/*
    Collected Sensor Data
    Readings are stored in the order the robot drives themm
    odd numbered pass are spatially reversed because they are drove backwards
    data is corrected in web_server.cpp when the data is serialised
*/
static float grid_readings[MAX_GRID_PASSES][MAX_GRID_SAMPLES];
static int samples_per_pass[MAX_GRID_PASSES];
static int total_completed_passes = 0;
static int current_sample_count = 0;
static long last_sample_ticks = 0;


// Heading Data Timer Variables
unsigned long lastHeadingSendTime = 0;
const int HEADING_SEND_INTERVAL = 100;

// Flag for Grid Active
bool isAutoPilotActive = false;

// State Machine Variables
enum State { IDLE, DRIVING_LONG, TURNING_1, DRIVING_SHORT, TURNING_2, FINISHED };
State currentState = IDLE;

int current_pass = 0;
long target_ticks = 0;
bool current_turn_right = true;

// Configuration
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
    if (isAutoPilotActive) return;

    isAutoPilotActive = true;
    currentState = DRIVING_LONG;
    current_pass = 1;
    current_turn_right = turn_right_first;

    //Reset Grid Data Collection
    memset(grid_readings, 0, sizeof(grid_readings));
    memset(samples_per_pass, 0, sizeof(samples_per_pass));
    total_completed_passes = 0;
    current_sample_count = 0;
    last_sample_ticks = 0;

    resetYaw();
    target_heading = 0.0;

    resetTickCount();
    target_ticks = grid_len_ft * TICKS_PER_FOOT;

    wheelDown(); // Ensure wheel is down at the start of a run
    setTargetRPM(SPEED_GRID_RPM, SPEED_GRID_RPM);

    Serial.printf("Grid Started: %.1f ft x %.1f ft, %d passes\n", grid_len_ft, grid_width_ft, total_passes);
}

void stopGridRun()
{
    currentState = IDLE;
    stopAll();
    wheelDown(); // Always lower wheel on stop/emergency
    isAutoPilotActive = false;
    Serial.println("Grid Stopped");
}

bool isGridRunning()
{
    return isAutoPilotActive;
}

//Data Acess (called by web_server.cpp)
int getCompletedPasses() { return total_completed_passes; }
int getSamplesForPass(int pass) { return samples_per_pass[pass]; }
float getGridReading(int pass, int sample) { return grid_readings[pass][sample]; }

int getMaxSamplesAnyPass()
{
    int max = 0;
    for (int p = 0; p < total_completed_passes; p++)
    {
        if (samples_per_pass[p] > max) max = samples_per_pass[p];
    }
    return max;
}

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

    // Movement and Heading Logic
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
                float headingError = target_heading - getYawAngle();

                float heading_Kp = 5.0;
                float rpmCorrection = headingError * heading_Kp;
                rpmCorrection = constrain(rpmCorrection, -40, 40);

                setTargetRPM(SPEED_GRID_RPM - rpmCorrection,
                             SPEED_GRID_RPM + rpmCorrection);
            }

            //Electrod Sampling - one reading every TICKS_PER_FOOT during long passes
            if (currentState == DRIVING_LONG && current_dist >= last_sample_ticks + TICKS_PER_FOOT && current_sample_count < MAX_GRID_SAMPLES)
            {
               last_sample_ticks = current_dist;
               
               int passIdx = current_pass - 1;
               float mv = readHalfCellPotential_mV();
               grid_readings[passIdx][current_sample_count] = mv;
               samples_per_pass[passIdx] = ++current_sample_count;

               //Keep total completed passes up to date so the API can return
               //live data while the run is still in progress
               if (current_pass > total_completed_passes)
               {
                total_completed_passes = current_pass;
               }
               Serial.printf("Pass %d | Sample %d | %.2f mV (%s)\n", current_pass, current_sample_count, mv, getASTMRiskLabel(mv));
            }
        }
    }
    
    else if (currentState == TURNING_1 || currentState == TURNING_2)
    {
        float degreesRemaining = abs(target_heading - getYawAngle());
        if (degreesRemaining <= 4.0)
            targetReached = true;
    }

    // State Transition Logic
    if (targetReached)
    {
        stopAll();
        unsigned long brakeTimer = millis();
        while (millis() - brakeTimer < 200)
        {
            updateMPU();
            delay(1);
        }
        resetTickCount();

        switch (currentState)
        {
            case DRIVING_LONG:
                if (current_pass >= total_passes)
                {
                    currentState = FINISHED;
                }
                else
                {
                    // Lift wheel before turning, give servo time to reach position
                    wheelUp();
                    delay(400);

                    currentState = TURNING_1;
                    if (current_turn_right)
                    {
                        target_heading -= 90.0;
                        setTargetRPM(SPEED_TURN_RPM, -SPEED_TURN_RPM);
                    }
                    else
                    {
                        target_heading += 90.0;
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
                }
                else
                {
                    target_heading += 90.0;
                    setTargetRPM(-SPEED_TURN_RPM, SPEED_TURN_RPM);
                }
                break;

            case TURNING_2:
                // Lower wheel after final turn, give servo time before driving
                wheelDown();
                delay(400);

                currentState = DRIVING_LONG;
                current_pass++;
                current_turn_right = !current_turn_right;
                target_ticks = grid_len_ft * TICKS_PER_FOOT;
                current_sample_count = 0;
                last_sample_ticks = 0;
                setTargetRPM(SPEED_GRID_RPM, SPEED_GRID_RPM);
                break;

            default:
                break;
        }
    }
}