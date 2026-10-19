// TERMINAL — the dashboard as a split-pane Unix session (tmux-style status
// lines, one shell command per widget). White text on black, yellow prompts,
// red for anything outside its threshold.
#include "display/theme.h"
#include "display/theme_common.h"
#include "display/drawing.h"
#include "display/glyphs.h"
#include "utils/utils.h"
#include "lang.h"

static const int LINE = 16;
static const int TOP = 24;
static const int BOTTOM = EPD_HEIGHT - 22;
static const int PANE_W = 384;
static const int COLS = PANE_W / 8;

struct Pane
{
  int x, y;
  bool full() const { return y + LINE > BOTTOM - 4; }
};

// Red text is hard to read on black: values stay white, alerts get a red dot
// in the left margin of their line.
static int fg(bool) { return WHITE; }

static void put(Pane &p, int col, const char *s, int c)
{
  if (p.full())
    return;
  char clip[64];
  utf8Clip(clip, sizeof(clip), s, COLS - col);
  text(p.x + col * 8, p.y, clip, c);
}

static void nl(Pane &p) { p.y += LINE; }

static void mark(const Pane &p, bool alert)
{
  if (alert && !p.full())
    fillCircle(p.x + 6, p.y + 8, 3, RED);
}

static void prompt(Pane &p, const Settings &s, const char *cmd)
{
  if (p.full())
    return;
  char host[24];
  strlcpy(host, s.name, sizeof(host));
  for (char *c = host; *c; c++)
    if (*c >= 'A' && *c <= 'Z')
      *c += 32;
  char pr[32];
  snprintf(pr, sizeof(pr), "%s$ ", host);
  text(p.x, p.y, pr, YELLOW);
  put(p, utf8Len(pr), cmd, WHITE);
  nl(p);
}

// "label ........ value" with value right-aligned in the pane
static void kv(Pane &p, const char *label, const char *value, bool alert, int indent = 2)
{
  if (p.full())
    return;
  int vl = utf8Len(value);
  char lab[40];
  utf8Clip(lab, sizeof(lab), label, COLS - indent - vl - 2);
  text(p.x + indent * 8, p.y, lab, WHITE);
  int from = indent + utf8Len(lab) + 1, to = COLS - vl - 1;
  for (int c = from; c < to; c++)
    px(p.x + c * 8 + 3, p.y + 12, WHITE);
  text(p.x + (COLS - vl) * 8, p.y, value, fg(alert));
  mark(p, alert);
  nl(p);
}

static void cmdFor(const char *id, char *out, size_t n)
{
  struct C { const char *id, *cmd; };
  static const C cmds[] = {
      {"outdoor", "weather --now"}, {"indoor", "sensors --rooms"}, {"forecast", "forecast --days 5"},
      {"hourly", "forecast --hourly 24"}, {"family", "who"}, {"network", "speedtest --last"},
      {"power", "cat /proc/power"}, {"prices", "nordpool --today"}, {"house", "hass status"},
      {"calendar", "calendar --next"}, {"extras", "hass states"}, {"agenda", "date; sun"},
      {"system", "uptime"}};
  for (const C &c : cmds)
    if (!strcmp(c.id, id))
    {
      strlcpy(out, c.cmd, n);
      return;
    }
  strlcpy(out, id, n);
}

