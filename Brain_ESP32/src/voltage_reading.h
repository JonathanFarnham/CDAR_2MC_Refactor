#pragma once
#include <Arduino.h>

//Uses HSPI bus to not conflict with the I2C MPU-6050
void initElectrodeSensor();
float readHalfCellPotential_mV();
const char* getASTMRiskLabel(float potential_mv);