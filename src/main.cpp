// M17 E-Paper Dashboard — entry point. See app/app.cpp for the state machine
// and TECHNICAL.md for the architecture.
#include <Arduino.h>
#include "app/app.h"

void setup() { appSetup(); }

void loop() { appLoop(); }
