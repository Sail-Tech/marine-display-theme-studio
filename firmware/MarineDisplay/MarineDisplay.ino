// Marine Display — native LVGL port of the Marine Display Theme Studio's instrument
// for the Waveshare ESP32-S3-Touch-LCD-4 (480 x 480, GT911 touch).
//
// Simulation only: all data is generated on the board. There is no NMEA 2000 / CAN
// connection and nothing is ever transmitted; do not use it for navigation.
//
// Pages: Wind Quad, Sail Steer, Laylines, AIS, GPS. Keys: PAGES (hold = previous page),
// ▲ / ▼ (AIS range), ENTER (next AIS target), MENU. Swipe to change page; tap an AIS target.
// Themes designed in web/Marine_Display_Theme_Studio.html load over USB: see README.
//
// SPDX-License-Identifier: MIT
#include "ui.h"

static uint32_t lastTick = 0;

// ── serial command line (115200 baud, newline-terminated) ──
static String line, themeBuf;
static int themeDepth = -1;                         // >= 0 while a multi-line `theme {…}` is arriving

static void printHelp() {
  Serial.println(F(
    "Commands:\n"
    "  help                     this list\n"
    "  status                   page, theme, AIS and memory\n"
    "  page <0-4|name> | next   show a page (0 Wind Quad, 1 Sail Steer, 2 Laylines, 3 AIS, 4 GPS)\n"
    "  scheme day|night|amber|mono\n"
    "  bright <10-100>          backlight in %\n"
    "  title <text>             manufacturer title (max 16 chars)\n"
    "  textsize <70-150>        display text size in %\n"
    "  invert on|off\n"
    "  range auto|2|3|6|9|12|15 AIS range in NM;  range + / range -\n"
    "  cpa <nm> | tcpa <min>    AIS dangerous-target limits\n"
    "  target next|none         AIS selection\n"
    "  scenario normal|noheading|noposition|noais\n"
    "  theme                    print the current theme as JSON\n"
    "  theme {...}              load a theme exported by the web studio (paste the whole file)\n"
    "  theme reset              back to the default theme"));
}

static void printStatus() {
  Serial.printf("Marine Display %s  page %s  scheme %s  brightness %u%%  text %d%%  title \"%s\"\n",
                FW_VERSION, ui::PAGE_NAME[ui::page], theme::T.night ? "night" : "day", theme::T.brightness,
                (int)lroundf(theme::T.fontScale * 100), theme::T.title);
  sim::ais::View v[sim::ais::N]; int n = sim::ais::targets(v), dg = 0; for (int i = 0; i < n; i++) dg += v[i].dangerous;
  Serial.printf("AIS: %s %g NM  %d targets (%d dangerous)  CPA %g NM / TCPA %g min  rx 129038:%lu 129039:%lu 129794:%lu 129809:%lu 129810:%lu\n",
                sim::ais::autoRange ? "auto" : "manual", sim::ais::scale(), n, dg, sim::ais::cpaLimit, sim::ais::tcpaLimit,
                (unsigned long)sim::ais::rx129038, (unsigned long)sim::ais::rx129039, (unsigned long)sim::ais::rx129794,
                (unsigned long)sim::ais::rx129809, (unsigned long)sim::ais::rx129810);
  Serial.printf("Heap %u B, PSRAM %u B free, uptime %lu s\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(), (unsigned long)(millis() / 1000));
}

static void loadTheme(const String &json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) { Serial.printf("theme: invalid JSON (%s)\n", err.c_str()); return; }
  if (!theme::fromJson(doc.as<JsonVariantConst>())) { Serial.println(F("theme: no \"rose\", \"hull\", \"display\" or \"colors\" section")); return; }
  ui::themeChanged();
  Serial.println(F("theme: loaded and saved"));
}

