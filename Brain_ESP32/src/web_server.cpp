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
    //Safety Check ignore manual commands if grid is running
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

    int speedRPM = doc["speed"] | 300; // Defaulting to 300 RPM
    //fall back to "" when "direction" is missing so strcmp doesn't dereference a null pointer
    const char* cmd = doc["direction"] | "";

    if (strcmp(cmd, "FORWARD") == 0) setTargetRPM(speedRPM, speedRPM);
    else if (strcmp(cmd, "BACKWARD") == 0) setTargetRPM(-speedRPM, -speedRPM);
    else if (strcmp(cmd, "LEFT") == 0) setTargetRPM(-speedRPM, speedRPM);
    else if (strcmp(cmd, "RIGHT") == 0) setTargetRPM(speedRPM, -speedRPM);
    else if (strcmp(cmd, "STOP") == 0) stopAll();
    else
    {
        server.send(400, "application/json", "{\"error\":\"Unknown or missing direction\"}");
        return;
    }

    server.send(200, "application/json", "{\"status\":\"success\"}");
}

/*
    /api/robot/data
    Returns all collected half-cell potential readings as a 2D JSON Array
    ready for ContourMapGenerator.renderToCanvas().

    Serpentine correction is applied here as the robot drives in alternating passes
    in opposite spatial directions so odd indexed passes are reversed so that column 0
    always represents the same physical side of the survey area.
*/

void handleGetData()
{
    int passes = getCompletedPasses();
    int maxSamples = getMaxSamplesAnyPass();
    //32KB should be good for a 30x50 grid (1500floats * ~10chars + overhead)
    DynamicJsonDocument doc(32768);
    doc["isRunning"] = isGridRunning();
    doc["passes"] = passes;
    doc["samplesPerPass"] = maxSamples;

    JsonArray dataArr = doc.createNestedArray("data");

    for (int p = 0; p < passes; p++)
    {
        JsonArray row = dataArr.createNestedArray();
        int samplesInPass = getSamplesForPass(p);
        bool reversedPass = (p % 2 == 1);

        //Add the measured samples, applying serpentine correction
        if (reversedPass)
        {
            for (int s = samplesInPass - 1; s >= 0; s--)
            {
                row.add(getGridReading(p, s));
            }
        }
        else
        {
            for (int s = 0; s < samplesInPass; s++)
            {
                row.add(getGridReading(p, s));
            }
        }

        //Pad shorter rows like an incomplete final pass with the last value
        //So that the frontend always receives a rectangular array
        float padValue = (samplesInPass > 0) ? getGridReading(p, samplesInPass - 1) : 0.0f;
        for (int s = samplesInPass; s < maxSamples; s++)
        {
            row.add(padValue);
        }
    }
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
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
    server.on("/api/robot/upload", HTTP_POST, handleUpload);
    server.on("/api/robot/start", HTTP_POST, handleStartGrid);
    server.on("/api/robot/stop_grid", HTTP_POST, handleStopGrid);
    server.on("/api/robot/data", HTTP_GET, handleGetData);
    server.on("/api/robot/clear", HTTP_POST, handleClearData);    
    server.on("/api/robot/telemetry", HTTP_GET, handleTelemetry); 
    server.onNotFound(handleFileRequest);

    //Start Server
    server.begin();
    Serial.println("HTTP server started");
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

    //Reject bad length/width that is over max values
    if (len <= 0 || len > MAX_GRID_SAMPLES)
    {
        server.send(400, "application/json", "{\"error\":\"length must be between 1 and " + String(MAX_GRID_SAMPLES) + " ft\"}");
        return;
    }
    if (width <= 0 || width > 500)
    {
        server.send(400, "application/json", "{\"error\":\"width must be between 1 and 500 ft\"}");
        return;
    }
    if (passes < 1 || passes > MAX_GRID_PASSES)
    {
        server.send(400, "application/json", "{\"error\":\"passes out of range\"}");
        return;
    }
    String dir = doc["direction"] | "left";

    configureGrid(len, width, passes, (dir == "right"));

    server.send(200, "application/json", "{\"status\":\"configured\"}");
}

//Handler for Grid Start
void handleStartGrid()
{
    startGridRun();
    server.send(200, "application/json", "{\"status\":\"started\"}");
}

void handleStopGrid()
{
    stopGridRun();
    server.send(200, "application/json", "{\"status\":\"stopped\"}");
}

//Zero all stored grid readings so the next /data poll returns an empty set.
void handleClearData()
{
    if (isGridRunning())
    {
        server.send(409, "application/json",
            "{\"error\":\"Cannot clear while grid is running\"}");
        return;
    }
    clearGridData();
    server.send(200, "application/json", "{\"status\":\"cleared\"}");
}

//Live encoder/connection status for the manual control page.
void handleTelemetry()
{
    String json = "{\"ticksL\":" + String(getTicksLeft()) +
                  ",\"ticksR\":" + String(getTicksRight()) + "}";
    server.send(200, "application/json", json);
}