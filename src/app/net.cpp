#include "app/net.h"
#include "app/log.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

static DNSServer dns;
static bool apMode = false;
static char apName[40];

bool netConnect(const Settings &s, uint32_t timeoutMs)
{
  if (!s.ssid[0])
  {
    logf("WiFi: no SSID configured");
    return false;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(s.hostname);
  WiFi.begin(s.ssid, s.wifiPass);
  logf("WiFi: connecting to '%s'", s.ssid);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - start > timeoutMs)
    {
      logf("WiFi: timeout (status %d)", (int)WiFi.status());
      WiFi.disconnect(true);
      return false;
    }
    delay(100);
  }
  logf("WiFi: connected %s RSSI %d ch %d in %lu ms", WiFi.localIP().toString().c_str(), WiFi.RSSI(),
       WiFi.channel(), millis() - start);
  return true;
}

void netStartSetupAp(const Settings &s)
{
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apName, sizeof(apName), "%s-Setup-%02X%02X", s.name[0] ? s.name : "M17", mac[4], mac[5]);
  for (char *c = apName; *c; c++)
    if (*c == ' ')
      *c = '-';
  WiFi.mode(WIFI_AP_STA); // STA side lets the admin scan for networks
  WiFi.softAP(apName);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  apMode = true;
  logf("AP: '%s' at %s", apName, WiFi.softAPIP().toString().c_str());
}

bool netIsSetupAp() { return apMode; }

void netLoop()
{
  if (apMode)
    dns.processNextRequest();
}

void netStartMdns(const Settings &s)
{
  if (MDNS.begin(s.hostname))
  {
    MDNS.addService("http", "tcp", 80);
    logf("mDNS: http://%s.local", s.hostname);
  }
}

const char *netApName() { return apName; }

String netIp()
{
  if (apMode)
    return WiFi.softAPIP().toString();
  return WiFi.localIP().toString();
}
