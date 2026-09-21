from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
LANG = (ROOT / "firmware/src/15_language_strings.c").read_text(encoding="utf-8")
STATE = (ROOT / "firmware/src/07_state_machine.c").read_text(encoding="utf-8")
START = (ROOT / "firmware/src/01_startup.c").read_text(encoding="utf-8")
STRPOOL = (ROOT / "firmware/src/strpool.c").read_text(encoding="utf-8")

entries = re.findall(r'\{(?:0x[0-9a-fA-F]+|UI_TEXT_[A-Z_]+),"([^"]*)"\}', LANG)
assert entries and all(len(text) <= 16 for text in entries), "English LCD text exceeds 16 columns"
original_glyphs = set("0123456789%:. +IVUAWSBP-DCRc,M*FHTEN")
extended_glyphs = set("OX/GJKLQY")
used_glyphs = set("".join(entries))
assert used_glyphs <= original_glyphs | extended_glyphs, \
    f"English LCD text uses unsupported glyphs: {sorted(used_glyphs - original_glyphs - extended_glyphs)}"
assert 'ext_char8_map[] = "/GJKLQY"' in (ROOT / "firmware/src/02_lcd_display.c").read_text(encoding="utf-8")
assert '"INPUT:        %"' in LANG and '"OUTPUT:       V"' in LANG and '"CURRENT:      A"' in LANG
assert LANG.count('" PASS: ------"') == 2
for address in (0x0710, 0x4334, 0x6A8C, 0x6A98, 0xA070, 0xA0B0):
    assert f"0x{address:04x}" in LANG.lower(), f"12p-only text 0x{address:04x} is untranslated"
assert "ui_language_load();" in START
assert "EEPROM_UI_LANGUAGE = 0xff" in (ROOT / "firmware/inc/firmware_language.h").read_text(encoding="utf-8")
assert "*m2 > 9" in STATE and "*MENU == 0x0d" in STATE
assert "ui_language_translate(canonical, mapped)" in STRPOOL
print(f"BILINGUAL_UI_12: PASS translations={len(entries)} max_width={max(map(len, entries))}")
