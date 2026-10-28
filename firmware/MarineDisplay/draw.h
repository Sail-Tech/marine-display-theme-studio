// draw.h — a small painter over LVGL 8's draw primitives, used inside the display
// object's LV_EVENT_DRAW_MAIN. Pages are written in the web studio's page coordinates
// (500 x 370 for most pages) and mapped to the LCD with one uniform scale, so circles
// stay circles. LVGL clips every primitive to the band being rendered.
#pragma once
#include <lvgl.h>
#include "theme.h"
#include "sim.h"

struct Painter {
  lv_draw_ctx_t *dc;
  float ox, oy, k;                                     // screen = o + page * k

  lv_coord_t X(float x) const { return (lv_coord_t)lroundf(ox + x * k); }
  lv_coord_t Y(float y) const { return (lv_coord_t)lroundf(oy + y * k); }
  lv_coord_t L(float v) const { return (lv_coord_t)max(1L, lroundf(v * k)); }

  void rect(float x, float y, float w, float h, lv_color_t c, float radius = 0, lv_opa_t opa = LV_OPA_COVER) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = c; d.bg_opa = opa; d.radius = (lv_coord_t)lroundf(radius * k);
    lv_area_t a = {X(x), Y(y), (lv_coord_t)(X(x + w) - 1), (lv_coord_t)(Y(y + h) - 1)};
    lv_draw_rect(dc, &d, &a);
  }
  void frame(float x, float y, float w, float h, lv_color_t c, float width, float radius = 0) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP; d.border_color = c; d.border_width = L(width); d.border_opa = LV_OPA_COVER;
    d.radius = (lv_coord_t)lroundf(radius * k);
    lv_area_t a = {X(x), Y(y), (lv_coord_t)(X(x + w) - 1), (lv_coord_t)(Y(y + h) - 1)};
    lv_draw_rect(dc, &d, &a);
  }
  void disc(float cx, float cy, float r, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = c; d.bg_opa = opa; d.radius = LV_RADIUS_CIRCLE;
    lv_coord_t R = (lv_coord_t)lroundf(r * k);
    lv_area_t a = {(lv_coord_t)(X(cx) - R), (lv_coord_t)(Y(cy) - R), (lv_coord_t)(X(cx) + R), (lv_coord_t)(Y(cy) + R)};
    lv_draw_rect(dc, &d, &a);
  }
  // Ring between rIn and rOut from nautical angle a0 to a1 (0 = up, clockwise).
  void band(float cx, float cy, float rIn, float rOut, float a0, float a1, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
    lv_draw_arc_dsc_t d; lv_draw_arc_dsc_init(&d);
    d.color = c; d.opa = opa; d.width = L(rOut - rIn); d.rounded = 0;
    lv_point_t ctr = {X(cx), Y(cy)};
    int s = (int)lroundf(a0 - 90), e = (int)lroundf(a1 - 90);
    s = ((s % 360) + 360) % 360; e = ((e % 360) + 360) % 360;
    if (fabsf(a1 - a0) >= 359.5f) { s = 0; e = 360; }
    lv_draw_arc(dc, &d, &ctr, (uint16_t)lroundf(rOut * k), (uint16_t)s, (uint16_t)e);
  }
  void ring(float cx, float cy, float r, lv_color_t c, float width, lv_opa_t opa = LV_OPA_COVER) {
    band(cx, cy, r - width / 2, r + width / 2, 0, 360, c, opa);
  }
  void line(float x1, float y1, float x2, float y2, lv_color_t c, float width, bool round = false, lv_opa_t opa = LV_OPA_COVER) {
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.color = c; d.width = L(width); d.opa = opa; d.round_start = round; d.round_end = round;
    lv_point_t p1 = {X(x1), Y(y1)}, p2 = {X(x2), Y(y2)};
    lv_draw_line(dc, &d, &p1, &p2);
  }
  // Convex/concave polygon, at most 12 points (LVGL keeps one mask per edge).
  void poly(const float *xy, int n, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
    lv_point_t p[12]; n = min(n, 12);
    for (int i = 0; i < n; i++) p[i] = {X(xy[2 * i]), Y(xy[2 * i + 1])};
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = c; d.bg_opa = opa;
    lv_draw_polygon(dc, &d, p, (uint16_t)n);
  }
  // Polygon given in a local frame, rotated by deg (clockwise) and moved to (x, y).
  void shape(const float *xy, int n, float x, float y, float deg, lv_color_t c, float scale = 1) {
    float out[24]; n = min(n, 12);
    float s = sinf(deg * sim::DEG), co = cosf(deg * sim::DEG);
    for (int i = 0; i < n; i++) {
      float px = xy[2 * i] * scale, py = xy[2 * i + 1] * scale;
      out[2 * i] = x + px * co - py * s; out[2 * i + 1] = y + px * s + py * co;
    }
    poly(out, n, c);
  }
  // Text with its baseline at y (as canvas fillText with textBaseline 'alphabetic').
  // align: 'l', 'c' or 'r'. px is the page-space size; the theme's text size applies.
  void text(const char *s, float x, float y, float px, lv_color_t c, char align = 'l', bool scaled = true) {
    const lv_font_t *f = theme::font(px * k, scaled);
    lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d);
    d.color = c; d.font = f;
    d.align = align == 'c' ? LV_TEXT_ALIGN_CENTER : align == 'r' ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT;
    lv_coord_t top = Y(y) - (f->line_height - f->base_line), sx = X(x);
    lv_area_t a;
    if (align == 'c') a = {(lv_coord_t)(sx - 400), top, (lv_coord_t)(sx + 400), (lv_coord_t)(top + f->line_height)};
    else if (align == 'r') a = {(lv_coord_t)(sx - 800), top, sx, (lv_coord_t)(top + f->line_height)};
    else a = {sx, top, (lv_coord_t)(sx + 800), (lv_coord_t)(top + f->line_height)};
    lv_draw_label(dc, &d, &a, s, nullptr);
  }
};

static lv_color_t mixColor(uint32_t a, uint32_t b, float t) {
  t = constrain(t, 0.0f, 1.0f);
  auto ch = [&](int sh) { return (uint32_t)lroundf(((a >> sh) & 255) + ((((b >> sh) & 255) - (float)((a >> sh) & 255)) * t)); };
  uint32_t c = (ch(16) << 16) | (ch(8) << 8) | ch(0);
  return theme::fixed(c);
}
static float smoothstep(float t) { t = constrain(t, 0.0f, 1.0f); return t * t * (3 - 2 * t); }
