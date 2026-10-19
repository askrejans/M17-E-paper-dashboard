// Shared helpers: SVG seven-segment numerals, segmented meters, API access.
"use strict";

const $ = (s, el = document) => el.querySelector(s);
const $$ = (s, el = document) => [...el.querySelectorAll(s)];
const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);
const has = (v) => v !== null && v !== undefined && !Number.isNaN(v);
const fmt = (v, d = 1, unit = "") => (has(v) ? Number(v).toFixed(d) + (unit ? " " + unit : "") : "--");

// ── seven segment ────────────────────────────────────────────────
// a=top b=upper-right c=lower-right d=bottom e=lower-left f=upper-left g=middle
const SEG = { 0: "abcdef", 1: "bc", 2: "abdeg", 3: "abcdg", 4: "bcfg", 5: "acdfg", 6: "acdefg", 7: "abc", 8: "abcdefg", 9: "abcdfg", "-": "g", E: "adefg", C: "adef", H: "bcefg", L: "def", P: "abefg", o: "cdeg" };

function segPaths(x, w, h, t) {
  const m = h / 2, k = t / 2;
  const H = (y) => `M${x + k},${y} l${k},${-k} h${w - 2 * t} l${k},${k} l${-k},${k} h${-(w - 2 * t)}z`;
  const V = (xx, y) => `M${xx},${y + k} l${k},${-k} l${k},${k} v${m - 2 * t} l${-k},${k} l${-k},${-k}z`;
  return { a: H(k), g: H(m), d: H(h - k), f: V(x, 0), b: V(x + w - t, 0), e: V(x, m - k), c: V(x + w - t, m - k) };
}

/** Render text as an SVG seven-segment display; unlit segments are ghosted. */
function seg(text, h = 48, cls = "vfd") {
  const s = String(text ?? "--");
  const w = h * 0.5, t = Math.max(3, h * 0.11), gap = h * 0.14;
  const ghost = h >= 36; // unlit segments only behind large numerals
  let x = 0, body = "";
  for (const ch of s) {
    if (ch === "." || ch === ":") {
      const r = t * 0.55;
      if (ch === ".") body += `<circle cx="${x + r}" cy="${h - r}" r="${r}" class="on"/>`;
      else body += `<circle cx="${x + r}" cy="${h * 0.3}" r="${r}" class="on"/><circle cx="${x + r}" cy="${h * 0.7}" r="${r}" class="on"/>`;
      x += r * 2 + gap * 0.6;
      continue;
    }
    if (ch === " ") { x += w * 0.6; continue; }
    if (ch === "°") { body += `<circle cx="${x + h * 0.12}" cy="${h * 0.14}" r="${h * 0.09}" fill="none" stroke-width="${t * 0.6}" class="on stroke"/>`; x += h * 0.3; continue; }
    const lit = SEG[ch] ?? "";
    const p = segPaths(x, w, h, t);
    for (const k of "abcdefg") if (ghost || lit.includes(k)) body += `<path d="${p[k]}" class="${lit.includes(k) ? "on" : "off"}"/>`;
    x += w + gap;
  }
  return `<svg class="seg ${cls}" viewBox="-2 -2 ${x + 4} ${h + 4}" height="${h}" aria-label="${esc(s)}"><style>.seg .on{fill:currentColor}.seg .stroke{fill:none;stroke:currentColor}.seg .off{fill:currentColor;opacity:.07}</style>${body}</svg>`;
}

/** Segmented meter. frac 0..1; hotFrom marks segments above it red; peak draws a red tick. */
function meter(frac, n = 24, hotFrom = 2, peak = -1) {
  frac = has(frac) ? Math.min(1, Math.max(0, frac)) : 0;
  const on = Math.round(frac * n), pk = peak >= 0 ? Math.round(Math.min(1, peak) * n) - 1 : -1;
  let s = '<div class="meter">';
  for (let i = 0; i < n; i++) s += `<i class="${i < on ? "on" + (i / n >= hotFrom ? " hot" : "") : i === pk ? "peak" : ""}"></i>`;
  return s + "</div>";
}

// ── API ──────────────────────────────────────────────────────────
async function api(path, opts = {}) {
  const init = { method: opts.method || (opts.body !== undefined ? "POST" : "GET"), headers: {} };
  if (opts.body !== undefined) {
    init.headers["Content-Type"] = "application/json";
    init.body = typeof opts.body === "string" ? opts.body : JSON.stringify(opts.body);
  }
  const r = await fetch(path, init);
  const ct = r.headers.get("content-type") || "";
  const data = ct.includes("json") ? await r.json() : await r.text();
  if (!r.ok && r.status !== 202) throw new Error((data && data.error) || `HTTP ${r.status}`);
  return data;
}

/** Start a background job on the device and wait for its result. */
async function job(path, opts = {}, timeoutMs = 30000) {
  const start = await api(path, opts);
  const id = start.job;
  const t0 = Date.now();
  while (Date.now() - t0 < timeoutMs) {
    await new Promise((r) => setTimeout(r, 400));
    const j = await api(`/api/job?id=${id}`);
    if (j.done) return j;
  }
  throw new Error("device did not answer in time");
}

function toast(msg, err = false) {
  let t = $(".toast");
  if (!t) { t = document.createElement("div"); t.className = "toast"; document.body.append(t); }
  t.textContent = msg;
  t.classList.toggle("err", err);
  t.style.opacity = 1;
  clearTimeout(toast.h);
  toast.h = setTimeout(() => (t.style.opacity = 0), err ? 6000 : 2600);
}

const ago = (s) => (s < 0 ? "never" : s < 90 ? `${s}s ago` : s < 5400 ? `${Math.round(s / 60)} min ago` : `${Math.round(s / 3600)} h ago`);
