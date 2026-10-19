// Admin panel. Every control maps to a device API call (see TECHNICAL.md).
"use strict";

let cfg = {}, meta = { widgets: [], themes: [] }, status = {};

const THEME_INFO = {
  classic: "Original M17 layout: gradient header, titled boxes",
  vfd: "80s hi-fi fluorescent display: 7-seg, meters, spectrum",
  terminal: "Split-pane Unix session, one command per widget",
  printout: "Fanfold line-printer report, red ribbon alerts",
};
const TH_LABELS = {
  out_temp_low: "Outdoor temp low °C", out_temp_high: "Outdoor temp high °C", wind: "Wind high", gust: "Gust high",
  in_temp_low: "Indoor temp low °C", in_temp_high: "Indoor temp high °C", in_hum_low: "Indoor humidity low %", in_hum_high: "Indoor humidity high %",
  down_low: "Download low Mbps", up_low: "Upload low Mbps", ping_high: "Ping high ms", rssi_low: "Wi-Fi RSSI low dBm",
  price_high: "Price high", power_high: "Power high W", volt_low: "Voltage low V", volt_high: "Voltage high V",
  freq_low: "Frequency low Hz", freq_high: "Frequency high Hz", pf_low: "Power factor low (0-1)", month_high: "Month kWh high",
};

// ── dotted-path helpers ─────────────────────────────────────────
const getK = (o, k) => k.split(".").reduce((a, p) => (a == null ? a : a[p]), o);
function setK(o, k, v) {
  const parts = k.split(".");
  let a = o;
  for (const p of parts.slice(0, -1)) a = a[p] ??= {};
  a[parts.at(-1)] = v;
}

function bindInputs() {
  for (const el of $$("[data-k]")) {
    const v = getK(cfg, el.dataset.k);
    if (el.type === "checkbox") el.checked = !!v;
    else el.value = v ?? "";
    if (el.classList.contains("ent")) el.setAttribute("list", "entities");
    el.onchange = el.oninput = () => {
      let val = el.type === "checkbox" ? el.checked : el.value;
      if (el.type === "number" || el.dataset.k.startsWith("thresholds.") || /quiet_/.test(el.dataset.k)) val = Number(val);
      setK(cfg, el.dataset.k, val);
      if (el.dataset.k.startsWith("power.")) powerHint();
    };
  }
}

// ── tabs ───────────────────────────────────────────────────────
$$("#tabs button").forEach((b) => (b.onclick = () => {
  $$("#tabs button").forEach((x) => x.classList.toggle("on", x === b));
  $$("section.tab").forEach((s) => s.classList.toggle("on", s.id === "tab-" + b.dataset.tab));
  location.hash = b.dataset.tab;
  if (b.dataset.tab === "display") preview();
}));
if (location.hash) $(`#tabs button[data-tab="${location.hash.slice(1)}"]`)?.click();

// ── save ───────────────────────────────────────────────────────
async function save(sections) {
  const body = {};
  for (const s of sections) body[s] = cfg[s];
  try {
    const r = await api("/api/config", { body });
    toast("Saved" + (r.reboot_required ? " — reboot to apply network changes" : ""));
    if (r.reboot_required) banner("Network settings changed. <button class='b warn' data-action='reboot'>Reboot now</button>");
    await load();
  } catch (e) { toast("Save failed: " + e.message, true); }
}
$$("[data-save]").forEach((b) => (b.onclick = () => save(b.dataset.save.split(","))));

function banner(html) {
  $("#banners").innerHTML = `<div class="banner" style="margin-bottom:16px">${html}</div>`;
  bindActions();
}

// ── actions ────────────────────────────────────────────────────
function bindActions() {
  $$("[data-action]").forEach((b) => (b.onclick = async () => {
    const a = b.dataset.action;
    if (a === "reboot" && !confirm("Reboot the device?")) return;
    if (a === "sleep" && !confirm("Put the device to sleep now? The admin panel will be unreachable until the next wake.")) return;
    try { await api("/api/action", { body: { action: a } }); toast(`${a}: OK`); } catch (e) { toast(e.message, true); }
    setTimeout(refreshStatus, 800);
  }));
}
bindActions();
$("#maint").onchange = async (e) => {
  try { await api("/api/action", { body: { action: e.target.checked ? "maintenance_on" : "maintenance_off" } }); refreshStatus(); }
  catch (err) { toast(err.message, true); }
};

