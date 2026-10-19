// Live dashboard: every widget as a responsive card, all values shown (VFD look).
"use strict";

const LANGS = {
  EN: {
    home: "Home", thermostat: "Thermostat", outdoor: "Outdoor", indoor: "Indoor", forecast: "Forecast", hourly: "Next 24 hours",
    people: "People", network: "Network", power: "Electricity", prices: "Energy price today", house: "House", calendar: "Calendar",
    extras: "Sensors", sun: "Sun & notes",
    atHome: "Home", away: "Away", now: "Now", humidity: "Humidity", pressure: "Pressure", wind: "Wind", gusts: "Gusts",
    clouds: "Clouds", high: "High", low: "Low", rain: "Rain", average: "Average",
    lights: "Lights on", open: "Open", allClosed: "All closed", battery: "Low battery", noneLow: "None", updates: "Updates pending",
    playing: "Playing", download: "Download", upload: "Upload", ping: "Ping", price: "Price", voltage: "Voltage",
    frequency: "Frequency", pf: "Power factor", todayKwh: "Today", month: "This month", cheapest: "Cheapest", dearest: "Most expensive",
    avg: "Average", mid: "mid", sunrise: "Sunrise", sunset: "Sunset", noData: "No data yet — waiting for Home Assistant", updated: "Updated",
    wake: "wake", signal: "Wi-Fi", never: "never", sAgo: " s ago", minAgo: " min ago", hAgo: " h ago", heating: "Heating", cooling: "Cooling", idle: "Idle", off: "Off",
    peopleHome: "at home", barHint: "Bars: speed relative to 1 Gbit/s",
    ann: { home: "Home", rain: "Rain", frost: "Frost", wind: "Wind", open: "Open", old: "Data old" },
    days: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"],
    daysLong: ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"],
    cond: { sunny: "Sunny", "clear-night": "Clear night", cloudy: "Cloudy", partlycloudy: "Partly cloudy", rainy: "Rainy", pouring: "Pouring",
      snowy: "Snowy", "snowy-rainy": "Sleet", fog: "Fog", hail: "Hail", windy: "Windy", "windy-variant": "Wind & clouds",
      lightning: "Lightning", "lightning-rainy": "Thunderstorm", exceptional: "Exceptional" },
  },
  LV: {
    home: "Mājas", thermostat: "Termostats", outdoor: "Ārā", indoor: "Telpās", forecast: "Prognoze", hourly: "Tuvākās 24 stundas",
    people: "Cilvēki", network: "Tīkls", power: "Elektrība", prices: "Elektrības cena šodien", house: "Māja", calendar: "Kalendārs",
    extras: "Sensori", sun: "Saule un piezīmes",
    atHome: "Mājās", away: "Prom", now: "Tagad", humidity: "Mitrums", pressure: "Spiediens", wind: "Vējš",
    gusts: "Brāzmas", clouds: "Mākoņi", high: "Max", low: "Min", rain: "Nokrišņi", average: "Vidēji",
    lights: "Ieslēgtas gaismas", open: "Atvērts", allClosed: "Viss aizvērts", battery: "Zema baterija",
    noneLow: "Nav", updates: "Atjauninājumi", playing: "Atskaņo", download: "Lejupielāde", upload: "Augšupielāde", ping: "Ping",
    price: "Cena", voltage: "Spriegums", frequency: "Frekvence", pf: "Jaudas koef.", todayKwh: "Šodien", month: "Šomēnes",
    cheapest: "Lētākā", dearest: "Dārgākā", avg: "Vidējā", mid: "vidēja", sunrise: "Saullēkts", sunset: "Saulriets",
    noData: "Vēl nav datu — gaidu Home Assistant", updated: "Atjaunots", wake: "pamošanās", signal: "Wi-Fi",
    never: "nekad", sAgo: " s atpakaļ", minAgo: " min atpakaļ", hAgo: " h atpakaļ",
    heating: "Silda", cooling: "Dzesē", idle: "Gaida", off: "Izslēgts", peopleHome: "mājās", barHint: "Joslas: ātrums līdz 1 Gbit/s",
    ann: { home: "Mājās", rain: "Lietus", frost: "Salna", wind: "Vējš", open: "Atvērts", old: "Veci dati" },
    days: ["Sv", "P", "O", "T", "C", "Pk", "S"],
    daysLong: ["Svētdiena", "Pirmdiena", "Otrdiena", "Trešdiena", "Ceturtdiena", "Piektdiena", "Sestdiena"],
    cond: { sunny: "Saulains", "clear-night": "Skaidra nakts", cloudy: "Mākoņains", partlycloudy: "Daļēji mākoņains", rainy: "Lietus",
      pouring: "Stiprs lietus", snowy: "Sniegs", "snowy-rainy": "Slapjš sniegs", fog: "Migla", hail: "Krusa", windy: "Vējains",
      "windy-variant": "Vējš un mākoņi", lightning: "Zibens", "lightning-rainy": "Pērkona negaiss", exceptional: "Īpaši apstākļi" },
  },
};
const ROOM_KEYS = { "@living": ["Living Room", "Viesistaba"], "@bedroom": ["Bedroom", "Guļamistaba"], "@kids": ["Kids Room", "Bērnu istaba"], "@bath": ["Bathroom", "Vannasistaba"], "@office": ["Office", "Ofiss"] };

