#pragma once

void initDriveSystem();
void updateDriveSystem(); 
void setTargetRPM(float leftRPM, float rightRPM);
void stopAll();

long getTicksLeft();
long getTicksRight();
void resetTickCount();

//Get Telemetry
float getCurrentRPMLeft();
float getCurrentRPMRight();

//Get Debub Targets
float getTargetRPMLeft();
float getTargetRPMRight();