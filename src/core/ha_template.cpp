#include "ha_template.h"

bool validEntityId(const char *id)
{
  if (!id || !*id)
    return false;
  bool dot = false;
  for (const char *p = id; *p; p++)
  {
    char c = *p;
    if (c == '.')
    {
      if (dot || p == id || !p[1])
        return false;
      dot = true;
    }
    else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
      return false;
  }
  return dot;
}

// Jinja string literal with escaping, e.g. Living "A" -> "Living \"A\""
static std::string lit(const char *s)
{
  std::string o = "\"";
  for (; *s; s++)
  {
    if (*s == '"' || *s == '\\')
      o += '\\';
    if (*s == '\n' || *s == '\r')
      continue;
    o += *s;
  }
  return o + "\"";
}

// Numeric state (or attribute) -> JSON number or null
static std::string num(const char *entity, const char *attr = nullptr)
{
  if (!validEntityId(entity))
    return "null";
  std::string e = lit(entity);
  if (attr)
    return "{{ state_attr(" + e + ", " + lit(attr) + ") | float(none) | to_json }}";
  return "{{ states(" + e + ") | float(none) | to_json }}";
}

static void kv(std::string &o, const char *key, const std::string &val, bool comma = true)
{
  o += "\"";
  o += key;
  o += "\": ";
  o += val;
  if (comma)
    o += ", ";
}

