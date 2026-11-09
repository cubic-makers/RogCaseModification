#include "light.h"
#include "app_config.h"
#include <string.h>
static light_mode_t mode;
static uint16_t selected_hue, duration;
static uint8_t level, max_level, from, target, preview, sent_valid;
static uint8_t pixel_levels[APP_LED_COUNT];
/* Only this RGB snapshot is read by DMA callbacks. Rebuild it exclusively when idle. */
static rgb_t tx_pixels[APP_LED_COUNT];
static uint16_t group_offsets[APP_LIGHT_GROUP_COUNT + 1U];
static uint8_t groups_valid;
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
int light_init(uint16_t hue, uint8_t brightness) {
    uint8_t i; uint32_t total = 0;
    selected_hue = hue % 360U; max_level = brightness; level = preview = sent_valid = 0;
    mode = LIGHT_OFF; custom = 0; last_frame = 0; seen_errors = ws2812_errors();
    groups_valid = 1;
    memset(pixel_levels, 0, sizeof(pixel_levels)); memset(tx_pixels, 0, sizeof(tx_pixels));
    group_offsets[0] = 0;
    for (i = 0; i < APP_LIGHT_GROUP_COUNT; ++i) {
        if (!light_group_sizes[i]) groups_valid = 0;
        total += light_group_sizes[i]; group_offsets[i + 1U] = (uint16_t)total;
    }
    groups_valid = groups_valid && total == APP_LED_COUNT;
    return groups_valid;
}
static void advance(uint32_t now) {
    if (mode == LIGHT_FADE_IN || mode == LIGHT_FADE_OUT) {
        uint32_t e = now - started;
        if (!duration || e >= duration) { level = target; mode = target ? LIGHT_ON : LIGHT_OFF; }
        else level = (uint8_t)((int32_t)from + ((int32_t)target - from) * (int32_t)e / duration);
        memset(pixel_levels, level, sizeof(pixel_levels));
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
    custom = fn; custom_context = ctx;
    mode = fn ? LIGHT_CUSTOM : LIGHT_PIXELS;
}
light_mode_t light_mode(void) { return mode; }
uint8_t light_brightness_limit(void) { return max_level; }
int light_set_group_brightness(uint8_t id, uint8_t brightness) {
    uint16_t p;
    if (!groups_valid || !id || id > APP_LIGHT_GROUP_COUNT) return 0;
    custom = 0; mode = LIGHT_PIXELS;
    for (p = group_offsets[id - 1U]; p < group_offsets[id]; ++p) pixel_levels[p] = brightness;
    return 1;
}
void light_set_timeline_state(int target_open, int busy) {
    mode = busy ? (target_open ? LIGHT_SEQUENCE_ON : LIGHT_SEQUENCE_OFF) : (target_open ? LIGHT_ON : LIGHT_OFF);
}
int light_set_pixel_brightness(uint16_t index, uint8_t brightness) {
    if (index >= APP_LED_COUNT) return 0;
    custom = 0; mode = LIGHT_PIXELS;
    pixel_levels[index] = brightness; return 1;
}
uint8_t light_get_pixel_brightness(uint16_t index) { return index < APP_LED_COUNT ? pixel_levels[index] : 0; }
static uint8_t component(uint8_t value, uint8_t brightness) { return (uint8_t)(((uint32_t)value * brightness + 127U) / 255U); }
void light_poll(uint32_t now) {
    rgb_t c; uint8_t b, custom_level = 0; uint16_t i; int changed;
    advance(now);
    if (seen_errors != ws2812_errors()) { seen_errors = ws2812_errors(); sent_valid = 0; }
    if (now - last_frame < APP_LIGHT_FRAME_MS) return;
    last_frame = now; c = light_hue_rgb(selected_hue);
    if (mode == LIGHT_CUSTOM && custom) custom(now, &c, &custom_level, custom_context);
    if (ws2812_busy()) return; /* Coalesce animation frames while a transfer owns its snapshot. */
    changed = !sent_valid;
    for (i = 0; i < APP_LED_COUNT; ++i) {
        rgb_t next;
        b = (mode == LIGHT_CUSTOM && custom) ? custom_level : pixel_levels[i];
        if (preview) c = light_hue_rgb(selected_hue); /* Hue only; never override timeline brightness. */
        next.r = component(c.r, b); next.g = component(c.g, b); next.b = component(c.b, b);
        if (next.r != tx_pixels[i].r || next.g != tx_pixels[i].g || next.b != tx_pixels[i].b) changed = 1;
        tx_pixels[i] = next;
    }
    if (changed) {
        sent_valid = ws2812_show_pixels(tx_pixels, APP_LED_COUNT, 255) != 0;
    }
}
