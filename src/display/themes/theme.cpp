#include "display/theme.h"
#include "display/drawing.h"
#include "display/glyphs.h"
#include "lang.h"

static const Theme THEMES[] = {
    {"classic", themeClassic},
    {"vfd", themeVfd},
    {"terminal", themeTerminal},
    {"printout", themePrintout},
};

void renderDashboard(const DashboardData &d, const Settings &s, const char *themeId)
{
  const char *id = themeId && themeId[0] ? themeId : s.theme;
  for (const Theme &t : THEMES)
    if (strcmp(t.id, id) == 0)
    {
      t.render(d, s);
      return;
    }
  themeClassic(d, s);
}

// Shared frame for status screens: black top band with a yellow rule, a
// "punched card" corner and large centred title.
static void statusFrame(const char *title)
{
  fill(WHITE);
  fillRect(0, 0, EPD_WIDTH, 64, BLACK);
  hline(0, 64, EPD_WIDTH, YELLOW);
  hline(0, 65, EPD_WIDTH, YELLOW);
  hline(0, 67, EPD_WIDTH, RED);
  textScaled(24, 16, "M17", WHITE, 2);
  text(84, 24, "E-PAPER DASHBOARD", YELLOW);
  textRight(EPD_WIDTH - 24, 24, "FW " FW_VERSION, WHITE);
  textCenter(EPD_WIDTH / 2, 96, title, BLACK, 3);
  fillRectDither(0, EPD_HEIGHT - 12, EPD_WIDTH, 12, YELLOW, WHITE);
}

void renderSetupScreen(const char *apName, const char *url)
{
  statusFrame(lang.setup_title);
  int y = 170;
  text(80, y, lang.setup_step1, BLACK);
  textScaled(110, y + 26, apName, RED, 2);
  y += 90;
  text(80, y, lang.setup_step2, BLACK);
  textScaled(110, y + 26, url, RED, 2);
  y += 90;
  text(80, y, lang.setup_step3, BLACK);
}

void renderMessageScreen(const char *title, const char *line1, const char *line2, const char *url)
{
  statusFrame(title);
  if (line1)
    textCenter(EPD_WIDTH / 2, 200, line1, BLACK, 1);
  if (line2)
    textCenter(EPD_WIDTH / 2, 230, line2, BLACK, 1);
  if (url && url[0])
    textCenter(EPD_WIDTH / 2, 290, url, RED, 2);
}
