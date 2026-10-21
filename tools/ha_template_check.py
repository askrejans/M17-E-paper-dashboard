#!/usr/bin/env python3
"""Render the device's Home Assistant template in an HA-like Jinja environment.

Home Assistant evaluates the pull-mode template server-side, so a typo there
only shows up on a live system. This harness emulates the parts of HA's
template engine the firmware relies on (states, state_attr, now, to_json,
float(default), is_number, as_datetime, as_local, ...) with a small fake
house, renders the template the firmware generates and checks that the result
is valid payload-v2 JSON.

Usage:
    pio run -e preview
    python3 tools/ha_template_check.py [settings.json ...]

Each argument is a settings JSON (same format as the admin export); without
arguments a mapped and an auto-discovery configuration are checked. Requires
`jinja2` (pip install jinja2). Exit code 0 = all templates valid.
"""
import datetime as dt
import json
import os
import subprocess
import sys
import tempfile

from jinja2 import StrictUndefined, Undefined
from jinja2.sandbox import ImmutableSandboxedEnvironment

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROGRAM = os.path.join(ROOT, ".pio", "build", "preview", "program")
TZ = dt.timezone(dt.timedelta(hours=3))
NOW = dt.datetime(2026, 10, 19, 14, 31, tzinfo=TZ)


class State:
    def __init__(self, entity_id, state, **attributes):
        self.entity_id = entity_id
        self.domain = entity_id.split(".")[0]
        self.state = str(state)
        self.attributes = attributes
        self.name = attributes.get("friendly_name", entity_id.split(".")[1].replace("_", " ").title())


HOUSE = [
    State("weather.forecast_home", "partlycloudy", temperature=9.7, humidity=81, pressure=1012.3,
          cloud_coverage=64, wind_speed=19.8, wind_gust_speed=43.6, friendly_name="Forecast Home"),
    State("sun.sun", "above_horizon", next_rising="2026-10-20T04:58:11+00:00", next_setting="2026-10-19T15:12:40+00:00"),
    State("sensor.living_temperature", "21.8", unit_of_measurement="°C", device_class="temperature"),
    State("sensor.living_humidity", "46", unit_of_measurement="%", device_class="humidity"),
    State("sensor.bedroom_temperature", "unavailable", unit_of_measurement="°C"),
    State("sensor.speedtest_download", "457.23", unit_of_measurement="Mbit/s"),
    State("sensor.speedtest_upload", "92.66", unit_of_measurement="Mbit/s"),
    State("sensor.speedtest_ping", "12", unit_of_measurement="ms"),
    State("sensor.nordpool_kwh_lv", "0.112", unit_of_measurement="EUR/kWh",
          today=[round(0.05 + 0.002 * i, 4) for i in range(96)], friendly_name="Nord Pool"),
    State("sensor.power", "1322", unit_of_measurement="W"),
    State("sensor.co2", "812", unit_of_measurement="ppm", friendly_name="CO₂ office"),
    State("sensor.hall_motion_battery", "12", unit_of_measurement="%", device_class="battery", friendly_name="Hall motion"),
    State("sensor.phone_battery", "88", unit_of_measurement="%", device_class="battery"),
    State("person.alice_smith", "home", friendly_name="Alice Smith"),
    State("person.bob", "not_home", friendly_name="Bob"),
    State("person.diana", "Ķengarags", friendly_name="Diāna"),
    State("binary_sensor.garage_door", "on", device_class="garage_door", friendly_name="Garage door"),
    State("binary_sensor.kitchen_window", "off", device_class="window"),
    State("binary_sensor.motion", "on", device_class="motion"),
    State("light.kitchen", "on"), State("light.hall", "on"), State("light.porch", "off"),
    State("update.home_assistant_core", "on"),
    State("media_player.living", "playing", media_artist="Kraftwerk", media_title="Computer Love"),
    State("calendar.family", "off", message="Football \"practice\"", start_time="2026-10-19 18:00:00", all_day=False),
    State("calendar.school", "off", message="Recycling pickup", start_time="2026-10-21 00:00:00", all_day=True),
    State("input_text.notice", "Back at 19:00"),
    State("climate.living", "heat", temperature=21.5, current_temperature=20.8, hvac_action="heating",
          friendly_name="Living room"),
]


