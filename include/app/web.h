// HTTP server: live dashboard, admin panel and JSON API (see TECHNICAL.md).
#pragma once

#include <ESPAsyncWebServer.h>

void webBegin(AsyncWebServer &server);
// true while an OTA upload runs or the admin panel was used recently, which
// keeps the device awake.
bool webBusy();
// Executes queued slow jobs (Home Assistant calls, Wi-Fi scans) on the loop task.
void webLoop();
