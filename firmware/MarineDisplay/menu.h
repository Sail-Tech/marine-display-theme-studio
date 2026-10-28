// menu.h — pages, persisted settings and the on-screen menu model.
// The menu is drawn by pages.h (same look as the web simulator: black list, grey header,
// lime focus bar) and driven by the keys or by touch from ui.h.
#pragma once
#include <Preferences.h>
#include "theme.h"
#include "sim.h"

namespace ui {

enum Page : uint8_t { PG_WIND_QUAD, PG_SAIL_STEER, PG_LAYLINES, PG_AIS, PG_GPS, PG_COUNT };
static const char *const PAGE_NAME[PG_COUNT] = {"Wind Quad", "Sail Steer", "Laylines", "AIS", "GPS"};
static uint8_t page = PG_WIND_QUAD;
static bool needRedraw = true;          // set by anything that changes what the display shows
static bool needChrome = true;          // title strip / keys (colours, title, brightness)

static void saveSettings() {
  Preferences p; p.begin("mfd", false);
  p.putUChar("page", page);
  p.putBool("aisAuto", sim::ais::autoRange); p.putUChar("aisRange", sim::ais::rangeIdx);
  p.putFloat("aisCpa", sim::ais::cpaLimit); p.putFloat("aisTcpa", sim::ais::tcpaLimit);
  p.putBool("aisHide", sim::ais::hideSafe); p.putBool("aisTrack", sim::ais::tracking);
  p.end();
}
static void loadSettings() {
  Preferences p; p.begin("mfd", true);
  page = min<uint8_t>(p.getUChar("page", PG_WIND_QUAD), PG_COUNT - 1);
  sim::ais::autoRange = p.getBool("aisAuto", true);
  sim::ais::rangeIdx = min<uint8_t>(p.getUChar("aisRange", 2), sim::ais::NSTEPS - 1);
  sim::ais::cpaLimit = p.getFloat("aisCpa", 0.5f); sim::ais::tcpaLimit = p.getFloat("aisTcpa", 10);
  sim::ais::hideSafe = p.getBool("aisHide", false); sim::ais::tracking = p.getBool("aisTrack", true);
  p.end();
}
static void themeChanged() { theme::revision++; theme::save(); needRedraw = needChrome = true; }

}  // namespace ui