class States:
    def __init__(self, states):
        self._all = {s.entity_id: s for s in states}

    def __call__(self, entity_id):
        s = self._all.get(entity_id)
        return s.state if s else "unknown"

    def __iter__(self):
        return iter(sorted(self._all.values(), key=lambda s: s.entity_id))

    def __getattr__(self, domain):
        return [s for s in self if s.domain == domain]


_SENTINEL = object()


def make_env(house=None):
    env = ImmutableSandboxedEnvironment(undefined=Undefined, extensions=["jinja2.ext.loopcontrols"])
    states = States(HOUSE if house is None else house)

    def state_attr(entity_id, attr):
        s = states._all.get(entity_id)
        return s.attributes.get(attr) if s else None

    def to_float(value, default=_SENTINEL):
        try:
            return float(value)
        except (TypeError, ValueError):
            if default is _SENTINEL:
                raise ValueError(f"float got invalid input '{value}' and no default")
            return default

    def is_number(value):
        try:
            float(value)
            return not isinstance(value, bool)
        except (TypeError, ValueError):
            return False

    def as_datetime(value):
        if isinstance(value, dt.datetime):
            return value
        return dt.datetime.fromisoformat(str(value))

    def as_local(value):
        return value.astimezone(TZ) if value.tzinfo else value.replace(tzinfo=TZ)

    env.globals.update(states=states, state_attr=state_attr, now=lambda: NOW, as_datetime=as_datetime,
                       as_local=as_local, float=to_float)
    env.filters.update(float=to_float, to_json=lambda v: json.dumps(v, ensure_ascii=False), is_number=is_number,
                       as_datetime=as_datetime, as_local=as_local)
    import re
    env.tests.update(is_number=is_number, search=lambda v, pat: bool(re.search(pat, str(v))),
                     match=lambda v, pat: bool(re.match(pat, str(v))))
    return env


def template_for(settings_path):
    args = [PROGRAM, "--template"] + ([settings_path] if settings_path else [])
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


MAPPED = {
    "source": {"map": {
        "weather": "weather.forecast_home", "sun": "sun.sun", "auto_discover": True,
        "rooms": [{"name": "Living", "temp": "sensor.living_temperature", "hum": "sensor.living_humidity"},
                  {"name": "Bed \"room\"", "temp": "sensor.bedroom_temperature", "hum": ""}],
        "family": [{"name": "", "entity": "person.alice_smith"}, {"name": "Bob", "entity": "person.bob"}],
        "net_down": "sensor.speedtest_download", "net_up": "sensor.speedtest_upload", "net_ping": "sensor.speedtest_ping",
        "price": "sensor.nordpool_kwh_lv", "power": "sensor.power", "price_series": "sensor.nordpool_kwh_lv",
        "price_series_attr": "today", "extras": [{"name": "", "entity": "sensor.co2"}],
        "calendars": "calendar.family, calendar.school", "message": "input_text.notice"}}}
AUTO = {"source": {"map": {"weather": "", "sun": "sun.sun", "auto_discover": True}}}
MINIMAL = {"source": {"map": {"weather": "", "sun": "", "auto_discover": False}}}


