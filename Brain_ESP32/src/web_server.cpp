#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "config.h"
#include "web_server.h"
#include "grid_control.h"
#include "drive_system.h"

WebServer server(80);

//Internal Helpers

void handleFileRequest()
{
    String path = server.uri();
    if(path.endsWith("/")) path += "index.html";

    String contentType = "text/plain";
    if (path.endsWith(".html")) contentType = "text/html";
    else if (path.endsWith(".css")) contentType = "text/css";
    else if (path.endsWith(".js")) contentType = "application/javascript";

    if (LittleFS.exists(path))
    {
        File file = LittleFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
    }
    else
    {
        server.send(404, "text/plain", "404: File Not Found");
    }
}

void handleMove()
{
    Serial.println("Received move request");
    //Safety Check- ignore manual commands if grid is running
    if (isGridRunning())
    {
        server.send(409, "application/json", "{\"error\":\"Robot is in AutoPilot Mode. Stop grid first.\"}");
        return;
    }
    
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"error\":\"Body missing\"}");
        return;
    }
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error)
    {
        server.send(400,"application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    //Get Manual Control Commands
    int speedRPM = doc["speed"] | 1000;
    const char* cmd = doc["direction"];

    if (strcmp(cmd, "FORWARD") == 0) setTargetRPM(speedRPM, speedRPM);
    else if (strcmp(cmd, "BACKWARD") == 0) setTargetRPM(-speedRPM, -speedRPM);
    else if (strcmp(cmd, "LEFT") == 0) setTargetRPM(speedRPM, -speedRPM);
    else if (strcmp(cmd, "RIGHT") == 0) setTargetRPM(-speedRPM, speedRPM);
    else if (strcmp(cmd, "STOP") == 0) stopAll();
    
    server.send(200, "application/json", "{\"status\":\"success\"}");
}

//Public Init Function
void initWebServer()
{
    //initialize file system
    if (!LittleFS.begin())
    {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    
    //start WiFi Access Point
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    //Define Routes
    server.on("/api/robot/move", HTTP_POST, handleMove);
    server.onNotFound(handleFileRequest);

    //Start Server
    server.begin();
    Serial.println("HTTP server started");

    //Handle Grid Upload/Run
    server.on("/api/robot/upload", HTTP_POST, handleUpload);
    server.on("/api/robot/start", HTTP_POST, handleStartGrid);
}

void handleClient()
{
    server.handleClient();
}

//Handler For Upload Settings
void handleUpload()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "application/json", "{\"error\":\"No Data\"}");
        return;
    }

    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg("plain"));

    float len = doc["length"];
    float width = doc["width"];
    int passes = doc["passes"];
    String dir = doc["direction"];

    configureGrid(len, width, passes, (dir == "right"));

    server.send(200, "application/json", "{\"status\":\"configured\"}");
}

//Handler for Grid Start
void handleStartGrid()
{
    startGridRun();
    server.send(200, "application/json", "{\"status\":\"started\"}");
}