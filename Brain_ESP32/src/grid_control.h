#pragma once

void configureGrid(float len, float width, int passes, bool start_right);
void startGridRun();
void stopGridRun();
void handleGrid();
bool isGridRunning();

//Grid Data Access for web_server.cpp to build the /api/robot/data response
int getCompletedPasses();
int getSamplesForPass(int pass);
float getGridReading(int pass, int sample);
int getMaxSamplesAnyPass();