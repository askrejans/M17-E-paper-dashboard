// JSON payload -> DashboardData. Used by push mode (POST /update), pull mode
// (rendered Home Assistant template) and the preview tools. See TECHNICAL.md
// for the schema; payload v1 (original fixed rooms object) is still accepted.
#ifndef PAYLOAD_H
#define PAYLOAD_H

#include "data.h"
#include <ArduinoJson.h>

// Read a number from int/float/numeric-string; NAN for anything else.
float jf(JsonVariantConst v);

// Replace `d` with the content of `doc`. Sections missing from the document are
// left empty (NAN / zero count). Returns false when `doc` is not an object.
bool parsePayload(JsonVariantConst doc, DashboardData &d, const char *dateFormat);

// Merge a Home Assistant `weather.get_forecasts` list into d.forecast,
// skipping entries dated `todayIso` (YYYY-MM-DD) or earlier.
void parseHaForecast(JsonArrayConst list, DashboardData &d, const char *todayIso);

// Fill d.hourly from a Home Assistant hourly forecast list.
void parseHaHourly(JsonArrayConst list, DashboardData &d);

// Serialise data back to JSON (API /api/state, web dashboard).
void dataToJson(const DashboardData &d, JsonObject out);

#endif