// ── manual pull ────────────────────────────────────────────────
$$(".pull-now").forEach((b) => (b.onclick = async () => {
  const outs = $$(".pull-out");
  if (status && status.ha_configured === false) {
    outs.forEach((o) => (o.innerHTML = '<span class="alert">Set the Home Assistant URL and token first (Data source tab) and save.</span>'));
    return;
  }
  $$(".pull-now").forEach((x) => (x.disabled = true));
  outs.forEach((o) => (o.textContent = "Pulling from Home Assistant…"));
  try {
    const j = await job("/api/ha/fetch", { method: "POST" }, 45000);
    const ok = j.body && j.body.ok;
    outs.forEach((o) => (o.innerHTML = `<span class="${ok ? "vfd" : "alert"}">${esc(j.body ? j.body.message : "HTTP " + j.code)}</span>`));
    toast(ok ? "Pulled from Home Assistant" : "Pull failed", !ok);
  } catch (e) {
    outs.forEach((o) => (o.innerHTML = `<span class="alert">${esc(e.message)}</span>`));
  }
  $$(".pull-now").forEach((x) => (x.disabled = false));
  refreshStatus();
}));

// ── status & logs ──────────────────────────────────────────────
async function refreshStatus() {
  try {
    status = await api("/api/status");
  } catch (e) { $("#stat").innerHTML = `<div><b>error</b><span class="alert">${esc(e.message)}</span></div>`; return; }
  const s = status;
  const items = [
    ["Firmware", `${s.fw} (${s.partition})`], ["Mode", s.mode === "setup" ? `setup AP ${s.ap}` : s.power_mode], ["Source", s.source],
    ["Last data", `${ago(s.last_update_age)} · ${s.last_source || "-"}`], ["Wake", `#${s.wake_count} · ${s.wake_reason}`],
    ["Next wake in", s.power_mode === "always_on" ? "— (always on)" : `${Math.round(s.next_wake_s / 60)} min`], ["Wi-Fi", `${s.ssid} · ${s.rssi} dBm · ch ${s.channel}`],
    ["Address", `${s.ip} · ${s.hostname}.local`], ["Heap", `${Math.round(s.heap / 1024)} KB (min ${Math.round(s.heap_min / 1024)})`],
    ["Panel timing", `render ${s.render_ms} ms · SPI ${s.spi_ms} ms · refresh ${(s.panel_ms / 1000).toFixed(1)} s${s.render_skipped ? " · skipped (unchanged)" : ""}`],
    ["Home Assistant", s.source === "pull" ? (s.ha_ok ? `OK · ${s.ha_ms} ms` : "not fetched / failed") : "push"],
    ["Storage", `${Math.round(s.fs_used / 1024)}/${Math.round(s.fs_total / 1024)} KB`], ["Chip temp", `${s.chip_temp?.toFixed?.(1)} °C`],
    ["Awake", s.maintenance ? `maintenance ${Math.round(s.maintenance_left / 60)} min` : s.admin_window_left ? `admin window ${s.admin_window_left}s` : "normal"],
  ];
  if (s.last_error) items.push(["Last error", `<span class="alert">${esc(s.last_error)}</span>`]);
  $("#stat").innerHTML = items.map(([k, v]) => `<div><b>${k}</b><span>${typeof v === "string" && v.startsWith("<span") ? v : esc(v)}</span></div>`).join("");
  $("#up").textContent = `${Math.floor(s.uptime / 3600)}h ${Math.floor((s.uptime % 3600) / 60)}m`;
  $("#maint").checked = s.maintenance;
  const ann = [["ONLINE", true, ""], ["SETUP", s.mode === "setup", "amb"], [s.source.toUpperCase(), true, ""], ["MAINT", s.maintenance, "amb"], ["AUTH", s.auth, ""], ["ERROR", !!s.last_error, "red"]];
  $("#ann").innerHTML = ann.map(([t, on, c]) => `<span class="${on ? "on " + c : ""}">${t}</span>`).join("");
  const t = new Date((s.time || 0) * 1000);
  $("#clock").innerHTML = seg(s.time > 1.6e9 ? `${String(t.getHours()).padStart(2, "0")}:${String(t.getMinutes()).padStart(2, "0")}` : "--:--", 44);
  $("#authState").innerHTML = s.auth
    ? `Login is <span class="vfd">enabled</span> for user <code>${esc(cfg.device?.admin_user)}</code>.`
    : `<span class="amber">The admin panel is open (no password).</span> Set a password to protect it with HTTP Basic auth.`;
  if (!s.auth && !$("#banners").innerHTML) banner(`Admin panel is not password protected. <button class="b warn" onclick="$('#tabs button[data-tab=security]').click()">Set a password</button>`);
}