let T = LANGS.EN, S = {}, TH = {};

// ── helpers ────────────────────────────────────────────────────
const out = (v, lo, hi) => has(v) && (v < lo || v > hi);
const bad = (b) => (b ? " alert" : "");
const roomName = (n) => (ROOM_KEYS[n] ? ROOM_KEYS[n][S.language === "LV" ? 1 : 0] : n);
const wday = (d, off, long) => (d.weekday >= 0 ? (long ? T.daysLong : T.days)[(d.weekday + off) % 7] : "");
const cond = (c) => T.cond[c] || c || "";
const where = (s) => (s === "home" ? T.atHome : s === "not_home" || s === "away" ? T.away : s);
const agoT = (s) => (s < 0 ? T.never : s < 90 ? `${s}${T.sAgo}` : s < 5400 ? `${Math.round(s / 60)}${T.minAgo}` : `${Math.round(s / 3600)}${T.hAgo}`);
const deg = (v, d = 1) => (has(v) ? `${Number(v).toFixed(d)}°` : "--");

/** Weather icon from the panel's 24x24 bitmap, rendered as crisp pixels. */
function icon(state, size = 48) {
  const hex = ICONS[state] || ICONS.cloudy;
  let rects = "";
  for (let r = 0; r < 24; r++) {
    const row = parseInt(hex.substr(r * 6, 6), 16);
    let start = -1;
    for (let c = 0; c <= 24; c++) {
      const on = c < 24 && row & (1 << (23 - c));
      if (on && start < 0) start = c;
      if (!on && start >= 0) { rects += `<rect x="${start}" y="${r}" width="${c - start}" height="1"/>`; start = -1; }
    }
  }
  return `<svg class="wicon" viewBox="0 0 24 24" width="${size}" height="${size}" shape-rendering="crispEdges" role="img" aria-label="${esc(cond(state))}">${rects}</svg>`;
}

function card(title, body, opts = {}) {
  return `<section class="card ${opts.cls || ""}"><header><span class="legend">${esc(title)}</span>${opts.sum ? `<span class="sum">${opts.sum}</span>` : ""}</header><div class="body">${body}</div></section>`;
}
const kv = (k, v, alert = false) => `<div class="kv"><span class="k">${esc(k)}</span><span class="v${bad(alert)}">${v}</span></div>`;
const metric = (k, v, frac, alert = false, opts = {}) => `<div class="metric">${kv(k, v, alert)}${meter(frac, opts.n || 30, opts.hot ?? 2, opts.peak ?? -1)}</div>`;

function sparkline(values, W, H, minSpan = 0) {
  const vals = values.filter(has);
  if (vals.length < 2) return "";
  const lo = Math.min(...vals), hi = Math.max(...vals), span = Math.max(minSpan, hi - lo) || 1, n = values.length;
  const x = (i) => (i * W) / (n - 1), y = (v) => H - 3 - ((v - lo) / span) * (H - 6);
  const pts = values.map((v, i) => (has(v) ? `${x(i).toFixed(1)},${y(v).toFixed(1)}` : "")).filter(Boolean).join(" ");
  return `<svg class="spark" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none"><polyline points="${pts}"/></svg>`;
}