static void command(String cmd) {
  cmd.trim();
  if (!cmd.length()) return;
  int sp = cmd.indexOf(' ');
  String verb = sp < 0 ? cmd : cmd.substring(0, sp), arg = sp < 0 ? "" : cmd.substring(sp + 1);
  arg.trim(); verb.toLowerCase();
  if (verb == "help" || verb == "?") printHelp();
  else if (verb == "status") printStatus();
  else if (verb == "next") ui::setPage(ui::page + 1);
  else if (verb == "page") {
    int p = -1;
    for (int i = 0; i < ui::PG_COUNT; i++) if (arg.equalsIgnoreCase(ui::PAGE_NAME[i])) p = i;
    if (p < 0 && arg.length() && isdigit(arg[0])) p = arg.toInt();
    if (p >= 0 && p < ui::PG_COUNT) ui::setPage(p); else Serial.println(F("page: 0-4 or a page name"));
  }
  else if (verb == "scheme") {
    if (arg == "day" || arg == "night" || arg == "amber" || arg == "mono") { theme::applyPreset(arg.c_str()); ui::themeChanged(); }
    else Serial.println(F("scheme: day, night, amber or mono"));
  }
  else if (verb == "bright") { theme::T.brightness = constrain(arg.toInt(), 10, 100); ui::themeChanged(); }
  else if (verb == "title") { strlcpy(theme::T.title, arg.c_str(), sizeof(theme::T.title)); ui::themeChanged(); }
  else if (verb == "textsize") { theme::T.fontScale = constrain(arg.toInt(), 70, 150) / 100.0f; ui::themeChanged(); }
  else if (verb == "invert") { theme::T.invert = arg == "on"; ui::themeChanged(); }
  else if (verb == "range") {
    namespace A = sim::ais;
    if (arg == "auto") A::autoRange = true;
    else if (arg == "+") A::stepRange(+1);
    else if (arg == "-") A::stepRange(-1);
    else {
      int i = -1; for (int k = 0; k < A::NSTEPS; k++) if (A::STEPS[k] == arg.toFloat()) i = k;
      if (i < 0) { Serial.println(F("range: auto, +, - or 2, 3, 6, 9, 12, 15")); return; }
      A::autoRange = false; A::rangeIdx = i;
    }
    A::autoSince = 0; ui::saveSettings(); ui::needRedraw = true;
  }
  else if (verb == "cpa") { sim::ais::cpaLimit = constrain(arg.toFloat(), 0.1f, 5.0f); ui::saveSettings(); ui::needRedraw = true; }
  else if (verb == "tcpa") { sim::ais::tcpaLimit = constrain(arg.toFloat(), 1.0f, 60.0f); ui::saveSettings(); ui::needRedraw = true; }
  else if (verb == "target") { if (arg == "none") sim::ais::selected = 0; else sim::ais::stepTarget(); ui::needRedraw = true; }
  else if (verb == "scenario") {
    sim::valid = sim::Valid();
    if (arg == "noheading") sim::valid.heading = false;
    else if (arg == "noposition") sim::valid.position = false;
    else if (arg == "noais") sim::valid.ais = false;
    else if (arg != "normal") Serial.println(F("scenario: normal, noheading, noposition or noais"));
    sim::ais::advance(0); ui::needRedraw = true;
  }
  else if (verb == "theme") {
    if (!arg.length()) { JsonDocument doc; theme::toJson(doc); serializeJsonPretty(doc, Serial); Serial.println(); }
    else if (arg == "reset") { theme::resetAll(); ui::themeChanged(); Serial.println(F("theme: defaults restored")); }
    else Serial.println(F("theme: expected JSON starting with {"));
  }
  else Serial.printf("unknown command '%s' (try help)\n", verb.c_str());
}

// A pasted theme file spans many lines: collect from `theme {` until the braces balance.
static void serialInput() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (themeDepth >= 0) {
      themeBuf += c;
      if (c == '{') themeDepth++;
      else if (c == '}' && --themeDepth == 0) { loadTheme(themeBuf); themeBuf = ""; themeDepth = -1; }
      if (themeBuf.length() > 16384) { Serial.println(F("theme: too long")); themeBuf = ""; themeDepth = -1; }
      continue;
    }
    if (c == '\r') continue;
    if (c == '{' && line.startsWith("theme")) { themeBuf = "{"; themeDepth = 1; line = ""; continue; }
    if (c == '\n') { command(line); line = ""; }
    else if (line.length() < 200) line += c;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);                          // never block when no host is reading USB
  delay(200);
  Serial.printf("\nMarine Display %s — simulation only, nothing is transmitted\n", FW_VERSION);
  theme::load();
  ui::loadSettings();
  sim::begin();
  if (!ui::begin(Serial)) Serial.println(F("UI failed to start"));
  printHelp();
}

void loop() {
  serialInput();
  uint32_t now = millis();
  if (now - lastTick >= 1000) { lastTick += 1000; if (now - lastTick > 5000) lastTick = now; sim::tick(); ui::needRedraw = true; }
  ui::service();
  delay(5);
}
