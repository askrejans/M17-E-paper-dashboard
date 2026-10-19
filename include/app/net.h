// Wi-Fi station / setup access point / mDNS.
#pragma once

#include <Arduino.h>
#include "settings.h"

bool netConnect(const Settings &s, uint32_t timeoutMs);
// Open setup AP "<name>-Setup-XXXX" with a captive DNS that points every
// hostname at the admin panel.
void netStartSetupAp(const Settings &s);
bool netIsSetupAp();
void netLoop();             // captive DNS processing
void netStartMdns(const Settings &s);
const char *netApName();
String netIp();