def check(name, settings):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        json.dump(settings, fh)
        path = fh.name
    try:
        tpl = template_for(path)
        out = make_env().from_string(tpl).render()
        data = json.loads(out)
    except Exception as exc:  # noqa: BLE001 - report any failure
        print(f"FAIL {name}: {exc}")
        return False
    finally:
        os.unlink(path)
    problems = []
    for key in ("date", "time", "epoch", "weather", "rooms", "family", "internet", "power"):
        if key not in data:
            problems.append(f"missing '{key}'")
    if name != "minimal":
        if data["weather"]["temp"] != 9.7:
            problems.append("weather temperature")
        if not data["family"]:
            problems.append("no people")
        if data.get("house", {}).get("open") != ["Garage door"]:
            problems.append(f"house.open = {data.get('house', {}).get('open')}")
        if data.get("house", {}).get("low_battery") != ["Hall motion"]:
            problems.append("low battery list")
        if data.get("house", {}).get("media") != "Kraftwerk - Computer Love":
            problems.append("media")
    if name == "mapped":
        if data["rooms"][1]["temp"] is not None:
            problems.append("unavailable sensor must be null")
        if len(data["prices"]["values"]) != 24:
            problems.append(f"prices not downsampled to 24 ({len(data['prices']['values'])})")
        if data["events"][0]["title"] != 'Football "practice"':
            problems.append("calendar escaping")
        if data["events"][1]["when"] != "Wed":
            problems.append(f"all-day event label {data['events'][1]['when']!r}")
        if data["family"][0]["name"] != "Alice Smith":
            problems.append("friendly name fallback")
        if data["sun"]["rise"] != "07:58":
            problems.append(f"sunrise local time {data['sun']['rise']}")
        if data.get("thermostat", {}).get("target") != 21.5 or data["thermostat"]["action"] != "heating":
            problems.append(f"thermostat {data.get('thermostat')}")
        if data["message"] != "Back at 19:00":
            problems.append("message")
    if name == "auto":
        if data["weather_entity"] != "weather.forecast_home":
            problems.append("weather auto-discovery")
        if [p["name"] for p in data["family"]] != ["Alice", "Bob", "Diāna"]:
            problems.append(f"people auto-discovery {data['family']}")
    if problems:
        print(f"FAIL {name}: " + "; ".join(problems))
        return False
    print(f"ok   {name}: {len(tpl)} char template -> {len(out)} bytes JSON")
    return data


def check_package():
    """Render the push-mode package payload with fake forecast responses."""
    try:
        import yaml
    except ImportError:
        print("skip package: pip install pyyaml")
        return True
    pkg = yaml.safe_load(open(os.path.join(ROOT, "homeassistant", "m17_epaper.yaml"), encoding="utf-8"))
    seq = pkg["script"]["m17_epaper_push"]["sequence"]
    variables = dict(seq[0]["variables"])
    variables.update(weather_entity="weather.forecast_home", power="sensor.power",
                     rooms=[["Living", "sensor.living_temperature", "sensor.living_humidity"]],
                     internet=["sensor.speedtest_download", "sensor.speedtest_upload", "sensor.speedtest_ping"],
                     extras=[["CO2", "sensor.co2"]], message="input_text.notice")
    day = lambda d, c, t, lo, p: {"datetime": f"2026-10-{d}T00:00:00+03:00", "condition": c, "temperature": t, "templow": lo, "precipitation": p}
    variables["daily"] = {"weather.forecast_home": {"forecast": [day(19, "sunny", 10, 4, 0), day(20, "rainy", 9, 3, 4.2)]}}
    variables["hourly"] = {"weather.forecast_home": {"forecast": [
        {"datetime": f"2026-10-19T{12 + i:02d}:00:00+00:00", "temperature": 9 - i * 0.2, "precipitation": 0.1 * i} for i in range(6)]}}
    payload_tpl = next(a for a in seq if a.get("action") == "rest_command.m17_epaper_update")["data"]["payload"]
    try:
        data = json.loads(make_env().from_string(payload_tpl).render(**variables))
    except Exception as exc:  # noqa: BLE001
        print(f"FAIL package: {exc}")
        return False
    problems = []
    if [f["condition"] for f in data["forecast"]] != ["rainy"]:
        problems.append("forecast must skip today")
    if data["hourly"]["start_hour"] != 15 or len(data["hourly"]["temp"]) != 6:
        problems.append(f"hourly {data['hourly']}")
    if len(data["prices"]["values"]) != 24 or data["house"]["open"] != ["Garage door"]:
        problems.append("prices/house")
    if data["rooms"][0]["temp"] != 21.8 or data["power"]["voltage"] is not None:
        problems.append("numbers")
    if problems:
        print("FAIL package: " + "; ".join(problems))
        return False
    print(f"ok   package: push payload renders ({len(json.dumps(data))} bytes)")
    return True


