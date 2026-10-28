// pages.h — the display pages, ported from the web simulator's canvas drawing.
// Most pages use the web page space (500 x 370); Laylines uses the instrument space
// (display aperture 674 x 522 at 108,134), as in the original.
#pragma once
#include "draw.h"
#include "menu.h"

namespace pages {
using namespace theme;
using sim::d;
using sim::DEG;

static float px(float cx, float a, float r) { return cx + sinf(a * DEG) * r; }
static float py(float cy, float a, float r) { return cy - cosf(a * DEG) * r; }
static void fmt3(char *b, float v) { snprintf(b, 8, "%03d", ((int)lroundf(v) % 360 + 360) % 360); }

// ── Marine Wind Rose (shared by Wind Quad, Sail Steer and Laylines) ──
static void rose(Painter &p, float cx, float cy, float R) {
  const Rose &r = T.rose;
  float rb = R * r.rBezelOuter / 300, ro = R * r.rRingOuter / 300, ri = R * r.rRingInner / 300, rs = R * r.shadowRadius / 300;
  uint32_t port = T.color[WIND_PORT], star = T.color[WIND_STARBOARD], side = T.color[WIND_NEUTRAL];
  p.disc(cx, cy, rb, fixed(r.bezel));
  p.ring(cx, cy, ro + R * .005f, fixed(r.lip), max(1.0f, R * .005f), 166);
  // aft arc: side grey darkening towards the stern
  for (float a = r.gradEndDeg; a < 360 - r.gradEndDeg; a += 4) {
    float mid = a + 2;
    p.band(cx, cy, ri, ro, a, min(a + 4.6f, 360.0f - r.gradEndDeg + 0.6f), mixColor(side, r.bottomGrey, (1 - cosf(mid * DEG)) / 2));
  }
  p.band(cx, cy, ri, ro, 0, r.gradStartDeg + 0.5f, fixed(star));
  p.band(cx, cy, ri, ro, 360 - r.gradStartDeg - 0.5f, 360, fixed(port));
  int span = r.gradEndDeg - r.gradStartDeg, steps = max(4, span);
  for (int i = 0; i < steps; i++) {
    float t = smoothstep((i + 0.5f) / steps), a0 = r.gradStartDeg + (float)i * span / steps, a1 = a0 + (float)span / steps + 0.6f;
    p.band(cx, cy, ri, ro, a0, a1, mixColor(star, side, t));
    p.band(cx, cy, ri, ro, 360 - a1, 360 - a0, mixColor(port, side, t));
  }
  // dial: face colour with a soft grey shadow towards the band
  p.disc(cx, cy, ri, col(WIND_FACE));
  if (!T.night) {
    uint32_t face = finish(WIND_FACE, T.color[WIND_FACE]), mid = 0xcfd6dc;
    float o = r.shadowOpacity;
    uint32_t edge = ((uint32_t)lroundf(255 - 133 * o) << 16) | ((uint32_t)lroundf(255 - 120 * o) << 8) | (uint32_t)lroundf(255 - 109 * o);
    const int n = 8;
    for (int j = 0; j < n; j++) {
      float f0 = 0.68f + 0.32f * j / n, f1 = 0.68f + 0.32f * (j + 1) / n, f = (f0 + f1) / 2;
      lv_color_t c = f < 0.88f ? mixColor(face, mid, (f - 0.68f) / 0.2f) : mixColor(mid, edge, (f - 0.88f) / 0.12f);
      p.band(cx, cy, rs + (ri - rs) * f0, rs + (ri - rs) * f1 + 0.5f, 0, 360, c);
    }
  }
  p.ring(cx, cy, ri, fixed(r.innerStroke), max(1.0f, R * .0073f));
  p.ring(cx, cy, ro, fixed(0x14191d), max(1.0f, R * .0053f));
  lv_color_t tick = col(WIND_TICKS);
  for (int deg = 0; deg < 360; deg += 15) {
    bool cardinal = deg % 90 == 0, medium = !cardinal && deg % 30 == 0;
    float r0 = ri + R * (cardinal ? .015f : .018f), r1 = cardinal ? ro - R * .015f : medium ? ri + (ro - ri) * .56f : ri + (ro - ri) * .30f;
    p.line(px(cx, deg, r0), py(cy, deg, r0), px(cx, deg, r1), py(cy, deg, r1), tick, max(1.0f, R * (cardinal ? .024f : medium ? .018f : .0153f)));
  }
}

// ── vector hull: symmetric cubic Bézier in a 420 x 950 box, RMSE 0.201 px ──
static void bez(float t, const float *P, float &x, float &y) {
  float u = 1 - t;
  x = u * u * u * P[0] + 3 * u * u * t * P[2] + 3 * u * t * t * P[4] + t * t * t * P[6];
  y = u * u * u * P[1] + 3 * u * u * t * P[3] + 3 * u * t * t * P[5] + t * t * t * P[7];
}
// lineWidth < 0: the theme's stroke (29 in hull space) scaled to the box, times strokeScale.
static void hull(Painter &p, float x, float y, float w, float h, lv_color_t c, float lineWidth = -1, float strokeScale = 1, lv_opa_t opa = LV_OPA_COVER) {
  static const float STAR[8] = {200, 29.32f, 264.45f, 137.90f, 413.00f, 388.89f, 284.69f, 891.04f};
  static const float PORT[8] = {115.31f, 891.04f, -13.00f, 388.89f, 135.55f, 137.90f, 200, 29.32f};
  float sx = w / 420, sy = h / 950;
  float lw = lineWidth < 0 ? T.hullWidth * min(sx, sy) * strokeScale : lineWidth * T.hullWidth / 29.0f;
  auto seg = [&](const float *P) {
    float lx, ly; bez(0, P, lx, ly);
    for (int i = 1; i <= 16; i++) {
      float bx, by; bez(i / 16.0f, P, bx, by);
      p.line(x + lx * sx, y + ly * sy, x + bx * sx, y + by * sy, c, lw, true, opa);
      lx = bx; ly = by;
    }
  };
  seg(STAR);
  p.line(x + 284.69f * sx, y + 891.04f * sy, x + 115.31f * sx, y + 891.04f * sy, c, lw, true, opa);
  seg(PORT);
}

// ── Wind Quad: apparent and true wind ──
static void windWedge(Painter &p, float cx, float cy, float angle, float len, float half, lv_color_t c, const char *label, float lr) {
  float q[12], L = len;
  auto qb = [&](float t, float &x, float &y) {          // quadratic edge (-half,-L+25) → (0,-L-3) → (half,-L+25)
    float u = 1 - t; x = u * u * -half + t * t * half; y = u * u * (-L + 25) + 2 * u * t * (-L - 3) + t * t * (-L + 25);
  };
  q[0] = 0; q[1] = 0; q[2] = -half; q[3] = -L + 25;
  for (int i = 1; i <= 3; i++) qb(i / 4.0f, q[2 + 2 * i], q[3 + 2 * i]);
  q[10] = half; q[11] = -L + 25;
  p.shape(q, 6, cx, cy, angle, c);
  p.text(label, px(cx, angle, lr), py(cy, angle, lr) + 9, 24, lv_color_white(), 'c');
}
static void windQuad(Painter &p) {
  char b[16];
  const float cx = 250, cy = 185;
  p.rect(0, 0, 500, 370, fixed(0x050607));
  lv_color_t panel = col(DISPLAY_BG), ink = col(PRIMARY_TEXT);
  p.rect(2, 8, 108, 354, panel, 14);
  p.rect(390, 8, 108, 354, panel, 14);
  rose(p, cx, cy, 177);
  hull(p, 203, 72, 94, 221, col(HULL), 7);
  float twa = constrain(d.twa, -180.0f, 180.0f), awa = constrain(d.awa, -180.0f, 180.0f);
  windWedge(p, cx, cy, twa, 159, 13, col(ARROW_SECONDARY), "T", 143);
  windWedge(p, cx, cy, awa, 154, 28, lit(ARROWS, 0x2d4b9a), "A", 119);
  snprintf(b, sizeof b, "%.1f", d.aws); p.text(b, 10, 66, 46, ink);
  p.text("AWS", 10, 101, 26, ink);
  snprintf(b, sizeof b, "%d", abs((int)lroundf(awa))); p.text(b, 490, 66, 46, ink, 'r');
  p.text("AWA", 490, 101, 26, ink, 'r');
  p.text("TWS", 10, 301, 26, ink);
  snprintf(b, sizeof b, "%.1f", d.tws); p.text(b, 10, 356, 46, ink);
  p.text("TWA", 490, 301, 26, ink, 'r');
  snprintf(b, sizeof b, "%d", abs((int)lroundf(twa))); p.text(b, 490, 356, 46, ink, 'r');
}

// ── Sail Steer ──
static void sailingSteer(Painter &p) {
  char b[24];
  const float cx = 250, cy = 186;
  lv_color_t ink = col(PRIMARY_TEXT);
  p.rect(0, 0, 500, 370, col(DISPLAY_BG));
  rose(p, cx, cy, 169);
  for (int i = 0; i < 12; i++) {
    float a = i * 30.0f; snprintf(b, sizeof b, "%03d", i * 30);
    p.text(b, px(cx, a, 113), py(cy, a, 113) + 5, 15, ink, 'c');
  }
  const float red[] = {250, 92, 236, 130, 264, 130};
  p.poly(red, 3, lit(ARROW_SECONDARY, 0xe54439));
  hull(p, 221, 132, 58, 94, col(HULL), -1, 0.9f);
  const float blue[] = {183, 183, 230, 172, 230, 194};
  p.poly(blue, 3, lit(ARROWS, 0x3769b4));
  snprintf(b, sizeof b, "BSPD %.1f kn", d.bspd); p.text(b, 14, 38, 22, ink);
  snprintf(b, sizeof b, "TWA %d\xC2\xB0", (int)lroundf(d.twa)); p.text(b, 481, 38, 22, ink, 'r');
  snprintf(b, sizeof b, "TWD %03d\xC2\xB0", (int)lroundf(d.twd) % 360); p.text(b, 14, 352, 26, ink);
  snprintf(b, sizeof b, "TWS %.1f kn", d.tws); p.text(b, 484, 352, 26, ink, 'r');
}

// ── Laylines (instrument space; the rose part is drawn at 1.17x about 444,398) ──
static void laylines(Painter &p, Painter &r) {
  char b[16];
  lv_color_t paper = lit(DISPLAY_BG, 0xf9fafb), ink = lit(PRIMARY_TEXT, 0x242529);
  p.rect(108, 134, 674, 522, paper);
  const float cx = 444, cy = 412;
  rose(r, cx, cy, 215);
  r.disc(cx, cy, 140, lit(WIND_NEUTRAL, 0x56616a));
  r.disc(cx, cy, 111, paper);
  auto rel = [](float dir) { return sim::rel180(dir - d.heading); };
  // wind wedges: apex at the centre, curved top on the inner ring, radial edge
  auto slice = [&](float angle, float half, lv_color_t fill, lv_opa_t fo, lv_color_t edge, lv_opa_t eo) {
    float q[14]; q[0] = cx; q[1] = cy;
    for (int i = 0; i <= 4; i++) { float a = angle - half + 2 * half * i / 4; q[2 + 2 * i] = px(cx, a, 112); q[3 + 2 * i] = py(cy, a, 112); }
    r.poly(q, 6, fill, fo);
    r.line(cx, cy, px(cx, angle + half, 112), py(cy, angle + half, 112), edge, 6, true, eo);
  };
  float green = -max(9.0f, min(70.0f, d.twa * .48f)), red = max(12.0f, min(125.0f, d.twa * 1.95f));
  slice(green, 8, lit(WIND_STARBOARD, 0x7ec23c), 138, lit(WIND_STARBOARD, 0x5bb022), 217);
  slice(red, 10, lit(WIND_PORT, 0xd33a46), 135, lit(WIND_PORT, 0xd52634), 232);
  // 65 % hull over the paper, pre-blended: a translucent stroke would bead at the segment joints
  uint32_t hullRaw = T.color[HULL] != TOK_DEFAULT[HULL] ? T.color[HULL] : 0x585c5b;
  uint32_t paperRaw = T.night ? 0x07090b : T.color[DISPLAY_BG] != TOK_DEFAULT[DISPLAY_BG] ? T.color[DISPLAY_BG] : 0xf9fafb;
  hull(r, 399, 325, 88, 164, mixColor(paperRaw, hullRaw, 0.65f), 8);
  // degree ring labels (head up), dots and ticks
  static const char *const LBL[] = {"090", "150", "180", "210", "240", "270", "300", "330", "N", "030", "060"};
  for (const char *v : LBL) {
    float a = rel(v[0] == 'N' ? 0 : atof(v));
    r.text(v, px(cx, a, 127), py(cy, a, 127) + 8, v[0] == 'N' ? 24 : 21, paper, 'c');
  }
  for (int abs10 = 0; abs10 < 360; abs10 += 10) { float a = rel(abs10); r.disc(px(cx, a, 168), py(cy, a, 168), abs10 % 30 ? 2.5f : 3, lit(GRID, 0x8a9299)); }
  for (int abs30 = 0; abs30 < 360; abs30 += 30) { float a = rel(abs30); r.line(px(cx, a, 160), py(cy, a, 160), px(cx, a, 177), py(cy, a, 177), paper, abs30 % 90 ? 3.5f : 4.5f); }
  r.line(cx, cy - 168 - 11, cx, cy - 168 + 10, paper, 7);
  // true-wind marker: base on the outer ring, tip towards the centre
  float br = rel(d.twd);
  const float tri[] = {-22, -22, 22, -22, 0, 36};
  r.shape(tri, 3, px(cx, br, 180), py(cy, br, 180), br, lit(ARROWS, 0x334779));
  r.text("T", px(cx, br, 180), py(cy, br, 180) + 10, 27, lv_color_white(), 'c');
  const float o1[] = {432, 252, 456, 252, 444, 273}, o2[] = {444, 308, 454, 325, 434, 325};
  r.poly(o1, 3, lit(ARROW_SECONDARY, 0xe8a42c)); r.poly(o2, 3, lit(ARROW_SECONDARY, 0xe8a42c));
  r.rect(411, 274, 66, 31, paper, 8); r.frame(411, 274, 66, 31, lit(SECONDARY_TEXT, 0x353c43), 2, 8);
  snprintf(b, sizeof b, "%d", (int)lroundf(d.heading)); r.text(b, 444, 297, 24, ink, 'c');
  // corner readings (instrument space, not magnified)
  lv_color_t lbl = lit(SECONDARY_TEXT, 0x5d5e60);
  snprintf(b, sizeof b, "%.1f", d.bspd); p.text(b, 134, 232, 58, ink); p.text("BSPD", 134, 263, 24, lbl);
  snprintf(b, sizeof b, "%d", (int)lroundf(d.twa)); p.text(b, 747, 232, 58, ink, 'r'); p.text("TWA", 747, 263, 24, lbl, 'r');
  p.text("TWD", 134, 562, 24, lbl); snprintf(b, sizeof b, "%d", (int)lroundf(d.twd) % 360); p.text(b, 134, 625, 58, ink);
  p.text("TWS", 747, 562, 24, lbl, 'r'); snprintf(b, sizeof b, "%.1f", d.tws); p.text(b, 747, 625, 58, ink, 'r');
}

// ── AIS chart (head up, range 2-15 NM) ──
struct AisHit { uint32_t mmsi; float x, y; };
static AisHit aisDrawn[sim::ais::N];
static int aisDrawnN = 0;
static Painter aisPainter;                   // remembered for touch hit-testing

static void ais(Painter &p) {
  namespace A = sim::ais;
  char b[48], nb[16];
  const float cx = 250, cy = 190, R = 150;
  A::View v[A::N]; int n = A::targets(v);
  A::updateAuto(v, n);
  float scale = A::scale(), hdg = sim::headingTrue();
  lv_color_t ringC = col(AIS_RINGS), muted = lit(SECONDARY_TEXT, 0x9a9a9a), dim = lit(SECONDARY_TEXT, 0x6a6a6a);
  auto fmtNm = [](char *o, float x) { snprintf(o, 8, "%g", roundf(x * 100) / 100); };
  p.rect(0, 0, 500, 370, fixed(0x050607));
  for (int k = 1; k <= 3; k++) {
    p.ring(cx, cy, R * k / 3, ringC, 1.2f);
    fmtNm(b, scale * k / 3); p.text(b, cx + R * k / 3 - 15, cy + 7, 19, ringC, 'c');
  }
  p.line(cx, cy, cx, 2, col(AIS_OWN), 3);
  p.text("HEAD UP", 494, 18, 12, muted, 'r');
  fmt3(nb, hdg); snprintf(b, sizeof b, "HDG %s\xC2\xB0T", nb); p.text(b, 494, 34, 13, ringC, 'r');
  p.text(LV_SYMBOL_UP LV_SYMBOL_DOWN " RANGE   " LV_SYMBOL_NEW_LINE " TARGET", 8, 18, 12, dim);
  aisDrawnN = 0; int beyond = 0;
  bool show = sim::valid.position && A::tracking && sim::valid.ais;
  if (show) {
    for (int pass = 0; pass < 2; pass++)                  // dangerous targets last, never hidden
      for (int i = 0; i < n; i++) {
        const A::View &t = v[i];
        if (t.dangerous != (bool)pass || isnan(t.range)) continue;
        if (t.range > scale) { beyond++; continue; }
        if (A::hideSafe && !t.dangerous && t.t->s->mmsi != A::selected) continue;
        float a = t.brg - hdg, rr = R * t.range / scale, x = cx + rr * sinf(a * DEG), y = cy - rr * cosf(a * DEG);
        aisDrawn[aisDrawnN++] = {t.t->s->mmsi, x, y};
        bool stopped = isnan(t.t->rCog) || t.t->rSog < 0.2f;
        static const float NAV[] = {-4.4f, -18.7f, 4.4f, -18.7f, 8.8f, -9.3f, 8.8f, 18.7f, -8.8f, 18.7f, -8.8f, -9.3f};
        static const float TRI[] = {0, -18.35f, 9.5f, 18.35f, -9.5f, 18.35f};
        static const float LOS[] = {0, -9.15f, 9.15f, 0, 0, 9.15f, -9.15f, 0};
        lv_color_t c = t.dangerous ? col(AIS_DANGER) : col(AIS_TARGET);
        float dir = stopped ? 0 : t.t->rCog - hdg;
        if (t.dangerous) p.shape(TRI, 3, x, y, dir, c);
        else if (stopped) p.shape(LOS, 4, x, y, 0, c);
        else p.shape(NAV, 6, x, y, dir, c);
      }
  }
  // own vessel: teardrop pointing up
  const float own[] = {0, -17.6f, 9.85f, 9.2f, -9.85f, 9.2f};
  p.shape(own, 3, cx, cy, 0, col(AIS_OWN)); p.disc(cx, cy + 9.2f, 9.85f, col(AIS_OWN));
  // selected target: box on the chart + info panel in the corner that hides the least
  const A::View *sel = nullptr;
  for (int i = 0; i < n; i++) if (v[i].t->s->mmsi == A::selected) sel = &v[i];
  if (A::selected && !sel) A::selected = 0;
  if (sel) {
    const A::Target &t = *sel->t; const A::Seed &s = *t.s;
    lv_color_t c = sel->dangerous ? col(AIS_DANGER) : ringC;
    const AisHit *at = nullptr; for (int i = 0; i < aisDrawnN; i++) if (aisDrawn[i].mmsi == s.mmsi) at = &aisDrawn[i];
    if (at) p.frame(at->x - 21, at->y - 21, 42, 42, c, 2);
    char L[8][44]; int nl = 0;
    snprintf(L[nl++], 44, "MMSI %lu   CLASS %c", (unsigned long)s.mmsi, s.cls);
    fmt3(nb, sel->brg); snprintf(L[nl++], 44, "BRG %s\xC2\xB0T   RNG %s NM", isnan(sel->brg) ? "---" : nb, isnan(sel->range) ? "---" : String(sel->range, 2).c_str());
    fmt3(nb, t.rCog); snprintf(L[nl++], 44, "COG %s\xC2\xB0T   SOG %.1f KN", isnan(t.rCog) ? "---" : nb, t.rSog);
    if (isnan(sel->cpa)) snprintf(L[nl++], 44, "CPA ---   TCPA ---");
    else if (sel->tcpa >= 0) snprintf(L[nl++], 44, "CPA %.2f NM   TCPA %.1f MIN", sel->cpa, sel->tcpa);
    else snprintf(L[nl++], 44, "CPA %.2f NM   TCPA ---", sel->cpa);
    if (t.hasStatic) {
      if (s.len) snprintf(L[nl++], 44, "%s   %d x %d M", A::shipType(s.type), s.len, s.beam); else snprintf(L[nl++], 44, "%s", A::shipType(s.type));
      if (s.cls == 'A' && s.nav >= 0) snprintf(L[nl++], 44, "%s", A::NAV_STATUS[s.nav]);
      else if (s.callsign) snprintf(L[nl++], 44, "CALLSIGN %s", s.callsign);
      if (s.cls == 'A' && s.dest) { if (s.draught > 0) snprintf(L[nl++], 44, "DEST %s   DRAUGHT %.1f M", s.dest, s.draught); else snprintf(L[nl++], 44, "DEST %s", s.dest); }
    } else snprintf(L[nl++], 44, "STATIC DATA NOT RECEIVED");
    snprintf(L[nl++], 44, "LAST REPORT %d S AGO", (int)(A::simTime - t.rxTime));
    float w = 236, h = 30 + nl * 17;
    auto cost = [&](float x0) { int c = 0; for (int i = 0; i < aisDrawnN; i++) { const AisHit &q = aisDrawn[i]; if (q.x > x0 - 22 && q.x < x0 + w + 22 && q.y < h + 26) c += q.mmsi == s.mmsi ? 100 : 1; } return c; };
    float left = cost(4) <= cost(500 - w - 4) ? 4 : 500 - w - 4;
    p.rect(left, 4, w, h, fixed(0x000000)); p.frame(left, 4, w, h, c, 2);
    snprintf(b, sizeof b, "%s%s", A::name(t, nb), sel->dangerous ? "  !" : "");
    p.text(b, left + 9, 25, 17, c);
    for (int i = 0; i < nl; i++) p.text(L[i], left + 9, 45 + i * 17, 13, i == 3 && sel->dangerous ? lv_color_hex(0xff8a80) : ringC);
  }
  // footer
  int danger = 0; for (int i = 0; i < n; i++) danger += v[i].dangerous;
  p.text("AIS", 8, 330, 12, muted);
  snprintf(b, sizeof b, "%d TARGETS", n); p.text(b, 8, 348, 15, ringC);
  if (danger) { snprintf(b, sizeof b, "%d DANGEROUS", danger); p.text(b, 8, 365, 15, col(AIS_DANGER)); }
  snprintf(b, sizeof b, "CPA %g NM " LV_SYMBOL_BULLET " %g MIN", A::cpaLimit, A::tcpaLimit); p.text(b, 8, danger ? 312 : 365, 11, dim);
  p.text(A::autoRange ? "RANGE AUTO" : "RANGE", 492, 318, 12, muted, 'r');
  fmtNm(nb, scale); snprintf(b, sizeof b, "%s NM", nb); p.text(b, 492, 346, 26, ringC, 'r');
  p.text(scale == A::STEPS[0] ? "MIN 2 NM" : scale == A::STEPS[A::NSTEPS - 1] ? "MAX 15 NM" : "2 - 15 NM", 492, 364, 11, dim, 'r');
  const char *note = !A::tracking ? "AIS OFF" : !sim::valid.ais ? "NO AIS DATA" : !sim::valid.position ? "NO POSITION" : !n ? "NO AIS TARGETS" : nullptr;
  if (!note && beyond) { fmtNm(nb, scale); snprintf(b, sizeof b, "+%d BEYOND %s NM", beyond, nb); note = b; }
  if (note) p.text(note, cx, 365, 16, muted, 'c');
  aisPainter = p;
}

// ── GPS position and navigation data ──
static void coord(char *o, double deg, bool lat) {
  char hemi = lat ? (deg >= 0 ? 'N' : 'S') : (deg >= 0 ? 'E' : 'W');
  double a = fabs(deg); int whole = (int)a;
  snprintf(o, 24, lat ? "%c %02d\xC2\xB0%06.3f'" : "%c %03d\xC2\xB0%06.3f'", hemi, whole, (a - whole) * 60);
}
static void gps(Painter &p) {
  char b[32];
  lv_color_t ink = col(PRIMARY_TEXT), lbl = col(SECONDARY_TEXT), sep = lit(SEPARATOR, 0xbbbbbb);
  p.rect(0, 0, 500, 370, col(DISPLAY_BG));
  p.text("POS", 15, 35, 22, lbl);
  if (sim::valid.position) { coord(b, d.lat, true); p.text(b, 250, 91, 36, ink, 'c'); coord(b, d.lon, false); p.text(b, 250, 135, 36, ink, 'c'); }
  else p.text("NO POSITION", 250, 112, 30, lit(WARNING, 0x9b0000), 'c');
  p.line(15, 152, 485, 152, sep, 1);
  uint32_t s = d.t + 10 * 3600;                         // demo clock: starts at 10:00:00
  char clk[12]; snprintf(clk, sizeof clk, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600 % 24), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
  char cog[12]; snprintf(cog, sizeof cog, "%03d\xC2\xB0M", (int)lroundf(d.cog) % 360);
  char sog[12]; snprintf(sog, sizeof sog, "%.1f kn", d.sog);
  const char *lab[3] = {"COG", "TIME LOC", "SOG"}, *val[3] = {cog, clk, sog};
  for (int i = 0; i < 3; i++) {
    float x = 18 + i * 163;
    if (i) p.line(x - 8, 159, x - 8, 262, sep, 1);
    p.text(lab[i], x, 184, 19, lbl); p.text(val[i], x, 238, i == 1 ? 26 : 30, ink);
  }
  p.line(15, 269, 485, 269, sep, 1);
  p.text("WPT BRG \xC2\xB0M", 18, 298, 18, lbl); snprintf(b, sizeof b, "%03d", (int)lroundf(d.wptBrg) % 360); p.text(b, 18, 348, 44, ink);
  p.text("XTE", 213, 298, 18, lbl); snprintf(b, sizeof b, "%.2fL", d.xte); p.text(b, 210, 343, 28, ink);
  p.text("WPT DIST NM", 354, 298, 18, lbl); snprintf(b, sizeof b, "%.2f", d.wptDist); p.text(b, 359, 348, 40, ink);
}

// ── unavailable-data screen ──
static void unavailable(Painter &p, const char *msg) {
  p.rect(0, 0, 500, 370, col(DISPLAY_BG));
  p.text(msg, 250, 175, 22, col(WARNING), 'c');
  p.text("SIMULATION - NO NAVIGATION DATA", 250, 205, 15, col(SECONDARY_TEXT), 'c');
}

// ── menu overlay (display space, 480 x 376) ──
static void menuOverlay(Painter &p) {
  const float x = 24, y = 16, w = 432, h = 344, row = 32;
  lv_color_t bg = col(MENU), head = col(MENU_HEADER), txt = col(MENU_TEXT), acc = col(MENU_ACCENT);
  p.rect(x, y, w, h, bg); p.frame(x, y, w, h, lit(MENU_HEADER, 0x424547), 2);
  p.rect(x, y, w, 40, head);
  const char *title = menu::sub < 0 ? "Menu" : menu::ITEMS[menu::sub].label;
  p.text(title, x + 14, y + 28, 20, txt, 'l', false);
  p.text("SIM", x + w - 14, y + 27, 13, acc, 'r', false);
  p.line(x + 3, y + 42, x + w - 3, y + 42, acc, 2);
  float top = y + 48;
  if (menu::sub >= 0 && !menu::ITEMS[menu::sub].n) {           // About
    char l[6][48];
    snprintf(l[0], 48, "Marine Display  firmware %s", FW_VERSION);
    snprintf(l[1], 48, "Waveshare ESP32-S3-Touch-LCD-4");
    snprintf(l[2], 48, "Uptime %lu s", (unsigned long)(millis() / 1000));
    snprintf(l[3], 48, "Heap %u KB  PSRAM %u KB free", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024));
    snprintf(l[4], 48, "Simulation only: no NMEA 2000 output");
    snprintf(l[5], 48, "MIT licence");
    for (int i = 0; i < 6; i++) p.text(l[i], x + 18, top + 26 + i * 32, 16, txt, 'l', false);
  } else {
    int n = menu::count(), cur = menu::sub >= 0 && menu::ITEMS[menu::sub].cur ? menu::ITEMS[menu::sub].cur() : -1;
    for (int i = menu::scroll; i < min(n, menu::scroll + menu::ROWS); i++) {
      float Y = top + (i - menu::scroll) * row;
      bool focus = i == menu::cursor;
      if (focus) p.rect(x + 3, Y, w - 6, row - 2, acc);
      else p.line(x + 10, Y + row - 2, x + w - 12, Y + row - 2, lit(MENU_HEADER, 0x383a36), 1);
      lv_color_t tc = focus ? lv_color_hex(0x12170c) : txt;
      p.text(menu::label(i), x + 20, Y + 22, 17, tc, 'l', false);
      if (menu::sub < 0 && menu::ITEMS[i].n) p.text(LV_SYMBOL_RIGHT, x + w - 18, Y + 21, 14, tc, 'r', false);
      if (menu::sub >= 0 && i == cur) p.text(LV_SYMBOL_OK, x + w - 18, Y + 21, 14, tc, 'r', false);
    }
    if (n > menu::ROWS) {
      float sh = menu::ROWS * row;
      p.rect(x + w - 7, top, 4, sh, lit(MENU_HEADER, 0x353b2d));
      p.rect(x + w - 7, top + menu::scroll * sh / n, 4, max(17.0f, sh * menu::ROWS / n), acc);
    }
  }
  float foot = y + h - 30;
  p.rect(x + 1, foot, w - 2, 29, lit(MENU, 0x1c1e1e));
  p.text(LV_SYMBOL_LIST " Back", x + 70, foot + 20, 13, lit(MENU_TEXT, 0xd6d8d1), 'c', false);
  p.text(LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate", x + w / 2, foot + 20, 13, lit(MENU_TEXT, 0xd6d8d1), 'c', false);
  p.text(LV_SYMBOL_NEW_LINE " Select", x + w - 70, foot + 20, 13, lit(MENU_TEXT, 0xd6d8d1), 'c', false);
}

}  // namespace pages
