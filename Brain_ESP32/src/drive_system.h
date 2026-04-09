#pragma once

void initDriveSystem();
void updateDriveSystem(); 
void setTargetRPM(float leftRPM, float rightRPM);
void stopAll();

long getTicksLeft();
long getTicksRight();
void resetTickCount();