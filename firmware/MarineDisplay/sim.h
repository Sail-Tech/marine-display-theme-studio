// sim.h — simulated instrument data and AIS traffic. Display-only: nothing is ever
// transmitted, there is no NMEA 2000 / CAN connection and no autopilot output.
//
// Same model as the web studio:
//   - instrument values move gently around a demo state (1 Hz),
//   - own position advances by dead reckoning on COG/SOG (PGN 129025 from 129026),
//   - AIS targets move on their own course and are only "received" at their real
//     reporting interval (Class A position 129038 ~4 s, Class B 129039 30 s, static data
//     129794 / 129809 / 129810 every 6 min); the display shows the last received report,
//   - CPA/TCPA from relative position and velocity (equirectangular, < 1 px error at 15 NM).
#pragma once
#include <Arduino.h>
#include <math.h>

namespace sim {

static constexpr float DEG = (float)M_PI / 180.0f;
static float norm360(float v) { v = fmodf(v, 360.0f); return v < 0 ? v + 360.0f : v; }
static float rel180(float v) { v = norm360(v + 180.0f) - 180.0f; return v; }

struct Data {
  float heading = 113, variation = -1.5f;          // 127250 (magnetic) + variation
  float cog = 113, sog = 11.0f;                    // 129026
  float bspd = 11.2f;                              // 128259
  float depth = 10.0f;                             // 128267
  float tws = 8.9f, twa = 57, twd = 170;           // 130306 true
  float aws = 13.2f, awa = 37;                     // 130306 apparent
  double lat = 38.6625, lon = -9.42;               // 129025, starts at sea off Cascais
  float wptBrg = 125, wptDist = 0.19f, xte = 0.05f;
  uint32_t t = 0;                                  // seconds since boot (demo clock)
};
static Data d;

// Data scenarios (unavailable-data states), as in the web studio's demo data.
struct Valid { bool heading = true, position = true, ais = true; };
static Valid valid;

static float headingTrue() { return norm360(d.heading + d.variation); }

static void offset(double &lat, double &lon, float brgDeg, float nm) {
  lat += nm * cosf(brgDeg * DEG) / 60.0;
  lon += nm * sinf(brgDeg * DEG) / (60.0 * cos(lat * DEG));
}

// ── AIS ─────────────────────────────────────────────────────────────────
namespace ais {

static constexpr float STEPS[] = {2, 3, 6, 9, 12, 15};   // range limited to 2-15 NM
static constexpr int NSTEPS = 6;
static constexpr uint32_t MAX_AGE_S = 180, STATIC_PERIOD_S = 360;
static const char *const NAV_STATUS[] = {
  "UNDER WAY USING ENGINE", "AT ANCHOR", "NOT UNDER COMMAND", "RESTRICTED MANOEUVRABILITY", "CONSTRAINED BY DRAUGHT",
  "MOORED", "AGROUND", "ENGAGED IN FISHING", "UNDER WAY SAILING", "RESERVED (HSC)", "RESERVED (WIG)",
  "TOWING ASTERN", "PUSHING AHEAD", "RESERVED", "AIS-SART ACTIVE", "NOT DEFINED"
};
static const char *shipType(int c) {
  if (c == 30) return "FISHING"; if (c == 31 || c == 32) return "TOWING"; if (c == 33) return "DREDGING";
  if (c == 34) return "DIVING OPS"; if (c == 35) return "MILITARY"; if (c == 36) return "SAILING";
  if (c == 37) return "PLEASURE CRAFT"; if (c >= 40 && c < 50) return "HIGH-SPEED CRAFT"; if (c == 50) return "PILOT VESSEL";
  if (c == 51) return "SEARCH AND RESCUE"; if (c == 52) return "TUG"; if (c == 53) return "PORT TENDER";
  if (c == 55) return "LAW ENFORCEMENT"; if (c == 58) return "MEDICAL TRANSPORT"; if (c >= 60 && c < 70) return "PASSENGER";
  if (c >= 70 && c < 80) return "CARGO"; if (c >= 80 && c < 90) return "TANKER"; if (c >= 90 && c < 100) return "OTHER";
  return "UNKNOWN TYPE";
}

struct Seed {
  uint32_t mmsi; char cls; const char *name, *callsign; uint32_t imo; int type; int len, beam; float draught;
  const char *dest; int nav; float cog, sog;
  bool meet; float a, b, c;      // meet: (minutes to CPA, CPA NM, side °) — else (bearing rel. COG, range NM, -)
  bool noStatic;
};
// Placement relative to own COG so the scene reads the same whatever the start heading.
static const Seed SEED[] = {
  {263401230, 'A', "TEJO TRADER", "CQAB7", 9301234, 70, 118, 18, 6.8f, "LEIXOES", 0, 205, 12.5f, true, 8, 0.2f, 90, false},
  {263512870, 'B', "BOA VIAGEM", "CSB4521", 0, 36, 12, 4, 0, nullptr, -1, 62, 5.2f, true, 14, 0.8f, -90, false},
  {263204410, 'A', "PILOT LISBOA", "CSPL2", 0, 50, 16, 5, 1.6f, "LISBOA", 1, NAN, 0, false, 55, 4.2f, 0, false},
  {263118300, 'A', "N SRA DA NAZARE", "CSNZ8", 0, 30, 22, 6, 2.4f, "PENICHE", 7, 340, 3.1f, false, -70, 5.5f, 0, false},
  {255806120, 'A', "ESTORIL SPIRIT", "CQEX3", 9412876, 80, 183, 32, 11.2f, "SINES", 0, 195, 13.4f, false, 150, 7.6f, 0, false},
  {263777010, 'A', "TAGUS FERRY 12", "CSTF2", 0, 60, 40, 9, 2.1f, "CACILHAS", 0, 290, 9.0f, false, 20, 11.5f, 0, false},
  {235112840, 'B', "SEA SPARROW", "MXYZ5", 0, 37, 14, 4, 0, nullptr, -1, 180, 6.4f, false, -120, 9.3f, 0, false},
  {636019221, 'A', "ATLANTIC MERIDIAN", "D5QR8", 9654321, 71, 294, 40, 13.5f, "NL RTM", 0, 350, 15.2f, false, -95, 17.5f, 0, false},
  {263999123, 'B', nullptr, nullptr, 0, -1, 0, 0, 0, nullptr, -1, 100, 4.0f, false, 115, 3.4f, 0, true},
};
static constexpr int N = sizeof(SEED) / sizeof(SEED[0]);

struct Target {
  const Seed *s;
  double lat, lon; float cog, sog, hdg;              // kinematics ("truth", not on the bus)
  float period, next, nextStatic;
  // last received reports
  bool hasPos = false, hasStatic = false;
  double rLat, rLon; float rCog, rSog, rHdg; float rxTime;
};
static Target fleet[N];
static float simTime = 0;
static bool tracking = true, wasReceiving = true;
static uint32_t rx129038 = 0, rx129039 = 0, rx129794 = 0, rx129809 = 0, rx129810 = 0;

// Settings (persisted by the UI)
static bool autoRange = true;
static int rangeIdx = 2;                 // manual range: index into STEPS
static float autoScale = 6;
static uint32_t autoSince = 0;
static float cpaLimit = 0.5f, tcpaLimit = 10;
static bool hideSafe = false;
static uint32_t selected = 0;            // MMSI, 0 = none

struct View {                            // a target as the display knows it
  const Target *t; float range, brg, cpa, tcpa; bool dangerous;
};

static void seed() {
  simTime = 0; selected = 0;
  rx129038 = rx129039 = rx129794 = rx129809 = rx129810 = 0;
  for (int i = 0; i < N; i++) {
    const Seed &s = SEED[i]; Target &t = fleet[i];
    t = Target(); t.s = &s;
    double lat = d.lat, lon = d.lon;
    if (s.meet) {                         // meets own vessel at CPA s.b after s.a minutes
      float h = s.a / 60.0f;
      offset(lat, lon, d.cog, d.sog * h);
      offset(lat, lon, d.cog + s.c, s.b);
      offset(lat, lon, s.cog + 180, s.sog * h);
    } else offset(lat, lon, d.cog + s.a, s.b);
    t.lat = lat; t.lon = lon; t.cog = s.cog; t.sog = s.sog;
    t.hdg = isnan(s.cog) ? NAN : norm360(s.cog + (i % 3) - 1);
    bool moving = !isnan(s.cog) && s.sog > 0.5f;
    t.period = s.cls == 'B' ? 30 : moving ? 4 : 10;
    t.next = i * 0.37f;
    t.nextStatic = s.noStatic ? INFINITY : 0.5f + i * 0.21f;
  }
}

static void solve(double tLat, double tLon, float tCog, float tSog, View &v) {
  float k = cosf(d.lat * DEG);
  float ry = (tLat - d.lat) * 60.0f, rx = (tLon - d.lon) * 60.0f * k;
  v.range = sqrtf(rx * rx + ry * ry);
  v.brg = norm360(atan2f(rx, ry) / DEG);
  v.cpa = NAN; v.tcpa = NAN;
  if (isnan(tCog) || isnan(tSog)) return;
  float vx = tSog * sinf(tCog * DEG) - d.sog * sinf(d.cog * DEG);
  float vy = tSog * cosf(tCog * DEG) - d.sog * cosf(d.cog * DEG);
  float v2 = vx * vx + vy * vy;
  if (v2 < 1e-6f) { v.cpa = v.range; return; }            // same track
  float th = -(rx * vx + ry * vy) / v2;
  v.tcpa = th * 60.0f;
  if (th < 0) { v.cpa = v.range; return; }                // already opening
  v.cpa = sqrtf((rx + vx * th) * (rx + vx * th) + (ry + vy * th) * (ry + vy * th));
}

static void advance(float dt) {
  bool receiving = tracking && valid.ais;
  simTime += dt;
  for (int i = 0; i < N; i++) {
    Target &t = fleet[i];
    if (!isnan(t.cog) && t.sog > 0) offset(t.lat, t.lon, t.cog, t.sog * dt / 3600.0f);
    // Keep the demo populated: a vessel that has opened beyond 18 NM turns back.
    View v; solve(t.lat, t.lon, t.cog, t.sog, v);
    if (v.range > 18 && !(v.tcpa > 0)) { t.cog = norm360(t.cog + 180); t.hdg = norm360(t.hdg + 180); }
    if (!receiving) { t.hasPos = t.hasStatic = false; continue; }
    if (!wasReceiving) t.nextStatic = t.s->noStatic ? INFINITY : simTime + 2 + random(0, 20);   // tracking just resumed
    if (simTime >= t.next) {
      t.hasPos = true; t.rLat = t.lat; t.rLon = t.lon; t.rCog = t.cog; t.rSog = t.sog; t.rHdg = t.s->cls == 'A' ? t.hdg : NAN;
      t.rxTime = simTime; t.next = simTime + t.period;
      if (t.s->cls == 'A') rx129038++; else rx129039++;
    }
    if (simTime >= t.nextStatic) {
      t.hasStatic = true; t.nextStatic = simTime + STATIC_PERIOD_S;
      if (t.s->cls == 'A') rx129794++; else { rx129809++; rx129810++; }
    }
    if (t.hasPos && simTime - t.rxTime > MAX_AGE_S) t.hasPos = false;
  }
  wasReceiving = receiving;
}

// Received targets, solved against own vessel. Returns the count.
static int targets(View *out) {
  int n = 0;
  for (int i = 0; i < N; i++) {
    const Target &t = fleet[i];
    if (!t.hasPos) continue;
    View &v = out[n++]; v.t = &t;
    if (valid.position) solve(t.rLat, t.rLon, t.rCog, t.rSog, v);
    else { v.range = v.brg = v.cpa = v.tcpa = NAN; }
    v.dangerous = !isnan(v.cpa) && !isnan(v.tcpa) && v.tcpa >= 0 && v.cpa <= cpaLimit && v.tcpa <= tcpaLimit;
  }
  return n;
}

static const char *name(const Target &t, char *buf) {
  if (t.hasStatic && t.s->name) return t.s->name;
  snprintf(buf, 16, "MMSI %lu", (unsigned long)t.s->mmsi);
  return buf;
}

static float scale() { return autoRange ? autoScale : STEPS[rangeIdx]; }

// Auto: widen at once when a target leaves the outer ring, narrow only after 10 s fitting
// the next step down; with no targets the scale does not move. Targets beyond 15 NM are not chased.
static void updateAuto(const View *v, int n) {
  if (!autoRange) return;
  float rmax = -1;
  for (int i = 0; i < n; i++) if (!isnan(v[i].range) && v[i].range <= STEPS[NSTEPS - 1]) rmax = max(rmax, v[i].range);
  if (rmax < 0) { autoSince = 0; return; }
  int i = 0; while (i < NSTEPS && STEPS[i] != autoScale) i++; if (i == NSTEPS) i = 2;
  if (rmax > STEPS[i]) { while (i < NSTEPS - 1 && rmax > STEPS[i]) i++; autoScale = STEPS[i]; autoSince = 0; }
  else if (i > 0 && rmax <= STEPS[i - 1]) {
    if (!autoSince) autoSince = millis();
    else if (millis() - autoSince > 10000) { autoScale = STEPS[i - 1]; autoSince = 0; }
  } else autoSince = 0;
}

// +1 widens, -1 narrows; leaving Auto keeps the scale currently shown.
static void stepRange(int k) {
  float cur = scale(); int i = 0;
  while (i < NSTEPS - 1 && STEPS[i] < cur) i++;
  autoRange = false;
  rangeIdx = constrain(i + k, 0, NSTEPS - 1);
}

// Enter walks a STABLE order (MMSI); with nothing selected it starts at the nearest
// target, and after the last one it clears the selection.
static void stepTarget() {
  View v[N]; int n = targets(v);
  if (!n) { selected = 0; return; }
  int cur = -1;
  for (int i = 0; i < n; i++) if (v[i].t->s->mmsi == selected) cur = i;
  if (cur < 0) {
    int best = 0;
    for (int i = 1; i < n; i++) if ((isnan(v[i].range) ? 1e9f : v[i].range) < (isnan(v[best].range) ? 1e9f : v[best].range)) best = i;
    selected = v[best].t->s->mmsi; return;
  }
  uint32_t next = 0;                                   // smallest MMSI above the current one
  for (int i = 0; i < n; i++) { uint32_t m = v[i].t->s->mmsi; if (m > selected && (!next || m < next)) next = m; }
  selected = next;
}

}  // namespace ais

// ── 1 Hz demo tick ──
static void tick() {
  uint32_t t = ++d.t;
  d.heading = norm360(113 + roundf(4 * sinf(t / 17.0f)));
  d.cog = norm360(d.heading + roundf(2 * sinf(t / 12.0f)));
  d.bspd = 11.2f + 0.5f * sinf(t / 8.0f);
  d.sog = d.bspd + 0.2f * sinf(t / 6.0f);
  d.depth = 10 + 0.6f * sinf(t / 11.0f);
  d.tws = 8.9f + 0.7f * sinf(t / 9.0f);
  d.awa = roundf(37 + 3 * sinf(t / 14.0f));
  d.twa = d.awa + 20;
  d.twd = norm360(d.heading + d.twa);
  d.aws = 13.2f + 0.8f * sinf(t / 10.0f);
  d.xte = 0.05f + 0.02f * sinf(t / 15.0f);
  d.wptDist = max(0.01f, 0.19f - 0.001f * (t % 180));
  d.wptBrg = norm360(125 + roundf(3 * sinf(t / 25.0f)));
  offset(d.lat, d.lon, d.cog, d.sog / 3600.0f);       // dead reckoning, 1 s
  ais::advance(1);
}

static void begin() { ais::seed(); }

}  // namespace sim
