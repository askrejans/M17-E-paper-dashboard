#ifndef WEB_H
#define WEB_H

#include <ESPAsyncWebServer.h>

extern unsigned long last_update_ms;
extern volatile bool update_received;

void setupWebServer(AsyncWebServer& server);

#endif