std::string buildHaTemplate(const SourceMap &m)
{
  std::string o;
  o.reserve(3000);
  o += "{";
  kv(o, "date", "\"{{ now().strftime('%Y-%m-%d') }}\"");
  kv(o, "time", "\"{{ now().strftime('%H:%M') }}\"");
  kv(o, "epoch", "{{ now().timestamp() | int }}");

  // weather: explicit sensors win over weather entity attributes; with
  // auto-discovery an unmapped weather entity falls back to the first weather.*
  bool hasW = validEntityId(m.weather);
  if (!hasW && m.autoDiscover)
  {
    o += "{% set w_auto = (states.weather | map(attribute='entity_id') | list + [''])[0] %}";
    hasW = true;
  }
  std::string W = validEntityId(m.weather) ? lit(m.weather) : "w_auto";
  o += "\"weather\": {";
  kv(o, "state", hasW ? "{{ states(" + W + ") | to_json }}" : "\"\"");
  auto wattr = [&](const char *attr) -> std::string {
    if (!hasW)
      return "null";
    return "{{ state_attr(" + W + ", " + lit(attr) + ") | float(none) | to_json }}";
  };
  kv(o, "temp", validEntityId(m.outTemp) ? num(m.outTemp) : wattr("temperature"));
  kv(o, "humidity", validEntityId(m.outHum) ? num(m.outHum) : wattr("humidity"));
  kv(o, "pressure", wattr("pressure"));
  kv(o, "clouds", wattr("cloud_coverage"));
  kv(o, "wind", wattr("wind_speed"));
  kv(o, "gust", wattr("wind_gust_speed"), false);
  o += "}, ";

  o += "\"rooms\": [";
  for (int i = 0; i < m.roomCount; i++)
  {
    const Binding &b = m.rooms[i];
    if (i)
      o += ", ";
    o += "{";
    kv(o, "name", "{{ " + lit(b.name) + " | to_json }}");
    kv(o, "temp", num(b.a));
    kv(o, "hum", num(b.b), false);
    o += "}";
  }
  o += "], ";

  o += "\"family\": [";
  int fc = 0;
  for (int i = 0; i < m.familyCount; i++)
  {
    const Binding &b = m.family[i];
    if (!validEntityId(b.a))
      continue;
    if (fc++)
      o += ", ";
    std::string e = lit(b.a);
    o += "{";
    if (b.name[0])
      kv(o, "name", "{{ " + lit(b.name) + " | to_json }}");
    else
      kv(o, "name", "{{ state_attr(" + e + ", 'friendly_name') | to_json }}");
    kv(o, "state", "{{ states(" + e + ") | to_json }}", false);
    o += "}";
  }
  if (fc == 0 && m.autoDiscover)
    o += "{% for p in states.person | sort(attribute='name') %}"
         "{\"name\": {{ p.name.split(' ')[0] | to_json }}, \"state\": {{ p.state | to_json }}}"
         "{{ ',' if not loop.last }}{% endfor %}";
  o += "], ";

  o += "\"internet\": {";
  kv(o, "down", num(m.netDown));
  kv(o, "up", num(m.netUp));
  kv(o, "ping", num(m.netPing), false);
  o += "}, ";

  o += "\"power\": {";
  kv(o, "price", num(m.price));
  kv(o, "total_power", num(m.power));
  kv(o, "voltage", num(m.voltage));
  kv(o, "frequency", num(m.frequency));
  kv(o, "power_factor", num(m.factor));
  kv(o, "monthly_consumption", num(m.month));
  kv(o, "today_consumption", num(m.today), false);
  o += "}, ";

  // Price series: downsample 15-min (96) or 30-min (48) data to hourly means.
  if (validEntityId(m.priceSeries) && m.priceSeriesAttr[0])
  {
    std::string e = lit(m.priceSeries), a = lit(m.priceSeriesAttr);
    o += "\"prices\": {% set p = (state_attr(" + e + ", " + a + ") or []) | select('number') | list %}"
         "{% set step = ((p | length) / 24) | round(0, 'ceil') | int if (p | length) > 24 else 1 %}"
         "{\"now\": {{ (now().hour if step > 1 else (now().hour * (p | length) // 24)) if p else 0 }}, "
         "\"values\": [{% for i in range(0, p | length, step) %}"
         "{{ ((p[i:i + step] | sum) / (p[i:i + step] | length)) | round(4) }}{{ ',' if not loop.last }}"
         "{% endfor %}]}, ";
  }

  o += "\"extras\": [";
  int ec = 0;
  for (int i = 0; i < m.extraCount; i++)
  {
    const Binding &b = m.extras[i];
    if (!validEntityId(b.a))
      continue;
    if (ec++)
      o += ", ";
    std::string e = lit(b.a);
    o += "{";
    if (b.name[0])
      kv(o, "label", "{{ " + lit(b.name) + " | to_json }}");
    else
      kv(o, "label", "{{ state_attr(" + e + ", 'friendly_name') | to_json }}");
    kv(o, "value", "{{ states(" + e + ") | to_json }}");
    kv(o, "unit", "{{ (state_attr(" + e + ", 'unit_of_measurement') or '') | to_json }}", false);
    o += "}";
  }
  o += "], ";

  // Calendar: next/current event of each calendar entity (attributes), sorted.
  {
    std::string cals;
    if (m.calendars[0])
    {
      cals = "[";
      char buf[sizeof(m.calendars)];
      strlcpy(buf, m.calendars, sizeof(buf));
      int n = 0;
      for (char *tok = strtok(buf, ", "); tok; tok = strtok(nullptr, ", "))
        if (validEntityId(tok))
          cals += std::string(n++ ? ", " : "") + lit(tok);
      cals += "]";
    }
    else if (m.autoDiscover)
      cals = "states.calendar | map(attribute='entity_id') | list";
    if (!cals.empty())
    {
      o += "{% set ev = namespace(l=[]) %}{% for c in " + cals + " %}"
           "{% set st = state_attr(c, 'start_time') %}"
           "{% if st and state_attr(c, 'message') %}"
           "{% set ev.l = ev.l + [{'s': st, 'm': state_attr(c, 'message'), 'a': state_attr(c, 'all_day')}] %}"
           "{% endif %}{% endfor %}"
           "\"events\": [{% for e in (ev.l | sort(attribute='s'))[:5] %}"
           "{% set dt = as_datetime(e.s) %}"
           "{\"when\": {{ (('' if dt.date() == now().date() else dt.strftime('%a ')) ~ "
           "('' if e.a else dt.strftime('%H:%M'))) | trim | to_json }}, "
           "\"title\": {{ e.m | to_json }}}{{ ',' if not loop.last }}{% endfor %}], ";
    }
  }

  // House status: zero-config summary of the whole HA instance.
  if (m.autoDiscover)
  {
    o += "{% set op = states.binary_sensor | selectattr('state', 'eq', 'on') "
         "| selectattr('attributes.device_class', 'defined') "
         "| selectattr('attributes.device_class', 'in', ['door', 'window', 'garage_door', 'opening']) | list %}"
         "{% set lb = namespace(n=[]) %}"
         "{% for b in states.sensor | selectattr('attributes.device_class', 'defined') "
         "| selectattr('attributes.device_class', 'eq', 'battery') %}"
         "{% if b.state | is_number and b.state | float < 20 %}{% set lb.n = lb.n + [b.name] %}{% endif %}"
         "{% endfor %}"
         "{% set mp = states.media_player | selectattr('state', 'eq', 'playing') | list %}"
         "\"house\": {"
         "\"lights_on\": {{ states.light | selectattr('state', 'eq', 'on') | list | count }}, "
         "\"open_count\": {{ op | count }}, "
         "\"open\": {{ op | map(attribute='name') | list | to_json }}, "
         "\"low_battery_count\": {{ lb.n | count }}, "
         "\"low_battery\": {{ lb.n | to_json }}, "
         "\"updates\": {{ states.update | selectattr('state', 'eq', 'on') | list | count }}, "
         "\"media\": {{ (((mp[0].attributes.media_artist ~ ' - ') if mp[0].attributes.media_artist else '') "
         "~ (mp[0].attributes.media_title or mp[0].name)) | to_json if mp else '\"\"' }}"
         "}, ";
  }

  // Thermostat: mapped climate entity, or the first one with auto-discovery
  if (validEntityId(m.thermostat) || m.autoDiscover)
  {
    o += validEntityId(m.thermostat)
             ? "{% set th = " + lit(m.thermostat) + " %}"
             : "{% set th = (states.climate | map(attribute='entity_id') | list + [''])[0] %}";
    o += "{% if th %}\"thermostat\": {"
         "\"name\": {{ (state_attr(th, 'friendly_name') or '') | to_json }}, "
         "\"target\": {{ state_attr(th, 'temperature') | float(none) | to_json }}, "
         "\"current\": {{ state_attr(th, 'current_temperature') | float(none) | to_json }}, "
         "\"mode\": {{ states(th) | to_json }}, "
         "\"action\": {{ (state_attr(th, 'hvac_action') or '') | to_json }}}, {% endif %}";
  }

  if (validEntityId(m.sun))
  {
    std::string e = lit(m.sun);
    o += "\"sun\": {";
    kv(o, "rise", "\"{{ as_local(as_datetime(state_attr(" + e + ", 'next_rising'))).strftime('%H:%M') "
                  "if state_attr(" + e + ", 'next_rising') else '' }}\"");
    kv(o, "set", "\"{{ as_local(as_datetime(state_attr(" + e + ", 'next_setting'))).strftime('%H:%M') "
                 "if state_attr(" + e + ", 'next_setting') else '' }}\"", false);
    o += "}, ";
  }

  if (validEntityId(m.message))
  {
    std::string e = lit(m.message);
    if (strncmp(m.message, "calendar.", 9) == 0)
      kv(o, "message",
         "{{ ((as_datetime(state_attr(" + e + ", 'start_time')).strftime('%H:%M') ~ ' ' "
         "if state_attr(" + e + ", 'start_time') else '') ~ (state_attr(" + e + ", 'message') or '')) | to_json }}");
    else
      kv(o, "message", "{{ states(" + e + ") | to_json }}");
  }

  kv(o, "weather_entity", hasW ? "{{ " + W + " | to_json }}" : "\"\"");
  o += "\"v\": 2}";
  return o;
}

std::string buildEntitySearchTemplate(const char *query, const char *domain, int limit)
{
  std::string q;
  for (const char *p = query ? query : ""; *p; p++)
    q += (*p >= 'A' && *p <= 'Z') ? (char)(*p + 32) : *p;
  std::string d = domain ? domain : "";
  for (char c : d)
    if (!((c >= 'a' && c <= 'z') || c == '_'))
      return "[]";
  char lim[8];
  snprintf(lim, sizeof(lim), "%d", limit);
  return "[{% for s in states if (" + lit(q.c_str()) + " in s.entity_id or " + lit(q.c_str()) +
         " in (s.name | lower)) and (" + lit(d.c_str()) + " == '' or s.domain == " + lit(d.c_str()) +
         ") %}{% if loop.index <= " + lim + " %}{{ ',' if loop.index > 1 }}"
         "{\"id\": {{ s.entity_id | to_json }}, \"name\": {{ s.name | to_json }}, "
         "\"state\": {{ s.state | to_json }}, "
         "\"unit\": {{ (s.attributes.unit_of_measurement or '') | to_json }}}"
         "{% endif %}{% endfor %}]";
}