static void block(Pane &p, const char *id, const DashboardData &d, const Settings &s)
{
  WidgetView v;
  if (!buildWidget(id, d, s, v))
    return;
  char cmd[40];
  cmdFor(id, cmd, sizeof(cmd));
  prompt(p, s, cmd);
  if (v.empty && strcmp(id, "system") != 0)
  {
    put(p, 2, lang.label_no_data, YELLOW);
    nl(p);
    p.y += LINE / 2;
    return;
  }

  if (!strcmp(id, "outdoor"))
  {
    // figlet-ish banner: big temperature + icon
    if (!p.full())
    {
      char b[24];
      snprintf(b, sizeof(b), "%s%s", v.hero, strcmp(v.hero, "--") ? "\xC2\xB0" "C" : "");
      textScaled(p.x + 16, p.y, b, fg(v.heroAlert), 3);
      icon(p.x + PANE_W - 40, p.y + 8, v.heroIcon, YELLOW);
      p.y += 48;
      put(p, 2, v.caption, YELLOW);
      nl(p);
    }
    for (int i = 0; i < v.n; i++)
      kv(p, v.rows[i].label, v.rows[i].value, v.rows[i].alert);
  }
  else if (v.kind == WK_TABLE)
  {
    char h[64];
    snprintf(h, sizeof(h), "%-26s%8s%8s", "NAME", "TEMP", "HUM");
    put(p, 2, h, YELLOW);
    nl(p);
    for (int i = 0; i < v.n && !p.full(); i++)
    {
      const WRow &r = v.rows[i];
      char nm[32];
      utf8Clip(nm, sizeof(nm), r.label, 22);
      put(p, 2, nm, fg(r.alert));
      text(p.x + (COLS - 14 - utf8Len(r.value)) * 8, p.y, r.value, fg(r.alert));
      text(p.x + (COLS - utf8Len(r.value2)) * 8, p.y, r.value2, fg(r.alert));
      mark(p, r.alert);
      nl(p);
    }
  }
  else if (v.kind == WK_FORECAST)
  {
    for (int i = 0; i < v.n && i < 5 && !p.full(); i++)
    {
      const WRow &r = v.rows[i];
      const ForecastDay &f = d.forecast[i];
      char b[64];
      snprintf(b, sizeof(b), "%-3s %-18.18s", r.label, translateWeatherState(f.condition));
      put(p, 2, b, WHITE);
      char t[24];
      snprintf(t, sizeof(t), "%s %s", r.value, r.value2);
      text(p.x + (COLS - utf8Len(t)) * 8, p.y, t, WHITE);
      nl(p);
    }
  }
  else if (v.kind == WK_PEOPLE)
  {
    for (int i = 0; i < v.n && !p.full(); i++)
    {
      const WRow &r = v.rows[i];
      char nm[16];
      strlcpy(nm, r.label, sizeof(nm));
      for (char *c = nm; *c; c++)
        if (*c >= 'A' && *c <= 'Z')
          *c += 32;
      char b[64];
      snprintf(b, sizeof(b), "%-10s %s%d", nm, r.on ? "tty" : "pts/", i + 1);
      put(p, 2, b, WHITE);
      char st[32];
      snprintf(st, sizeof(st), r.on ? "%s" : "(%s)", r.value);
      text(p.x + (COLS - utf8Len(st)) * 8, p.y, st, r.on ? WHITE : YELLOW);
      nl(p);
    }
  }
  else if (v.kind == WK_CHART || v.kind == WK_HOURLY)
  {
    for (int i = 0; i < v.n && i < 3; i++)
      if (v.rows[i].label[0])
        kv(p, v.rows[i].label, v.rows[i].value, v.rows[i].alert);
    if (p.y + 5 * LINE < BOTTOM)
    {
      if (v.kind == WK_CHART)
        drawPriceBars(v, p.x + 16, p.y + 4, PANE_W - 24, 3 * LINE, WHITE, RED, YELLOW);
      else
        drawHourlyCurve(d.hourly, v.sMin, v.sMax, p.x + 16, p.y + 4, PANE_W - 24, 3 * LINE, WHITE, YELLOW,
                        YELLOW, YELLOW);
      p.y += 5 * LINE;
    }
  }
  else
  {
    for (int i = 0; i < v.n; i++)
    {
      const WRow &r = v.rows[i];
      if (!r.value[0] || !r.label[0])
      {
        char b[64];
        snprintf(b, sizeof(b), "%s%s", r.label, r.value);
        put(p, 2, b, fg(r.alert));
        mark(p, r.alert);
        nl(p);
      }
      else
        kv(p, r.label, r.value, r.alert);
    }
  }
  p.y += LINE / 2;
}

static void statusLine(int y, const char *left, const char *right)
{
  fillRect(0, y, EPD_WIDTH, 20, YELLOW);
  text(8, y + 2, left, BLACK);
  textRight(EPD_WIDTH - 8, y + 2, right, BLACK);
}

void themeTerminal(const DashboardData &d, const Settings &s)
{
  fill(BLACK);
  StatusFlags f = computeStatus(d, s);
  char l[96], r[96];
  snprintf(l, sizeof(l), "[0] %s:~  tty0%s", s.name, d.stale ? "  [STALE]" : "");
  snprintf(r, sizeof(r), "%s %s %s", weekdayName(d, 0), d.date, d.time);
  statusLine(0, l, r);

  vlineDotted(EPD_WIDTH / 2, TOP, BOTTOM - TOP, YELLOW, 2, 2);
  Pane left = {8, TOP + 2}, right = {EPD_WIDTH / 2 + 8, TOP + 2};
  for (int i = 0; i < DASH_SLOTS; i++)
    block(i < 3 ? left : right, s.slots[i], d, s);

  // live cursor in the right pane
  Pane &cur = right.full() ? left : right;
  if (!cur.full())
  {
    prompt(cur, s, "");
    cur.y -= LINE;
    char host[24];
    snprintf(host, sizeof(host), "%s$ ", s.name);
    fillRect(cur.x + utf8Len(host) * 8, cur.y + 2, 8, 13, WHITE);
  }

  snprintf(l, sizeof(l), " wake #%lu  %s  %ddBm  heap %luK", (unsigned long)d.sys.wakeCount,
           d.sys.ip[0] ? d.sys.ip : "--", (int)d.sys.rssi, (unsigned long)(d.sys.heapFree / 1024));
  snprintf(r, sizeof(r), "alerts:%d  home:%d/%d  v%s", f.alerts, f.home, f.people, FW_VERSION);
  statusLine(EPD_HEIGHT - 20, l, r);
}