// ── widgets ────────────────────────────────────────────────────
function home(d) {
  const t = d.thermostat || {}, fam = d.family || [];
  if (!fam.length && !has(t.target)) return "";
  const inHome = fam.filter((p) => p.state === "home").length;
  const people = fam.map((p) => {
    const on = p.state === "home";
    return `<div class="person${on ? " on" : ""}"><span class="dot"></span><span class="pn">${esc(p.name)}</span><span class="pl">${esc(where(p.state))}</span></div>`;
  }).join("");
  let thermo = "";
  if (has(t.target)) {
    const act = { heating: T.heating, preheating: T.heating, cooling: T.cooling, idle: T.idle, off: T.off }[t.action] || { heat: T.heating, off: T.off }[t.mode] || t.action || t.mode || "";
    const heating = t.action === "heating" || t.action === "preheating";
    thermo = `<div class="thermo">
      <div><div class="k">${esc(T.thermostat)}${t.name ? ` · ${esc(t.name)}` : ""}</div><div class="hero">${seg(Number(t.target).toFixed(1), 54)}<span class="unit">°C</span></div></div>
      <div class="thermo-side">${kv(T.now, deg(t.current))}<span class="badge${heating ? " lit" : ""}">${esc(act)}</span></div></div>`;
  }
  const h = d.hourly && d.hourly.temp.length ? `<div class="mini"><div class="k">${esc(T.hourly)}</div>${sparkline(d.hourly.temp, 300, 50, 4)}</div>` : "";
  return card(T.home, `<div class="people">${people}</div>${thermo}${h}`, { sum: fam.length ? `${inHome}/${fam.length} ${T.peopleHome}` : "" });
}

function outdoor(d) {
  const w = d.weather;
  if (!has(w.temp) && !w.state) return "";
  const hot = out(w.temp, TH.out_temp_low, TH.out_temp_high);
  const temps = (d.hourly && d.hourly.temp.filter(has)) || [];
  const wmax = Math.max(TH.gust * 2, 40);
  return card(T.outdoor, `
    <div class="hero-row"><div class="hero${bad(hot)}">${seg(fmt(w.temp, 1), 76)}<span class="unit">°C</span></div><div class="wbig">${icon(w.state, 72)}<div class="amber">${esc(cond(w.state))}</div></div></div>
    ${temps.length ? `<div class="pair">${kv(T.high, deg(Math.max(...temps), 0))}${kv(T.low, deg(Math.min(...temps), 0))}</div>` : ""}
    ${metric(T.humidity, fmt(w.humidity, 0, "%"), w.humidity / 100)}
    ${metric(T.wind, fmt(w.wind, 1, S.wind_unit), w.wind / wmax, w.wind > TH.wind, { hot: TH.wind / wmax })}
    ${metric(T.gusts, fmt(w.gust, 1, S.wind_unit), w.gust / wmax, w.gust > TH.gust, { hot: TH.gust / wmax })}
    ${metric(T.pressure, fmt(w.pressure, 0, "hPa"), (w.pressure - 960) / 100)}
    ${metric(T.clouds, fmt(w.clouds, 0, "%"), w.clouds / 100)}`);
}

function hourly(d) {
  const h = d.hourly;
  if (!h || !h.temp.length) return "";
  const t = h.temp.filter(has), lo = Math.min(...t), hi = Math.max(...t), n = h.temp.length;
  const W = 720, H = 190, padL = 34, padB = 26, top = 18, span = Math.max(4, hi - lo);
  const x = (i) => padL + (i * (W - padL - 8)) / Math.max(1, n - 1);
  const y = (v) => top + (1 - (v - lo) / span) * (H - top - padB);
  let grid = "", bars = "", labels = "";
  for (let k = 0; k <= 4; k++) {
    const v = lo + (span * k) / 4, yy = y(v);
    grid += `<line x1="${padL}" x2="${W - 8}" y1="${yy}" y2="${yy}" class="gl"/><text x="${padL - 6}" y="${yy + 4}" text-anchor="end" class="ax">${v.toFixed(0)}°</text>`;
  }
  const rainMax = Math.max(2, ...h.precip.filter(has));
  h.precip.forEach((p, i) => {
    if (has(p) && p > 0) { const bh = (p / rainMax) * (H - top - padB) * 0.6; bars += `<rect x="${x(i) - 5}" y="${H - padB - bh}" width="10" height="${bh}" class="rain"><title>${p} mm</title></rect>`; }
  });
  for (let i = 0; i < n; i++) {
    const hr = h.start_hour >= 0 ? (h.start_hour + i) % 24 : i;
    if (hr % 3 === 0) labels += `<text x="${x(i)}" y="${H - 6}" text-anchor="middle" class="ax${hr === 0 ? " midnight" : ""}">${String(hr).padStart(2, "0")}</text>`;
    if (i % 3 === 0 && has(h.temp[i])) labels += `<text x="${x(i)}" y="${y(h.temp[i]) - 8}" text-anchor="middle" class="vl">${h.temp[i].toFixed(0)}°</text>`;
  }
  const pts = h.temp.map((v, i) => (has(v) ? `${x(i)},${y(v)}` : "")).filter(Boolean).join(" ");
  const rain = h.precip.filter(has).reduce((a, b) => a + b, 0);
  return card(T.hourly, `<svg class="chart big" viewBox="0 0 ${W} ${H}">${grid}${bars}<polyline points="${pts}" class="line"/>${labels}</svg>`,
    { cls: "wide", sum: `${deg(lo, 0)} … ${deg(hi, 0)} · ${T.rain} ${rain.toFixed(1)} mm` });
}

