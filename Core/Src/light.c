#include "light.h"
#include "app_config.h"
static light_mode_t mode;
static uint16_t selected_hue, duration;
static uint8_t level, max_level, from, target, preview, sent_level, sent_valid;
static rgb_t sent_color;
static uint32_t started, last_frame;
static uint32_t seen_errors;
static light_effect_fn custom;
static void *custom_context;
rgb_t light_hue_rgb(uint16_t hue) {
    rgb_t c; uint16_t h = hue % 360U; uint8_t x = (uint8_t)((h % 60U) * 255U / 60U);
    switch (h / 60U) {
    case 0: c.r = 255; c.g = x; c.b = 0; break;
    case 1: c.r = 255 - x; c.g = 255; c.b = 0; break;
    case 2: c.r = 0; c.g = 255; c.b = x; break;
    case 3: c.r = 0; c.g = 255 - x; c.b = 255; break;
    case 4: c.r = x; c.g = 0; c.b = 255; break;
    default: c.r = 255; c.g = 0; c.b = 255 - x; break;
    } return c;
}
void light_init(uint16_t hue, uint8_t brightness) {
    selected_hue = hue % 360U; max_level = brightness; level = preview = sent_valid = 0;
    mode = LIGHT_OFF; custom = 0; last_frame = 0; seen_errors = ws2812_errors();
}
static void advance(uint32_t now) {
    if (mode == LIGHT_FADE_IN || mode == LIGHT_FADE_OUT) {
        uint32_t e = now - started;
        if (!duration || e >= duration) { level = target; mode = target ? LIGHT_ON : LIGHT_OFF; }
        else level = (uint8_t)((int32_t)from + ((int32_t)target - from) * (int32_t)e / duration);
    }
}
static void fade(uint32_t now, uint16_t ms, uint8_t to) {
    advance(now); from = level; target = to; started = now; duration = ms;
    mode = to ? LIGHT_FADE_IN : LIGHT_FADE_OUT; custom = 0;
}
void light_fade_in(uint32_t now, uint16_t ms) { fade(now, ms, max_level); }
void light_fade_out(uint32_t now, uint16_t ms) { fade(now, ms, 0); }
void light_set_hue(uint16_t hue) { selected_hue = hue % 360U; }
void light_preview(int enable) { preview = enable != 0; }
void light_set_effect(light_effect_fn fn, void *ctx) {
    custom = fn; custom_context = ctx; mode = fn ? LIGHT_CUSTOM : (level ? LIGHT_ON : LIGHT_OFF);
}
light_mode_t light_mode(void) { return mode; }
void light_poll(uint32_t now) {
    rgb_t c; uint8_t b;
    advance(now);
    if (seen_errors != ws2812_errors()) { seen_errors = ws2812_errors(); sent_valid = 0; }
    if (now - last_frame < APP_LIGHT_FRAME_MS) return;
    last_frame = now; c = light_hue_rgb(selected_hue); b = level;
    if (mode == LIGHT_CUSTOM && custom) custom(now, &c, &b, custom_context);
    if (preview) { c = light_hue_rgb(selected_hue); b = max_level; }
    if (ws2812_busy()) return; /* Coalesce animation frames while a transfer owns its snapshot. */
    if (!sent_valid || b != sent_level || c.r != sent_color.r || c.g != sent_color.g || c.b != sent_color.b) {
        if (ws2812_show_solid(c, b)) { sent_level = b; sent_color = c; sent_valid = 1; }
    }
}