namespace menu {

struct Item {
  const char *label;
  uint8_t n;                               // number of options; 0 = information page
  const char *(*opt)(int i);
  int (*cur)();                            // current option, -1 = none
  void (*apply)(int i);
};

static char sbuf[24];
static const float CPA_OPT[] = {0.25f, 0.5f, 1.0f, 2.0f};
static const float TCPA_OPT[] = {5, 10, 15, 20, 30};
static const char *const SCHEMES[] = {"Day", "Night", "Amber night", "Monochrome"};
static const char *const SCHEME_KEY[] = {"day", "night", "amber", "mono"};
static const char *const SCENARIOS[] = {"Normal", "Heading unavailable", "Position unavailable", "AIS data unavailable"};
static const char *const OFF_ON[] = {"Off", "On"};

static const Item ITEMS[] = {
  {"Page", ui::PG_COUNT, [](int i) { return ui::PAGE_NAME[i]; }, []() { return (int)ui::page; },
   [](int i) { ui::page = i; ui::saveSettings(); }},
  {"Display scheme", 4, [](int i) { return SCHEMES[i]; },
   []() { return theme::T.night ? (theme::T.nightColor == 0xffb000 ? 2 : 1) : 0; },
   [](int i) { theme::applyPreset(SCHEME_KEY[i]); ui::themeChanged(); }},
  {"Brightness", 10, [](int i) { snprintf(sbuf, sizeof sbuf, "%d%%", (i + 1) * 10); return (const char *)sbuf; },
   []() { return (int)lroundf(theme::T.brightness / 10.0f) - 1; },
   [](int i) { theme::T.brightness = (i + 1) * 10; ui::themeChanged(); }},
  {"Text size", 9, [](int i) { snprintf(sbuf, sizeof sbuf, "%d%%", 70 + i * 10); return (const char *)sbuf; },
   []() { return (int)lroundf((theme::T.fontScale - 0.7f) * 10); },
   [](int i) { theme::T.fontScale = 0.7f + i * 0.1f; ui::themeChanged(); }},
  {"AIS range", sim::ais::NSTEPS + 1,
   [](int i) { if (!i) return "Auto"; snprintf(sbuf, sizeof sbuf, "%g NM", sim::ais::STEPS[i - 1]); return (const char *)sbuf; },
   []() { return sim::ais::autoRange ? 0 : sim::ais::rangeIdx + 1; },
   [](int i) { sim::ais::autoRange = !i; if (i) sim::ais::rangeIdx = i - 1; sim::ais::autoSince = 0; ui::saveSettings(); }},
  {"AIS alarm CPA", 4, [](int i) { snprintf(sbuf, sizeof sbuf, "%g NM", CPA_OPT[i]); return (const char *)sbuf; },
   []() { for (int i = 0; i < 4; i++) if (CPA_OPT[i] == sim::ais::cpaLimit) return i; return -1; },
   [](int i) { sim::ais::cpaLimit = CPA_OPT[i]; ui::saveSettings(); }},
  {"AIS alarm TCPA", 5, [](int i) { snprintf(sbuf, sizeof sbuf, "%g min", TCPA_OPT[i]); return (const char *)sbuf; },
   []() { for (int i = 0; i < 5; i++) if (TCPA_OPT[i] == sim::ais::tcpaLimit) return i; return -1; },
   [](int i) { sim::ais::tcpaLimit = TCPA_OPT[i]; ui::saveSettings(); }},
  {"Hide safe AIS vessels", 2, [](int i) { return OFF_ON[i]; }, []() { return (int)sim::ais::hideSafe; },
   [](int i) { sim::ais::hideSafe = i; ui::saveSettings(); }},
  {"AIS tracking", 2, [](int i) { return OFF_ON[i]; }, []() { return (int)sim::ais::tracking; },
   [](int i) { sim::ais::tracking = i; sim::ais::advance(0); ui::saveSettings(); }},
  {"Data scenario", 4, [](int i) { return SCENARIOS[i]; },
   []() { return !sim::valid.heading ? 1 : !sim::valid.position ? 2 : !sim::valid.ais ? 3 : 0; },
   [](int i) { sim::valid = sim::Valid(); if (i == 1) sim::valid.heading = false; if (i == 2) sim::valid.position = false;
               if (i == 3) sim::valid.ais = false; sim::ais::advance(0); }},
  {"Invert colours", 2, [](int i) { return OFF_ON[i]; }, []() { return (int)theme::T.invert; },
   [](int i) { theme::T.invert = i; ui::themeChanged(); }},
  {"Reset theme", 2, [](int i) { return i ? "Reset to defaults" : "Cancel"; }, []() { return 0; },
   [](int i) { if (i) { theme::resetAll(); ui::themeChanged(); } }},
  {"About", 0, nullptr, nullptr, nullptr},
};
static constexpr int N_ITEMS = sizeof(ITEMS) / sizeof(ITEMS[0]);
static constexpr int ROWS = 8;               // visible rows

static bool open = false;
static int sub = -1;                          // -1: root list, else the item whose options are shown
static int cursor = 0, scroll = 0;

static int count() { return sub < 0 ? N_ITEMS : ITEMS[sub].n; }
static const char *label(int i) { return sub < 0 ? ITEMS[i].label : ITEMS[sub].opt(i); }
static void clampScroll() {
  int n = count();
  cursor = constrain(cursor, 0, max(0, n - 1));
  if (cursor < scroll) scroll = cursor;
  if (cursor >= scroll + ROWS) scroll = cursor - ROWS + 1;
  scroll = constrain(scroll, 0, max(0, n - ROWS));
}
static void show() { open = true; sub = -1; cursor = scroll = 0; ui::needRedraw = true; }
static void close() { open = false; ui::needRedraw = true; }
static void back() { if (sub >= 0) { cursor = sub; sub = -1; clampScroll(); } else close(); ui::needRedraw = true; }
static void move(int d) { int n = count(); if (n) cursor = (cursor + d + n) % n; clampScroll(); ui::needRedraw = true; }
static void select() {
  if (sub < 0) {
    sub = cursor;
    int c = ITEMS[sub].n && ITEMS[sub].cur ? ITEMS[sub].cur() : 0;
    cursor = max(0, c); scroll = 0; clampScroll();
  } else if (ITEMS[sub].n) {
    ITEMS[sub].apply(cursor);
    cursor = sub; sub = -1; clampScroll();
  } else back();
  ui::needRedraw = true;
}

}  // namespace menu
