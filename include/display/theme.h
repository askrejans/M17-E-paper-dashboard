// Theme registry. A theme renders the whole 800x480 framebuffer from data +
// settings; it never touches the panel driver (see app.cpp for that), so every
// theme can also be rendered on the host by tools/preview.
#ifndef THEME_H
#define THEME_H

#include "data.h"
#include "settings.h"

typedef void (*ThemeRenderFn)(const DashboardData &d, const Settings &s);

struct Theme
{
  const char *id;
  ThemeRenderFn render;
};

// Render the dashboard into `fb` with the configured theme (or `themeId`).
void renderDashboard(const DashboardData &d, const Settings &s, const char *themeId = nullptr);

// Full-screen status screens (shared look across themes)
void renderSetupScreen(const char *apName, const char *url);
void renderMessageScreen(const char *title, const char *line1, const char *line2, const char *url);

// Theme implementations
void themeClassic(const DashboardData &d, const Settings &s);
void themeVfd(const DashboardData &d, const Settings &s);
void themeTerminal(const DashboardData &d, const Settings &s);
void themePrintout(const DashboardData &d, const Settings &s);

#endif