function forecast(d) {
  if (!d.forecast.length) return "";
  const days = d.forecast.map((f, i) => `<div class="day">
      <div class="dn">${esc(wday(d, i + 1))}</div>${icon(f.condition, 44)}<div class="dc">${esc(cond(f.condition))}</div>
      <div class="dt"><span class="vfd">${deg(f.temp_high, 0)}</span> <span class="dim">${deg(f.temp_low, 0)}</span></div>
      <div class="dp${f.precipitation > 0 ? " amber" : " dim"}">${has(f.precipitation) && f.precipitation > 0 ? f.precipitation.toFixed(1) + " mm" : "0 mm"}</div></div>`);
  return card(T.forecast, `<div class="days">${days.join("")}</div>`, { cls: "wide" });
}

function indoor(d) {
  if (!d.rooms.length) return "";
  const temps = d.rooms.map((r) => r.temp).filter(has);
  const rows = d.rooms.map((r) => {
    const isBath = r.name === "@bath";
    const tb = !isBath && out(r.temp, TH.in_temp_low, TH.in_temp_high), hb = out(r.hum, TH.in_hum_low, TH.in_hum_high);
    return `<div class="room"><span class="rn">${esc(roomName(r.name))}</span><span class="rt${bad(tb)}">${deg(r.temp)}</span>
      <span class="rh${bad(hb)}">${fmt(r.hum, 0, "%")}</span>${meter(r.hum / 100, 20, TH.in_hum_high / 100)}</div>`;
  });
  return card(T.indoor, `<div class="rooms">${rows.join("")}</div>`,
    { sum: temps.length ? `${T.average} ${deg(temps.reduce((a, b) => a + b, 0) / temps.length)}` : "" });
}

function priceChart(s, W, H, axis = false) {
  const v = s.values, n = v.length, hi = Math.max(...v), lo = Math.min(0, ...v);
  const sorted = [...v].sort((a, b) => a - b), cheap = sorted[Math.floor(n / 3)], dear = sorted[Math.floor((2 * n) / 3)];
  const x0 = axis ? 40 : 0, bw = (W - x0) / n, base = H - (axis ? 20 : 0);
  const hourOf = (i) => String(Math.round((i * 24) / n)).padStart(2, "0");
  let g = "";
  if (axis) g += `<text x="${x0 - 6}" y="12" text-anchor="end" class="ax">${hi.toFixed(2)}</text><text x="${x0 - 6}" y="${base}" text-anchor="end" class="ax">${lo.toFixed(2)}</text>`;
  v.forEach((p, i) => {
    const h = Math.max(2, ((p - lo) / (hi - lo || 1)) * (base - 6));
    const cls = i === s.now ? "now" : p <= cheap ? "cheap" : p >= dear ? "dear" : "mid";
    g += `<rect x="${x0 + i * bw + 1}" y="${base - h}" width="${Math.max(1, bw - 2)}" height="${h}" class="${cls}"><title>${hourOf(i)}:00 · ${p}</title></rect>`;
    if (axis && i % 3 === 0) g += `<text x="${x0 + i * bw + bw / 2}" y="${H - 5}" text-anchor="middle" class="ax">${hourOf(i)}</text>`;
  });
  return `<svg class="prices${axis ? " big" : ""}" viewBox="0 0 ${W} ${H}"${axis ? "" : ' preserveAspectRatio="none"'}>${g}</svg>`;
}

