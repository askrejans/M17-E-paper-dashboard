// Persistent settings on LittleFS (/config.json).
#pragma once

#include "settings.h"

bool storageBegin();
// Load settings; on first boot (no file) seeds defaults (incl. .env values)
// and writes them. Returns true when a stored config was found.
bool loadSettings(Settings &s);
bool saveSettings(const Settings &s);
void factoryReset();
