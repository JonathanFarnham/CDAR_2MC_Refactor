#pragma once
#include <WebServer.h>

extern WebServer server; //Allow main.cpp to access the server

void initWebServer();
void handleClient(); //called in main loop
void handleUpload();
void handleStartGrid();
void handleFileUpload();