async function refreshLog() {
  if (!$("#logAuto").checked || !$("#tab-status").classList.contains("on")) return;
  try {
    const t = await api("/api/logs");
    const el = $("#log"), atEnd = el.scrollTop + el.clientHeight >= el.scrollHeight - 20;
    el.textContent = t;
    if (atEnd) el.scrollTop = el.scrollHeight;
  } catch { /* ignore */ }
}

// ── display: themes, slots, preview ────────────────────────────
function renderThemes() {
  $("#themes").innerHTML = meta.themes.map((t) => `<button data-t="${t.id}" class="${cfg.display.theme === t.id ? "on" : ""}">${esc(t.name)}<small>${esc(THEME_INFO[t.id] || "")}</small></button>`).join("");
  $$("#themes button").forEach((b) => (b.onclick = () => { cfg.display.theme = b.dataset.t; renderThemes(); preview(); }));
  const opts = meta.widgets.map((w) => `<option value="${w}">${w}</option>`).join("");
  $("#slots").innerHTML = cfg.display.slots.map((s, i) => `<label class="f">Slot ${i + 1}<select data-slot="${i}">${opts}</select></label>`).join("");
  $$("#slots select").forEach((sel) => {
    sel.value = cfg.display.slots[sel.dataset.slot];
    sel.onchange = () => { cfg.display.slots[sel.dataset.slot] = sel.value; preview(); };
  });
}

let pvTimer;
function preview() {
  clearTimeout(pvTimer);
  pvTimer = setTimeout(() => {
    const q = new URLSearchParams({ theme: cfg.display.theme, slots: cfg.display.slots.join(","), lang: cfg.device.language, t: Date.now() });
    const img = $("#preview");
    img.style.opacity = 0.4;
    img.onload = () => (img.style.opacity = 1);
    img.onerror = () => { img.style.opacity = 1; toast("Preview unavailable (panel busy?)", true); };
    img.src = "/api/preview.bmp?" + q;
  }, 350);
}
$("#pvBtn").onclick = preview;

// ── source: mode, lists, entity search ─────────────────────────
function seg2(id, key, onChange) {
  const el = $(id);
  const sync = () => $$("button", el).forEach((b) => b.classList.toggle("on", b.dataset.v === getK(cfg, key)));
  $$("button", el).forEach((b) => (b.onclick = () => { setK(cfg, key, b.dataset.v); sync(); onChange?.(); }));
  sync();
}

function srcHint() {
  $("#srcHint").innerHTML = cfg.source.mode === "pull"
    ? "The device wakes, asks Home Assistant for everything in one template call (plus forecasts), draws and sleeps. Needs URL + token. Recommended."
    : "Home Assistant POSTs JSON to the device on its own schedule (rest_command). The device wakes slightly before each slot and listens.";
  $("#pushUrl").textContent = `http://${status.ip || location.host}/update`;
}

const LISTS = {
  rooms: { fields: [["name", "Room name"], ["temp", "Temperature sensor", "sensor"], ["hum", "Humidity sensor", "sensor"]], max: 8 },
  family: { fields: [["name", "Name (empty = HA name)"], ["entity", "person.* / device_tracker.*", "person"]], max: 6 },
  extras: { fields: [["name", "Label (empty = HA name)"], ["entity", "Any entity", ""]], max: 8 },
};

