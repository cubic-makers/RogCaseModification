#include "settings.h"
#include "stm32f1xx_hal.h"
#include <string.h>

#define SETTINGS_MAGIC   0x434D5631UL
#define SETTINGS_VERSION 1U
typedef struct {
    uint32_t magic;
    uint16_t version, length;
    settings_t value;
    uint32_t crc;
} flash_record_t;
typedef char record_fits_page[(sizeof(flash_record_t) <= APP_SETTINGS_PAGE_BYTES) ? 1 : -1];
typedef char record_halfword_aligned[(sizeof(flash_record_t) % 2U == 0) ? 1 : -1];

uint32_t settings_crc32(const void *data, uint32_t size) {
    const uint8_t *p = data; uint32_t crc = 0xFFFFFFFFUL, bit;
    while (size--) { crc ^= *p++; for (bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U))); }
    return ~crc;
}
void settings_defaults(settings_t *v) {
    unsigned i; memset(v, 0, sizeof(*v));
    for (i = 0; i < APP_SERVO_COUNT; ++i) {
        /* User-requested 180-degree servo travel test endpoints. */
        v->close_us[i] = 2500; v->open_us[i] = 500;
        v->move_ms[i] = 800; v->start_delay_ms[i] = (uint16_t)(i * 30U);
    }
    v->hue = 0; v->fade_ms = 800; v->brightness = 64; v->active_mask = APP_SERVO_ACTIVE_MASK;
}
int settings_valid(const settings_t *v) {
    unsigned i;
    if (!v || v->hue >= 360 || v->fade_ms < 20 || v->fade_ms > 5000 || !v->active_mask || v->reserved) return 0;
    for (i = 0; i < APP_SERVO_COUNT; ++i)
        if (v->close_us[i] < APP_SERVO_MIN_US || v->close_us[i] > APP_SERVO_MAX_US ||
            v->open_us[i] < APP_SERVO_MIN_US || v->open_us[i] > APP_SERVO_MAX_US ||
            v->move_ms[i] < 20 || v->move_ms[i] > 10000 || v->start_delay_ms[i] > 5000) return 0;
    return 1;
}
static int record_valid(const flash_record_t *r) {
    return r->magic == SETTINGS_MAGIC && r->version == SETTINGS_VERSION && r->length == sizeof(settings_t) &&
           r->crc == settings_crc32(&r->version, sizeof(*r) - sizeof(r->magic) - sizeof(r->crc)) && settings_valid(&r->value);
}
int settings_load(settings_t *v) {
    flash_record_t r;
    if (!v) return 0;
    memcpy(&r, (const void *)APP_SETTINGS_ADDRESS, sizeof(r));
    if (record_valid(&r)) { *v = r.value; return 1; }
    settings_defaults(v); return 0;
}
int settings_save(const settings_t *v) {
    flash_record_t r; FLASH_EraseInitTypeDef erase = {0}; uint32_t error, offset; int ok = 1;
    if (!settings_valid(v)) return 0;
    /* Unchanged valid records need no erase/program cycle. */
    {
        const flash_record_t *old = (const flash_record_t *)APP_SETTINGS_ADDRESS;
        if (record_valid(old) && memcmp(&old->value, v, sizeof(*v)) == 0) return 1;
    }
    memset(&r, 0, sizeof(r)); r.magic = SETTINGS_MAGIC; r.version = SETTINGS_VERSION; r.length = sizeof(*v); r.value = *v;
    r.crc = settings_crc32(&r.version, sizeof(r) - sizeof(r.magic) - sizeof(r.crc));
    if (HAL_FLASH_Unlock() != HAL_OK) return 0;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    erase.TypeErase = FLASH_TYPEERASE_PAGES; erase.PageAddress = APP_SETTINGS_ADDRESS; erase.NbPages = 1;
    if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) ok = 0;
    /* Write body/CRC first, magic last: interrupted writes fail validation. */
    for (offset = 4; ok && offset < sizeof(r); offset += 2) {
        uint16_t half; memcpy(&half, (const uint8_t *)&r + offset, 2);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, APP_SETTINGS_ADDRESS + offset, half) != HAL_OK) ok = 0;
    }
    for (offset = 0; ok && offset < 4; offset += 2) {
        uint16_t half; memcpy(&half, (const uint8_t *)&r + offset, 2);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, APP_SETTINGS_ADDRESS + offset, half) != HAL_OK) ok = 0;
    }
    if (HAL_FLASH_Lock() != HAL_OK) ok = 0;
    return ok && record_valid((const flash_record_t *)APP_SETTINGS_ADDRESS);
}
