#include "inc/types.h"
#include "inc/firmware_language.h"

void i2c_read_reg(undefined1 *out_buf, undefined4 reg_addr);
void i2c_write_reg(uint32_t data, uint32_t reg_addr);

static uint8_t current_language;

void ui_language_load(void)
{
  uint8_t stored = 0xff;
  i2c_read_reg(&stored, EEPROM_UI_LANGUAGE);
  current_language = (stored == UI_LANGUAGE_ENGLISH) ? UI_LANGUAGE_ENGLISH : UI_LANGUAGE_CHINESE;
}

void ui_language_save(uint8_t language)
{
  current_language = (language == UI_LANGUAGE_ENGLISH) ? UI_LANGUAGE_ENGLISH : UI_LANGUAGE_CHINESE;
  i2c_write_reg(current_language, EEPROM_UI_LANGUAGE);
}

uint8_t ui_language_get(void)
{
  return current_language;
}
