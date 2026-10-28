// screen.h — layout, key handling and the three drawn areas of the LCD screen.
// Board-independent: ui.h wires it to the panel and touch (and a host harness can
// render it off-target).
//   y   0- 36  title strip (manufacturer title on the chassis colour)
//   y  36-412  display: the active page, drawn natively (custom draw object)
//   y 412-480  five touch keys, as on the simulated instrument: PAGES ▲ ENTER ▼ MENU
#pragma once
#include "pages.h"

namespace ui {

static constexpr int TITLE_H = 36, DISP_Y = 36, DISP_H = 376, KEYS_Y = 412, KEYS_H = 68;
enum Key : int8_t { K_NONE = -1, K_PAGES, K_UP, K_ENTER, K_DOWN, K_MENU };

static int8_t keyDown = K_NONE;

// ── page navigation ──
static void setPage(int p) { page = (uint8_t)((p % PG_COUNT + PG_COUNT) % PG_COUNT); saveSettings(); needRedraw = true; }

static void handleKey(int k, bool held) {
  if (menu::open) {
    if (k == K_PAGES) menu::back();
    else if (k == K_UP) menu::move(-1);
    else if (k == K_DOWN) menu::move(1);
    else if (k == K_ENTER) menu::select();
    else if (k == K_MENU) menu::close();
    return;
  }
  switch (k) {
    case K_PAGES: setPage(page + (held ? -1 : 1)); break;          // hold = previous page
    case K_MENU: menu::show(); break;
    case K_UP: if (page == PG_AIS) { sim::ais::stepRange(+1); saveSettings(); needRedraw = true; } break;
    case K_DOWN: if (page == PG_AIS) { sim::ais::stepRange(-1); saveSettings(); needRedraw = true; } break;
    case K_ENTER: if (page == PG_AIS) { sim::ais::stepTarget(); needRedraw = true; } break;
  }
}

// ── drawing ──
static lv_area_t coordsOf(lv_event_t *e) { lv_area_t a; lv_obj_get_coords(lv_event_get_target(e), &a); return a; }

static void drawTitle(lv_event_t *e) {
  lv_area_t a = coordsOf(e);
  Painter p{lv_event_get_draw_ctx(e), (float)a.x1, (float)a.y1, 1};
  p.rect(0, 0, 480, TITLE_H, theme::col(theme::CHASSIS));
  p.line(0, TITLE_H - 1, 480, TITLE_H - 1, theme::col(theme::BEZEL), 2);
  p.text(theme::T.title, 240, 28, 26, theme::col(theme::MANUFACTURER), 'c', false);
}

static void drawKeys(lv_event_t *e) {
  lv_area_t a = coordsOf(e);
  Painter p{lv_event_get_draw_ctx(e), (float)a.x1, (float)a.y1, 1};
  using namespace theme;
  lv_color_t body = col(CHASSIS), edge = col(BEZEL), glyph = fixed(0xf4f5f7);   // key glyphs stay white, as on the bezel
  p.rect(0, 0, 480, KEYS_H, body);
  for (int i = 1; i < 5; i++) p.rect(i * 96 - 3, 10, 6, KEYS_H - 20, edge, 3);
  for (int i = 0; i < 5; i++) {
    float cx = 48 + i * 96, cy = KEYS_H / 2.0f;
    p.disc(cx, cy, 28, keyDown == i ? lit(CHASSIS, 0x55575c) : lit(CHASSIS, 0x35363a));
    p.ring(cx, cy, 28, edge, 2);
  }
  // PAGES: 3 x 3 grid
  for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) p.rect(48 - 11 + c * 8, KEYS_H / 2.0f - 11 + r * 8, 6, 6, glyph);
  const float up[] = {144 - 9, 40, 144 + 9, 40, 144, 28}, dn[] = {336 - 9, 28, 336 + 9, 28, 336, 40};
  p.poly(up, 3, glyph); p.poly(dn, 3, glyph);
  p.text(LV_SYMBOL_NEW_LINE, 240, 42, 20, glyph, 'c', false);
  p.text("MENU", 432, 40, 14, glyph, 'c', false);
}

static void drawDisplay(lv_event_t *e) {
  lv_area_t a = coordsOf(e);
  lv_draw_ctx_t *dc = lv_event_get_draw_ctx(e);
  using namespace theme;
  // web page space 500 x 370 → 480 x 355, letterboxed vertically
  const float k = 480.0f / 500;
  Painter full{dc, (float)a.x1, (float)a.y1, 1};
  Painter p{dc, (float)a.x1, a.y1 + (DISP_H - 370 * k) / 2, k};
  bool dark = page == PG_WIND_QUAD || page == PG_AIS;
  full.rect(0, 0, 480, DISP_H, dark ? fixed(0x050607) : (page == PG_LAYLINES ? lit(DISPLAY_BG, 0xf9fafb) : col(DISPLAY_BG)));
  bool needsHeading = page != PG_GPS;
  if (needsHeading && !sim::valid.heading) pages::unavailable(p, "HEADING DATA UNAVAILABLE");
  else switch (page) {
    case PG_WIND_QUAD: pages::windQuad(p); break;
    case PG_SAIL_STEER: pages::sailingSteer(p); break;
    case PG_AIS: pages::ais(p); break;
    case PG_GPS: pages::gps(p); break;
    case PG_LAYLINES: {
      // instrument space: display aperture 674 x 522 at (108, 134); the rose part is 1.17x about (444, 398)
      const float k2 = 480.0f / 674, k3 = k2 * 1.17f;
      float ox = a.x1 - 108 * k2, oy = a.y1 + (DISP_H - 522 * k2) / 2 - 134 * k2;
      Painter ip{dc, ox, oy, k2}, rp{dc, ox + 444 * (k2 - k3), oy + 398 * k2 - 412 * k3, k3};
      pages::laylines(ip, rp);
      break;
    }
  }
  if (menu::open) pages::menuOverlay(full);
}

}  // namespace ui