function renderList(kind) {
  const def = LISTS[kind], arr = (cfg.source.map[kind] ??= []);
  $(`#list-${kind}`).innerHTML = arr.map((row, i) => `<div class="item ${def.fields.length === 2 ? "p2" : ""}">${def.fields
    .map(([k, ph, dom]) => `<input placeholder="${ph}" value="${esc(row[k])}" data-i="${i}" data-f="${k}" ${dom !== undefined && k !== "name" ? `class="ent" list="entities" data-dom="${dom}"` : ""}>`)
    .join("")}<button class="b danger" data-del="${i}">✕</button></div>`).join("") || `<div class="hint">none${kind === "family" ? " — auto-discover uses all person.* entities" : ""}</div>`;
  $$(`#list-${kind} input`).forEach((el) => (el.oninput = () => (arr[el.dataset.i][el.dataset.f] = el.value)));
  $$(`#list-${kind} [data-del]`).forEach((b) => (b.onclick = () => { arr.splice(b.dataset.del, 1); renderList(kind); }));
  bindEntitySearch();
}
$$("[data-add]").forEach((b) => (b.onclick = () => {
  const kind = b.dataset.add, def = LISTS[kind], arr = (cfg.source.map[kind] ??= []);
  if (arr.length >= def.max) return toast(`max ${def.max}`, true);
  arr.push(Object.fromEntries(def.fields.map(([k]) => [k, ""])));
  renderList(kind);
}));

let entTimer, entBusy = false;
function bindEntitySearch() {
  $$("input.ent").forEach((el) => {
    el.addEventListener("input", () => {
      clearTimeout(entTimer);
      if (el.value.length < 2) return;
      entTimer = setTimeout(async () => {
        if (entBusy) return;
        entBusy = true;
        try {
          const q = new URLSearchParams({ q: el.value, domain: el.dataset.dom || "" });
          const j = await job("/api/ha/entities?" + q, {}, 15000);
          if (j.code === 200 && Array.isArray(j.body))
            $("#entities").innerHTML = j.body.map((e) => `<option value="${esc(e.id)}">${esc(e.name)} · ${esc(e.state)} ${esc(e.unit)}</option>`).join("");
        } catch { /* HA not configured yet */ }
        entBusy = false;
      }, 450);
    });
  });
}

$("#haTest").onclick = async (e) => {
  e.target.disabled = true;
  $("#haTestOut").textContent = "testing…";
  try {
    const j = await job("/api/ha/test", { body: { ha_url: cfg.source.ha_url, ha_token: cfg.source.ha_token, insecure_tls: cfg.source.insecure_tls } });
    $("#haTestOut").innerHTML = `<span class="${j.body.ok ? "vfd" : "alert"}">${esc(j.body.message)}</span>`;
  } catch (err) { $("#haTestOut").innerHTML = `<span class="alert">${esc(err.message)}</span>`; }
  e.target.disabled = false;
};

$("#tplTest").onclick = async () => {
  $("#tplPanel").style.display = "";
  $("#tplOut").value = "Rendering on Home Assistant (save first to test unsaved changes)…";
  try {
    const j = await job("/api/ha/render", { method: "POST" });
    $("#tplOut").value = j.code === 200 ? JSON.stringify(typeof j.body === "string" ? JSON.parse(j.body) : j.body, null, 2) : `HTTP ${j.code}\n${typeof j.body === "string" ? j.body : JSON.stringify(j.body)}`;
  } catch (e) { $("#tplOut").value = "Error: " + e.message; }
};
$("#tplShow").onclick = async () => {
  $("#tplPanel").style.display = "";
  $("#tplOut").value = await api("/api/ha/template");
};

// ── alerts ─────────────────────────────────────────────────────
function renderThresholds() {
  $("#th").innerHTML = Object.keys(cfg.thresholds).map((k) => `<label class="f">${esc(TH_LABELS[k] || k)}<input type="number" step="any" data-k="thresholds.${k}"></label>`).join("");
}

// ── network & power ────────────────────────────────────────────
function powerHint() {
  const p = cfg.power, perDay = Math.round((24 * 60) / p.interval_min);
  const quiet = p.quiet_start >= 0 ? ` Quiet hours ${p.quiet_start}:00–${p.quiet_end}:00.` : "";
  $("#pwrHint").textContent = p.mode === "always_on"
    ? `Always on: refreshes every ${p.interval_min} min (pull) or whenever HA pushes; admin always reachable.${quiet}`
    : `Deep sleep: wakes every ${p.interval_min} min (~${perDay} refreshes/day, each ~20 s of panel waveform).${quiet} Push mode: HA must send on the same schedule.`;
}
$("#presets").innerHTML = [5, 10, 15, 30, 60, 120].map((m) => `<button class="b" style="padding:4px 8px" data-m="${m}">${m}</button>`).join("");
$$("#presets button").forEach((b) => (b.onclick = (e) => { e.preventDefault(); cfg.power.interval_min = +b.dataset.m; $("[data-k='power.interval_min']").value = b.dataset.m; powerHint(); }));
const hours = (off) => (off ? '<option value="-1">off</option>' : "") + Array.from({ length: 24 }, (_, h) => `<option value="${h}">${String(h).padStart(2, "0")}:00</option>`).join("");
$("#qs").innerHTML = hours(true);
$("#qe").innerHTML = hours(false);

