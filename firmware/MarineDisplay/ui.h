// ui.h — board bring-up (panel, touch, CH32V003 helper), LVGL set-up and input.
// The screen layout and drawing live in screen.h.
// Board facts below were measured on the Waveshare ESP32-S3-Touch-LCD-4 (Rev 4).
#pragma once
#include <Arduino_GFX_Library.h>
#include <TouchDrvGT911.hpp>
#include <Wire.h>
#include "screen.h"

namespace board {

// ── CH32V003 I/O helper at I2C 0x24 (backlight PWM, LCD/touch reset, buzzer, SD D3) ──
static constexpr uint8_t CH32 = 0x24, REG_DIR = 0x02, REG_OUT = 0x03, REG_PWM = 0x05;
static constexpr uint8_t PIN_TP_RST = 1 << 1, PIN_LCD_RST = 1 << 3, PIN_SD_D3 = 1 << 4, PIN_SYS_EN = 1 << 5;
// The microSD's D3/CS is kept HIGH in every write: left LOW the card enters SPI mode.
static constexpr uint8_t OUT_ON = PIN_SYS_EN | PIN_LCD_RST | PIN_TP_RST | PIN_SD_D3;

static bool ch32Write(uint8_t reg, uint8_t v) {
  for (int i = 0; i < 5; i++) {
    Wire.beginTransmission(CH32); Wire.write(reg); Wire.write(v);
    if (Wire.endTransmission() == 0) return true;
    delay(10 + i * 10);
  }
  return false;
}
static bool ch32Read(uint8_t reg, uint8_t &v) {
  Wire.beginTransmission(CH32); Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)CH32, 1) != 1) return false;
  v = Wire.read(); return true;
}
// A previous run may have left the I2C bus with SDA held low: clock it free.
static void recoverI2C(int sda, int scl) {
  pinMode(sda, INPUT_PULLUP); pinMode(scl, OUTPUT); digitalWrite(scl, HIGH); delayMicroseconds(10);
  for (int i = 0; i < 9 && digitalRead(sda) == LOW; i++) { digitalWrite(scl, LOW); delayMicroseconds(10); digitalWrite(scl, HIGH); delayMicroseconds(10); }
  pinMode(sda, INPUT_PULLUP); pinMode(scl, INPUT_PULLUP); delay(2);
}
static bool ch32Begin() {
  recoverI2C(15, 7);
  Wire.begin(15, 7, 400000);
  delay(20);
  for (int attempt = 0; attempt < 3; attempt++) {
    // reset pulse on the LCD and touch, then power on with SD D3 high
    if (ch32Write(REG_DIR, 0xff) && ch32Write(REG_OUT, PIN_SD_D3)) {
      delay(200);
      if (ch32Write(REG_DIR, 0xff) && ch32Write(REG_OUT, OUT_ON)) { delay(200); return true; }
    }
    delay(20);
  }
  return false;
}
// MEASURED: the backlight PWM is inverted (0 = brightest, 255 = off), and the CH32 can
// acknowledge a write and drop it, so the value is read back and retried.
static void backlight(uint8_t percent) {
  uint8_t v = (uint8_t)((100 - constrain(percent, 10, 100)) * 255 / 100), rb = 0;
  for (int i = 0; i < 3; i++) { if (ch32Write(REG_PWM, v)) { delay(2); if (ch32Read(REG_PWM, rb) && rb == v) return; } delay(5); }
}

// ── RGB panel (ST7701, 480 x 480) and GT911 touch ──
static Arduino_DataBus *bus;
static Arduino_ESP32RGBPanel *panel;
static Arduino_RGB_Display *gfx;
static TouchDrvGT911 touch;
static bool touchOk = false;
static int16_t tpx[5], tpy[5];