function power(d) {
  const p = d.power;
  if (!has(p.total_power) && !has(p.price)) return "";
  const pf = has(p.power_factor) && Math.abs(p.power_factor) > 1.5 ? p.power_factor / 100 : p.power_factor;
  const dec = has(p.price) && Math.abs(p.price) < 10 ? 3 : 2;
  const mini = d.prices && d.prices.values.length ? `<div class="mini-prices" title="${esc(T.prices)}">${priceChart(d.prices, 180, 64)}</div>` : "";
  return card(T.power, `
    <div class="hero-row"><div class="hero${bad(p.total_power > TH.power_high)}">${seg(fmt(p.total_power, 0), 60)}<span class="unit">W</span></div>${mini}</div>
    ${meter(p.total_power / TH.power_high, 40, 0.8)}
    <div class="grid2">
      ${kv(T.price, `${fmt(p.price, dec)} ${esc(S.price_unit)}`, p.price > TH.price_high)}
      ${kv(T.voltage, fmt(p.voltage, 0, "V"), out(p.voltage, TH.volt_low, TH.volt_high))}
      ${kv(T.frequency, fmt(p.frequency, 2, "Hz"), out(p.frequency, TH.freq_low, TH.freq_high))}
      ${kv(T.pf, fmt(pf, 2), pf < TH.pf_low)}
      ${kv(T.todayKwh, fmt(p.today_consumption, 1, "kWh"))}
      ${kv(T.month, fmt(p.monthly_consumption, 1, "kWh"), p.monthly_consumption > TH.month_high)}
    </div>`);
}

function prices(d) {
  const s = d.prices;
  if (!s || !s.values.length) return "";
  const v = s.values, n = v.length, hourOf = (i) => `${String(Math.round((i * 24) / n)).padStart(2, "0")}:00`;
  const minI = v.indexOf(Math.min(...v)), maxI = v.indexOf(Math.max(...v)), avg = v.reduce((a, b) => a + b, 0) / n;
  const dec = Math.abs(avg) < 10 ? 3 : 2, u = esc(S.price_unit);
  return card(T.prices, `
    <div class="hero-row"><div class="hero">${seg(v[s.now].toFixed(dec), 48)}<span class="unit">${u}</span></div>
      <div class="price-stats">${kv(T.cheapest, `${v[minI].toFixed(dec)} · ${hourOf(minI)}`)}${kv(T.dearest, `${v[maxI].toFixed(dec)} · ${hourOf(maxI)}`)}${kv(T.avg, avg.toFixed(dec))}</div></div>
    ${priceChart(s, 720, 170, true)}
    <div class="legend-row"><span class="sw cheap"></span>${esc(T.cheapest)} ⅓ <span class="sw mid"></span>${esc(T.mid)} <span class="sw dear"></span>${esc(T.dearest)} ⅓ <span class="sw now"></span>${esc(T.now)}</div>`,
    { cls: "wide", sum: `${T.now} ${hourOf(s.now)}` });
}

function network(d) {
  const n = d.internet;
  if (!has(n.down) && !has(n.up) && !has(n.ping)) return "";
  const lg = (v) => (has(v) && v > 0 ? Math.log10(v) / 3 : 0);
  const big = (v, u) => `${seg(fmt(v, has(v) && v < 10 ? 1 : 0), 30)} <span class="dim">${u}</span>`;
  return card(T.network, `
    ${metric("↓ " + T.download, big(n.down, "Mbps"), lg(n.down), n.down < TH.down_low)}
    ${metric("↑ " + T.upload, big(n.up, "Mbps"), lg(n.up), n.up < TH.up_low)}
    ${metric(T.ping, big(n.ping, "ms"), n.ping / (TH.ping_high * 2), n.ping > TH.ping_high, { hot: 0.5 })}
    <div class="hint">${esc(T.barHint)}</div>`);
}