$("#scan").onclick = async (e) => {
  e.target.disabled = true;
  $("#scanOut").textContent = "scanning…";
  try {
    const j = await job("/api/wifi/scan", { method: "POST" }, 20000);
    $("#ssids").innerHTML = j.body.map((n) => `<option value="${esc(n.ssid)}">${n.rssi} dBm${n.secure ? " 🔒" : ""}</option>`).join("");
    $("#scanOut").textContent = `${j.body.length} networks — pick one in the SSID field`;
  } catch (err) { $("#scanOut").textContent = err.message; }
  e.target.disabled = false;
};

// ── security ───────────────────────────────────────────────────
$("#pwSave").onclick = async () => {
  const p1 = $("#pw1").value, p2 = $("#pw2").value;
  if (p1 !== p2) return toast("Passwords do not match", true);
  if (p1 && p1.length < 6) return toast("Use at least 6 characters", true);
  const dev = { admin_user: cfg.device.admin_user || "admin" };
  if (p1) dev.admin_password = p1;
  try {
    await api("/api/config", { body: { device: dev } });
    toast(p1 ? "Login saved — your browser will ask for it now" : "Username saved");
    $("#pw1").value = $("#pw2").value = "";
    setTimeout(load, 500);
  } catch (e) { toast(e.message, true); }
};
$("#pwClear").onclick = async () => {
  if (!confirm("Remove the admin password? The panel will be open to anyone on your network.")) return;
  try { await api("/api/config", { body: { device: { admin_password: "" } } }); toast("Password removed"); load(); }
  catch (e) { toast(e.message, true); }
};

// ── system ─────────────────────────────────────────────────────
$("#fwGo").onclick = () => {
  const f = $("#fw").files[0];
  if (!f) return toast("Choose firmware.bin first", true);
  if (!confirm(`Install ${f.name} (${Math.round(f.size / 1024)} KB)?`)) return;
  const fd = new FormData();
  fd.append("firmware", f, f.name);
  const x = new XMLHttpRequest();
  x.open("POST", "/api/ota");
  x.upload.onprogress = (e) => ($("#fwBar").style.width = `${(100 * e.loaded) / e.total}%`);
  x.onload = () => {
    let r = {};
    try { r = JSON.parse(x.responseText); } catch { /* */ }
    toast(r.message || `HTTP ${x.status}`, x.status !== 200);
    if (x.status === 200) setTimeout(() => location.reload(), 9000);
  };
  x.onerror = () => toast("Upload failed", true);
  x.send(fd);
};
$("#cfgImport").onclick = async () => {
  const f = $("#cfgFile").files[0];
  if (!f) return toast("Choose a config JSON first", true);
  try { await api("/api/config", { body: await f.text() }); toast("Configuration imported"); load(); }
  catch (e) { toast(e.message, true); }
};
$("#factory").onclick = async () => {
  if (!confirm("Erase all settings and restart?")) return;
  await api("/api/action", { body: { action: "factory_reset" } });
  toast("Resetting…");
};

// ── load ───────────────────────────────────────────────────────
async function load() {
  [cfg, meta] = await Promise.all([api("/api/config"), api("/api/meta")]);
  $("#name").textContent = cfg.device.name;
  document.title = `${cfg.device.name} · Admin`;
  renderThresholds();
  bindInputs();
  renderThemes();
  ["rooms", "family", "extras"].forEach(renderList);
  seg2("#srcMode", "source.mode", srcHint);
  seg2("#pwrMode", "power.mode", powerHint);
  srcHint();
  powerHint();
  bindEntitySearch();
}

load().then(refreshStatus).then(() => { srcHint(); refreshLog(); }).catch((e) => toast(e.message, true));
setInterval(refreshStatus, 5000);
setInterval(refreshLog, 4000);
