// theme.h — display theme for the LCD: element colours, night mode, brightness,
// title, text size, wind-rose geometry and hull stroke.
//
// It mirrors the web Theme Studio (web/Marine_Display_Theme_Studio.html): the same
// element names, the same presets and the same night-mode rules, and it reads the
// same JSON file the studio exports ("Export theme (.json)"), so a theme designed in
// the browser can be loaded on the board over USB (`theme {...}` on the serial port).
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define FW_VERSION "1.0.0"

namespace theme {

enum Tok : uint8_t {
  MANUFACTURER, CHASSIS, BEZEL, DISPLAY_BG, PRIMARY_TEXT, SECONDARY_TEXT, GRID, SEPARATOR,
  ARROWS, ARROW_SECONDARY, PORT, STARBOARD, WARNING, WIND_PORT, WIND_STARBOARD, WIND_NEUTRAL,
  WIND_FACE, WIND_TICKS, HULL, MENU, MENU_HEADER, MENU_TEXT, MENU_ACCENT,
  AIS_RINGS, AIS_TARGET, AIS_DANGER, AIS_OWN, TOK_COUNT
};

// JSON names, exactly as in the web studio's "colors" object.
static const char *const TOK_NAME[TOK_COUNT] = {
  "manufacturer", "chassis", "bezel", "displayBackground", "primaryText", "secondaryText", "grid", "separator",
  "arrows", "arrowSecondary", "port", "starboard", "warning", "windPort", "windStarboard", "windNeutral",
  "windFace", "windTicks", "hull", "menu", "menuHeader", "menuText", "menuAccent",
  "aisRings", "aisTarget", "aisDanger", "aisOwn"
};

static const uint32_t TOK_DEFAULT[TOK_COUNT] = {
  0xa8a9b6, 0x292a2c, 0x1b1c1f, 0xffffff, 0x111111, 0x555555, 0x8a9198, 0xc1c1c1,
  0x29469c, 0xe66f2e, 0xe23843, 0x36ad61, 0xed1b2f, 0xe02424, 0x008e3b, 0x343f46,
  0xffffff, 0xffffff, 0x858585, 0x080a09, 0x333638, 0xf5f6f2, 0xa7c939,
  0xfefefe, 0x8fb0d4, 0xd8352f, 0xe16639
};

// Wind rose geometry in the 600 x 600 SVG space of the web studio (centre 300,300).
struct Rose {
  int16_t rBezelOuter = 282, rRingOuter = 266, rRingInner = 221, shadowRadius = 184;
  int16_t gradStartDeg = 42, gradEndDeg = 74;
  float shadowOpacity = 0.55f;
  uint32_t bottomGrey = 0x242c30, bezel = 0x0b0e11, lip = 0x3e474c, innerStroke = 0x485258;
};

struct Theme {
  uint32_t color[TOK_COUNT];
  char title[17] = "P&G";
  bool night = false;
  uint32_t nightColor = 0xff3b30;
  uint8_t brightness = 100;          // 10..100 %
  bool invert = false;
  float fontScale = 1.0f;            // 0.7..1.5
  int16_t hullWidth = 29;            // 10..50, in the 420 x 950 hull space
  Rose rose;
};

static Theme T;
static uint32_t revision = 1;        // bumped on every change; the UI redraws when it moves

static void resetColors() { for (int i = 0; i < TOK_COUNT; i++) T.color[i] = TOK_DEFAULT[i]; }
static void resetAll() { T = Theme(); resetColors(); revision++; }

// Colour of an element after night mode and inversion — the same rules as the web studio.
static uint32_t finish(Tok t, uint32_t c) {
  if (T.night) {
    if (t == DISPLAY_BG || t == WIND_FACE) c = 0x07090b;
    if (t == PRIMARY_TEXT || t == SECONDARY_TEXT || t == MANUFACTURER || t == MENU_TEXT || t == AIS_RINGS) c = T.nightColor;
    if (t == GRID || t == SEPARATOR) c = 0x3a2020;
  }
  if (T.invert) c ^= 0xffffff;
  return c;
}
static uint32_t rgb(Tok t) { return finish(t, T.color[t]); }
static lv_color_t col(Tok t) { return lv_color_hex(rgb(t)); }
// A page's own shade of an element (e.g. the Laylines degree ring is a lighter wind-rose
// neutral): drawn as is until that element is customised, as in the web studio.
static lv_color_t lit(Tok t, uint32_t literal) {
  return lv_color_hex(finish(t, T.color[t] != TOK_DEFAULT[t] ? T.color[t] : literal));
}
// Fixed colours (black chart backgrounds, rose bezel…) only follow inversion.
static lv_color_t fixed(uint32_t c) { return lv_color_hex(T.invert ? c ^ 0xffffff : c); }

// ── lettering: every display font size goes through the text-size setting ──
static const lv_font_t *const FONTS[] = {
  &lv_font_montserrat_8, &lv_font_montserrat_10, &lv_font_montserrat_12, &lv_font_montserrat_14,
  &lv_font_montserrat_16, &lv_font_montserrat_18, &lv_font_montserrat_20, &lv_font_montserrat_22,
  &lv_font_montserrat_24, &lv_font_montserrat_26, &lv_font_montserrat_28, &lv_font_montserrat_30,
  &lv_font_montserrat_32, &lv_font_montserrat_34, &lv_font_montserrat_36, &lv_font_montserrat_38,
  &lv_font_montserrat_40, &lv_font_montserrat_42, &lv_font_montserrat_44, &lv_font_montserrat_46,
  &lv_font_montserrat_48
};
static const lv_font_t *font(float px, bool scaled = true) {
  float p = scaled ? px * T.fontScale : px;
  int i = (int)lroundf((p - 8.0f) / 2.0f);
  return FONTS[constrain(i, 0, 20)];
}

// ── presets (web studio: Day, Night, Amber night, Monochrome) ──
static void applyPreset(const char *name) {
  if (!strcmp(name, "day"))   { T.night = false; T.brightness = 100; T.invert = false; }
  else if (!strcmp(name, "night")) { T.night = true; T.brightness = 45; T.invert = false; T.nightColor = 0xff3b30; }
  else if (!strcmp(name, "amber")) { T.night = true; T.brightness = 35; T.invert = false; T.nightColor = 0xffb000; }
  else if (!strcmp(name, "mono")) {
    T.night = false; T.brightness = 90; T.invert = false;
    for (int i = 0; i < TOK_COUNT; i++) {
      Tok t = (Tok)i;
      T.color[i] = (t == DISPLAY_BG || t == WIND_FACE || t == WIND_TICKS || t == MENU_TEXT || t == AIS_RINGS) ? 0xeeeeee
                 : (t == PRIMARY_TEXT || t == BEZEL || t == MENU) ? 0x111111 : 0x666666;
    }
  } else return;
  revision++;
}

// ── JSON (the web studio's export format) ──
static bool parseHex(JsonVariantConst v, uint32_t &out) {
  const char *s = v.as<const char *>();
  if (!s || s[0] != '#' || strlen(s) != 7) return false;
  for (int i = 1; i < 7; i++) if (!isxdigit((unsigned char)s[i])) return false;
  out = strtoul(s + 1, nullptr, 16);
  return true;
}
static void hexStr(uint32_t c, char *buf) { snprintf(buf, 8, "#%06lx", (unsigned long)(c & 0xffffff)); }
template <typename V> static V clampNum(JsonVariantConst v, V lo, V hi, V fallback) {
  if (!v.is<float>() && !v.is<int>()) return fallback;
  float f = v.as<float>();
  return (V)constrain(f, (float)lo, (float)hi);
}

// Accepts {"rose":{…},"hull":{…},"display":{…}} (full studio theme) or a display-only
// theme with "colors" at the top level. Unknown keys are ignored; missing keys keep
// their current value. Returns false if the document has none of these sections.
static bool fromJson(JsonVariantConst doc) {
  JsonVariantConst disp = doc["display"].is<JsonObjectConst>() ? doc["display"] : (doc["colors"].is<JsonObjectConst>() ? doc : JsonVariantConst());
  JsonVariantConst rose = doc["rose"], hull = doc["hull"];
  if (disp.isNull() && rose.isNull() && hull.isNull()) return false;
  if (!disp.isNull()) {
    if (const char *m = disp["manufacturer"]) { strlcpy(T.title, m, sizeof(T.title)); }
    if (const char *mode = disp["mode"]) T.night = !strcmp(mode, "night");
    parseHex(disp["nightColor"], T.nightColor);
    T.brightness = clampNum<int>(disp["brightness"], 10, 100, T.brightness);
    if (disp["invert"].is<bool>()) T.invert = disp["invert"].as<bool>();
    T.fontScale = clampNum<float>(disp["fontScale"], 0.7f, 1.5f, T.fontScale);
    JsonVariantConst c = disp["colors"];
    for (int i = 0; i < TOK_COUNT; i++) parseHex(c[TOK_NAME[i]], T.color[i]);
  }
  if (!rose.isNull()) {
    Rose &r = T.rose;
    r.rBezelOuter  = clampNum<int>(rose["rBezelOuter"], 270, 295, r.rBezelOuter);
    r.rRingInner   = clampNum<int>(rose["rRingInner"], 195, 240, r.rRingInner);
    r.shadowRadius = clampNum<int>(rose["shadowRadius"], 150, 218, r.shadowRadius);
    r.gradStartDeg = clampNum<int>(rose["gradStartDeg"], 20, 60, r.gradStartDeg);
    r.gradEndDeg   = clampNum<int>(rose["gradEndDeg"], 50, 105, r.gradEndDeg);
    r.shadowOpacity = clampNum<float>(rose["shadowOpacity"], 0.0f, 0.85f, r.shadowOpacity);
    if (r.gradEndDeg < r.gradStartDeg + 4) r.gradEndDeg = r.gradStartDeg + 4;
    if (r.shadowRadius > r.rRingInner - 2) r.shadowRadius = r.rRingInner - 2;
    parseHex(rose["colBottomGrey"], r.bottomGrey);
    parseHex(rose["colBezel"], r.bezel);
    parseHex(rose["colBezelLip"], r.lip);
    parseHex(rose["colInnerStroke"], r.innerStroke);
    // The studio keeps these three in step with the element colours.
    parseHex(rose["colPort"], T.color[WIND_PORT]);
    parseHex(rose["colStarboard"], T.color[WIND_STARBOARD]);
    parseHex(rose["colSideGrey"], T.color[WIND_NEUTRAL]);
  }
  if (!hull.isNull()) {
    parseHex(hull["stroke"], T.color[HULL]);
    T.hullWidth = clampNum<int>(hull["strokeWidth"], 10, 50, T.hullWidth);
  }
  revision++;
  return true;
}

static void toJson(JsonDocument &doc) {
  char h[8];
  doc["format"] = "mfd-theme";
  doc["version"] = 3;
  JsonObject r = doc["rose"].to<JsonObject>();
  r["rBezelOuter"] = T.rose.rBezelOuter; r["rRingOuter"] = T.rose.rRingOuter; r["rRingInner"] = T.rose.rRingInner;
  r["shadowRadius"] = T.rose.shadowRadius; r["gradStartDeg"] = T.rose.gradStartDeg; r["gradEndDeg"] = T.rose.gradEndDeg;
  r["shadowOpacity"] = T.rose.shadowOpacity;
  hexStr(T.color[WIND_PORT], h); r["colPort"] = h;
  hexStr(T.color[WIND_STARBOARD], h); r["colStarboard"] = h;
  hexStr(T.color[WIND_NEUTRAL], h); r["colSideGrey"] = h;
  hexStr(T.rose.bottomGrey, h); r["colBottomGrey"] = h;
  hexStr(T.rose.bezel, h); r["colBezel"] = h;
  hexStr(T.rose.lip, h); r["colBezelLip"] = h;
  hexStr(T.rose.innerStroke, h); r["colInnerStroke"] = h;
  JsonObject hl = doc["hull"].to<JsonObject>();
  hexStr(T.color[HULL], h); hl["stroke"] = h; hl["strokeWidth"] = T.hullWidth;
  JsonObject d = doc["display"].to<JsonObject>();
  d["manufacturer"] = T.title; d["mode"] = T.night ? "night" : "day";
  hexStr(T.nightColor, h); d["nightColor"] = h;
  d["brightness"] = T.brightness; d["invert"] = T.invert; d["fontScale"] = T.fontScale;
  JsonObject c = d["colors"].to<JsonObject>();
  for (int i = 0; i < TOK_COUNT; i++) { hexStr(T.color[i], h); c[TOK_NAME[i]] = h; }
}

// ── persistence (NVS namespace "mfd") ──
static void save() {
  JsonDocument doc; toJson(doc);
  String s; serializeJson(doc, s);
  Preferences p; p.begin("mfd", false); p.putString("theme", s); p.end();
}
static void load() {
  resetAll();
  Preferences p; p.begin("mfd", true); String s = p.getString("theme", ""); p.end();
  if (!s.length()) return;
  JsonDocument doc;
  if (!deserializeJson(doc, s)) fromJson(doc.as<JsonVariantConst>());
}

}  // namespace theme