static bool beginTouch() {
  uint8_t addr = 0;
  for (uint8_t a : {GT911_SLAVE_ADDRESS_L, GT911_SLAVE_ADDRESS_H}) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) { addr = a; break; } }
  if (!addr) return false;
  touch.setPins(-1, -1);
  return touch.begin(Wire, addr, 15, 7);
}

static bool beginPanel() {
  bus = new Arduino_SWSPI(GFX_NOT_DEFINED, 42, 2, 1, GFX_NOT_DEFINED);
  // PCLK 10 MHz (the driver default is 12): the panel streams from a PSRAM frame buffer
  // through SRAM bounce buffers; the slower clock gives each refill more time and stops
  // the picture from rolling under load.
  panel = new Arduino_ESP32RGBPanel(40, 39, 38, 41, 46, 3, 8, 18, 17, 14, 13, 12, 11, 10, 9, 5, 45, 48, 47, 21,
                                    1, 10, 8, 50, 1, 10, 8, 20, 0, 10000000L);
  // rotation 2: the panel is mounted upside down
  gfx = new Arduino_RGB_Display(480, 480, panel, 2, true, bus, GFX_NOT_DEFINED,
                                st7701_type1_init_operations, sizeof(st7701_type1_init_operations));
  return gfx->begin();
}

}  // namespace board

namespace ui {

static lv_obj_t *titleObj, *dispObj, *keysObj;
static bool longFired = false, gestured = false;
static lv_disp_draw_buf_t drawBuf;

// ── input ──
static void touchRead(lv_indev_drv_t *, lv_indev_data_t *data) {
  // A finger held still sometimes reads as "no point" for a moment: a release only
  // counts after 80 ms without contact.
  static lv_coord_t lx = 0, ly = 0; static uint32_t lastSeen = 0; static bool down = false;
  uint8_t n = board::touchOk ? board::touch.getPoint(board::tpx, board::tpy, 1) : 0;
  if (n > 0) {
    lx = 480 - board::tpx[0]; ly = 480 - board::tpy[0];        // panel rotated 180°
    lastSeen = millis(); down = true;
  } else if (down && millis() - lastSeen >= 80) down = false;
  data->point.x = lx; data->point.y = ly;
  data->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

static int keyAt(lv_event_t *e) {
  lv_point_t pt; lv_indev_get_point(lv_indev_get_act(), &pt);
  lv_area_t a = coordsOf(e);
  return constrain((pt.x - a.x1) / 96, 0, 4);
}
static void keysEvent(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_PRESSED) { keyDown = keyAt(e); longFired = false; needChrome = true; }
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) { keyDown = K_NONE; needChrome = true; }
  else if (c == LV_EVENT_LONG_PRESSED) { longFired = true; handleKey(keyAt(e), true); }
  else if (c == LV_EVENT_CLICKED && !longFired) handleKey(keyAt(e), false);
}

static void displayEvent(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_PRESSED) { gestured = false; return; }
  if (c == LV_EVENT_GESTURE) {
    if (menu::open) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT || dir == LV_DIR_TOP) { setPage(page + 1); gestured = true; }
    else if (dir == LV_DIR_RIGHT || dir == LV_DIR_BOTTOM) { setPage(page - 1); gestured = true; }
    return;
  }
  if (c != LV_EVENT_CLICKED || gestured) return;
  lv_point_t pt; lv_indev_get_point(lv_indev_get_act(), &pt);
  lv_area_t a = coordsOf(e);
  float x = pt.x - a.x1, y = pt.y - a.y1;
  if (menu::open) {                                    // tap a row = select it; tap outside = close
    if (x < 24 || x > 456 || y < 16 || y > 360) { menu::close(); return; }
    if (menu::sub >= 0 && !menu::ITEMS[menu::sub].n) { menu::back(); return; }
    int row = (int)((y - 64) / 32);
    if (y >= 64 && row < menu::ROWS && menu::scroll + row < menu::count()) { menu::cursor = menu::scroll + row; menu::select(); }
    return;
  }
  if (page == PG_AIS) {                                 // tap a target to select it, empty water to clear
    const Painter &p = pages::aisPainter;
    float qx = (pt.x - p.ox) / p.k, qy = (pt.y - p.oy) / p.k, best = 22 * 22;
    uint32_t m = 0;
    for (int i = 0; i < pages::aisDrawnN; i++) {
      float dx = pages::aisDrawn[i].x - qx, dy = pages::aisDrawn[i].y - qy, d2 = dx * dx + dy * dy;
      if (d2 < best) { best = d2; m = pages::aisDrawn[i].mmsi; }
    }
    sim::ais::selected = m; needRedraw = true;
  }
}

