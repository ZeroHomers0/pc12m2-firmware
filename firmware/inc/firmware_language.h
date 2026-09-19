#ifndef FIRMWARE_LANGUAGE_H
#define FIRMWARE_LANGUAGE_H

#include <stdint.h>

enum { UI_LANGUAGE_CHINESE = 0, UI_LANGUAGE_ENGLISH = 1 };
enum { EEPROM_UI_LANGUAGE = 0xff };

void ui_language_load(void);
void ui_language_save(uint8_t language);
uint8_t ui_language_get(void);
uint32_t ui_language_translate(uint32_t canonical_address, uint32_t chinese_address);

#define UI_TEXT_MENU_LANGUAGE 0xffff0001u
#define UI_TEXT_LANGUAGE_TITLE 0xffff0002u
#define UI_TEXT_LANGUAGE_ZH 0xffff0003u
#define UI_TEXT_LANGUAGE_EN 0xffff0004u

#endif