function house(d) {
  const h = d.house;
  if (!h || (h.lights_on < 0 && h.open_count < 0 && h.low_battery_count < 0)) return "";
  const chips = (arr) => `<div class="chips">${arr.map((x) => `<span class="chip">${esc(x)}</span>`).join("")}</div>`;
  let b = "";
  if (h.lights_on >= 0) b += kv(T.lights, `<span class="vfd">${h.lights_on}</span>`);
  if (h.open_count >= 0) b += `<div class="block">${kv(T.open, h.open_count ? String(h.open_count) : T.allClosed, h.open_count > 0)}${h.open.length ? chips(h.open) : ""}</div>`;
  if (h.low_battery_count >= 0) b += `<div class="block">${kv(T.battery, h.low_battery_count ? String(h.low_battery_count) : T.noneLow, h.low_battery_count > 0)}${h.low_battery.length ? chips(h.low_battery) : ""}</div>`;
  if (h.updates > 0) b += kv(T.updates, `<span class="amber">${h.updates}</span>`);
  if (h.media) b += `<div class="block"><div class="k">♫ ${esc(T.playing)}</div><div class="amber">${esc(h.media)}</div></div>`;
  return card(T.house, b);
}

function calendar(d) {
  if (!d.events || !d.events.length) return "";
  return card(T.calendar, `<div class="events">${d.events.map((e) => `<div class="event"><span class="when vfd">${esc(e.when)}</span><span class="what">${esc(e.title)}</span></div>`).join("")}</div>`);
}

function extras(d) {
  if (!d.extras.length) return "";
  return card(T.extras, d.extras.map((e) => kv(e.label, `<span class="vfd">${esc(e.value)}</span> <span class="dim">${esc(e.unit)}</span>`)).join(""));
}

function sun(d) {
  if (!d.sun.rise && !d.sun.set && !d.message) return "";
  return card(T.sun, `<div class="pair">${kv(T.sunrise, `<span class="vfd">${esc(d.sun.rise || "--")}</span>`)}${kv(T.sunset, `<span class="vfd">${esc(d.sun.set || "--")}</span>`)}</div>
    ${d.message ? `<div class="message amber">${esc(d.message)}</div>` : ""}`);
}

// ── header ─────────────────────────────────────────────────────
function annunciators(d) {
  const w = d.weather, fam = d.family || [], homeN = fam.filter((f) => f.state === "home").length;
  const open = d.house && d.house.open_count > 0 ? d.house.open_count : 0, A = T.ann;
  const items = [
    [`${A.home} ${homeN}/${fam.length}`, homeN > 0, "", "People at home / tracked people"],
    [A.rain, /rain|pouring|lightning/.test(w.state || "") || (d.forecast[0] && d.forecast[0].precipitation >= 1), "", "Raining now, or ≥ 1 mm expected tomorrow"],
    [A.frost, has(w.temp) && w.temp <= 0, "", "Outdoor temperature at or below 0 °C"],
    [A.wind, w.gust > TH.gust || w.wind > TH.wind, "red", "Wind or gusts above the alert threshold"],
    [`${A.open} ${open}`, open > 0, "red", open ? d.house.open.join(", ") : "Doors / windows open"],
    [A.old, d.stale, "amb", "Home Assistant could not be reached; showing the last data"],
  ];
  $("#ann").innerHTML = items.map(([t, on, c, tip]) => `<span class="${on ? "on " + c : ""}" title="${esc(tip)}">${esc(t)}</span>`).join("");
}

async function refresh() {
  try {
    const st = await api("/api/state");
    const d = st.data;
    S = st.status;
    TH = S.thresholds || {};
    T = LANGS[S.language] || LANGS.EN;
    document.documentElement.lang = (S.language || "en").toLowerCase();
    document.title = `${S.name} · ${T.home}`;
    $("#name").textContent = S.name;
    $("#clock").innerHTML = seg(d.time || "--:--", 58);
    $("#date").textContent = d.date || "";
    $("#wd").textContent = wday(d, 0, true);
    annunciators(d);
    if (!d.valid) {
      $("#dash").innerHTML = `<div class="empty">${esc(T.noData)}</div>`;
    } else {
      $("#dash").innerHTML = [home, outdoor, hourly, indoor, forecast, power, prices, house, calendar, network, sun, extras]
        .map((f) => { try { return f(d); } catch (e) { console.error(f.name, e); return ""; } }).join("");
    }
    $("#upd").textContent = `${T.updated} ${agoT(S.last_update_age)} · FW ${S.fw} · ${T.wake} #${S.wake_count} · ${T.signal} ${S.rssi} dBm`;
  } catch (e) {
    $("#upd").innerHTML = `<span class="alert">offline — ${esc(e.message)}</span>`;
  }
}

refresh();
setInterval(refresh, 30000);