static lv_obj_t *area(lv_obj_t *parent, int y, int h, lv_event_cb_t draw) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, 0, y); lv_obj_set_size(o, 480, h);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(o, draw, LV_EVENT_DRAW_MAIN, nullptr);
  return o;
}

static void tickCb(void *) { lv_tick_inc(2); }

static bool begin(Stream &log) {
  if (!board::ch32Begin()) log.println(F("[ui] CH32V003 I/O helper not answering"));
  board::touchOk = board::beginTouch();
  log.printf("[ui] GT911 touch: %s\n", board::touchOk ? "OK" : "NOT FOUND");
  if (!board::beginPanel()) { log.println(F("[ui] display init FAILED")); return false; }
  board::backlight(theme::T.brightness);

  lv_init();
  // One 20-line draw buffer in internal RAM; LVGL flushes it into the panel's PSRAM frame buffer.
  const uint32_t lines = 20;
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(480 * lines * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf) buf = (lv_color_t *)heap_caps_malloc(480 * lines * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  if (!buf) { log.println(F("[ui] no memory for the draw buffer")); return false; }
  lv_disp_draw_buf_init(&drawBuf, buf, nullptr, 480 * lines);
  static lv_disp_drv_t dd; lv_disp_drv_init(&dd);
  dd.hor_res = 480; dd.ver_res = 480; dd.draw_buf = &drawBuf;
  dd.flush_cb = [](lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *c) {
    board::gfx->draw16bitRGBBitmap(a->x1, a->y1, (uint16_t *)&c->full, a->x2 - a->x1 + 1, a->y2 - a->y1 + 1);
    lv_disp_flush_ready(d);
  };
  lv_disp_drv_register(&dd);
  static lv_indev_drv_t id; lv_indev_drv_init(&id);
  id.type = LV_INDEV_TYPE_POINTER; id.read_cb = touchRead;
  lv_indev_drv_register(&id);
  const esp_timer_create_args_t ta = {.callback = &tickCb, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK, .name = "lv_tick", .skip_unhandled_events = true};
  esp_timer_handle_t th; esp_timer_create(&ta, &th); esp_timer_start_periodic(th, 2000);

  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  titleObj = area(scr, 0, TITLE_H, drawTitle);
  dispObj = area(scr, DISP_Y, DISP_H, drawDisplay);
  keysObj = area(scr, KEYS_Y, KEYS_H, drawKeys);
  // LVGL 8 sends LV_EVENT_GESTURE to the first object WITHOUT GESTURE_BUBBLE.
  lv_obj_add_flag(dispObj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(dispObj, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(dispObj, displayEvent, LV_EVENT_ALL, nullptr);
  lv_obj_add_flag(keysObj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(keysObj, keysEvent, LV_EVENT_ALL, nullptr);
  return true;
}

// Called from loop(): redraws what changed.
static void service() {
  static uint32_t rev = 0;
  if (rev != theme::revision) { rev = theme::revision; board::backlight(theme::T.brightness); needRedraw = needChrome = true; }
  if (needChrome) { needChrome = false; lv_obj_invalidate(titleObj); lv_obj_invalidate(keysObj); }
  if (needRedraw) { needRedraw = false; lv_obj_invalidate(dispObj); }
  lv_timer_handler();
}

}  // namespace ui
