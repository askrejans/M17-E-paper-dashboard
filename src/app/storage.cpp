#include "app/storage.h"
#include "app/log.h"
#include <LittleFS.h>

static const char *CONFIG_PATH = "/config.json";
static const char *CONFIG_TMP = "/config.tmp";
static bool mounted = false;

bool storageBegin()
{
  // The partition is labelled "spiffs" in the stock tables; format on first use.
  mounted = LittleFS.begin(true, "/littlefs", 5, "spiffs");
  logf("FS: %s (%u/%u bytes used)", mounted ? "mounted" : "FAILED", mounted ? (unsigned)LittleFS.usedBytes() : 0,
       mounted ? (unsigned)LittleFS.totalBytes() : 0);
  return mounted;
}

bool loadSettings(Settings &s)
{
  settingsDefaults(s);
  if (!mounted || !LittleFS.exists(CONFIG_PATH))
  {
    logf("Config: none stored, using defaults");
    saveSettings(s);
    return false;
  }
  File f = LittleFS.open(CONFIG_PATH, "r");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err)
  {
    logf("Config: parse error %s, using defaults", err.c_str());
    return false;
  }
  settingsFromJson(s, doc.as<JsonVariantConst>());
  logf("Config: loaded (theme %s, source %s)", s.theme, s.sourceMode == SOURCE_PULL ? "pull" : "push");
  return true;
}

bool saveSettings(const Settings &s)
{
  if (!mounted)
    return false;
  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>(), false);
  // write-then-rename so a power cut never leaves a truncated config
  File f = LittleFS.open(CONFIG_TMP, "w");
  if (!f)
    return false;
  size_t n = serializeJson(doc, f);
  f.close();
  if (!n)
    return false;
  LittleFS.remove(CONFIG_PATH);
  bool ok = LittleFS.rename(CONFIG_TMP, CONFIG_PATH);
  logf("Config: saved (%u bytes) %s", (unsigned)n, ok ? "OK" : "FAILED");
  return ok;
}

void factoryReset()
{
  if (mounted)
    LittleFS.remove(CONFIG_PATH);
  logf("Config: factory reset");
}