def check_normal_openings():
    """Ordinary interior contacts need an active, unbypassed alarm cause."""
    contacts = [
        State("binary_sensor.office_door", "on", device_class="door", friendly_name="Office door"),
        State("binary_sensor.bedroom_door", "on", device_class="door", friendly_name="Bedroom door"),
    ]
    templates = {}
    for name, csv in (("default", ""), ("configured", "binary_sensor.office_door, binary_sensor.bedroom_door"),
                      ("invalid", "sensor.not_contact, bad id")):
        with tempfile.NamedTemporaryFile("w", suffix=".json") as fh:
            json.dump({"source": {"map": {"normal_openings": csv}}}, fh)
            fh.flush()
            templates[name] = template_for(fh.name)
    if templates["default"] != templates["invalid"]:
        print("FAIL normal openings: invalid-only configuration changed default template")
        return False

    def panel(state, opened=None, bypassed=None, entity_id="alarm_control_panel.home"):
        return State(entity_id, state, open_sensors=opened, bypassed_sensors=bypassed)

    office = "binary_sensor.office_door"
    bedroom = "binary_sensor.bedroom_door"
    perimeter = {"Garage door"}
    cases = [
        ("default blank", "default", [panel("disarmed")], perimeter | {"Office door", "Bedroom door"}),
        ("disarmed", "configured", [panel("disarmed")], perimeter),
        ("disarmed stale cause", "configured", [panel("disarmed", {office: "open"})], perimeter),
        ("armed without cause", "configured", [panel("armed_home")], perimeter),
        ("armed stale cause", "configured", [panel("armed_away", {office: "open"})], perimeter),
        ("pending unrelated cause", "configured", [panel("pending", {office + "_other": "open"})], perimeter),
        ("pending exact cause", "configured", [panel("pending", {office: None})], perimeter | {"Office door"}),
        ("triggered cause", "configured", [panel("triggered", {bedroom: "open"})], perimeter | {"Bedroom door"}),
        ("pending bypassed", "configured", [panel("pending", {office: "open"}, [office])], perimeter),
        ("triggered bypassed", "configured", [panel("triggered", {bedroom: "open"}, [bedroom])], perimeter),
        ("non-mapping cause", "configured", [panel("triggered", [office])], perimeter),
        ("perimeter retained", "configured", [panel("triggered", {"binary_sensor.garage_door": "open"},
                                                              ["binary_sensor.garage_door"])], perimeter),
        ("bypass scoped to causal panel", "configured",
         [panel("pending", {office: "open"}, [office]),
          panel("triggered", {office: "open"}, None, "alarm_control_panel.other")], perimeter | {"Office door"}),
    ]
    problems = []
    for name, template, panels, expected in cases:
        try:
            data = json.loads(make_env(HOUSE + contacts + panels).from_string(templates[template]).render())
            house = data["house"]
            if set(house["open"]) != expected or house["open_count"] != len(expected):
                problems.append(f"{name}: open={house['open']}, count={house['open_count']}")
        except Exception as exc:  # noqa: BLE001 - report each isolated scenario
            problems.append(f"{name}: {exc}")
    if problems:
        print("FAIL normal openings: " + "; ".join(problems))
        return False
    print(f"ok   normal openings: {len(cases)} isolated alarm/contact scenarios; no live HA calls")
    return True


def main():
    if not os.path.exists(PROGRAM):
        sys.exit("build the host tool first: pio run -e preview")
    if len(sys.argv) > 1:
        ok = all(check(p, json.load(open(p))) for p in sys.argv[1:])
    else:
        results = [check("mapped", MAPPED), check("auto", AUTO), check("minimal", MINIMAL)]
        ok = all(results) and check_package() and check_normal_openings()
        if results[0]:
            # feed the rendered payload through the firmware parser + renderer
            out = os.path.join(ROOT, "preview", "ha_rendered.json")
            os.makedirs(os.path.dirname(out), exist_ok=True)
            json.dump(results[0], open(out, "w"), ensure_ascii=False)
            subprocess.run([PROGRAM, out, "", os.path.join(ROOT, "preview", "ha")], check=True, capture_output=True)
            print("ok   rendered payload through the firmware renderer -> preview/ha/")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
