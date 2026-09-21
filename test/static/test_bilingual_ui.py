from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
LANG = (ROOT / "firmware/src/15_language_strings.c").read_text(encoding="utf-8")
STATE = (ROOT / "firmware/src/07_state_machine.c").read_text(encoding="utf-8")
START = (ROOT / "firmware/src/01_startup.c").read_text(encoding="utf-8")
STRPOOL = (ROOT / "firmware/src/strpool.c").read_text(encoding="utf-8")

entries = re.findall(r'\{(?:0x[0-9a-fA-F]+|UI_TEXT_[A-Z_]+),"([^"]*)"\}', LANG)
entry_map = {int(address, 16): text for address, text in re.findall(r'\{0x([0-9a-fA-F]+),"([^"]*)"\}', LANG)}
assert entries and all(len(text) <= 16 for text in entries), "English LCD text exceeds 16 columns"
original_glyphs = set("0123456789%:. +IVUAWSBP-DCRc,M*FHTEN")
extended_glyphs = set("OX/GJKLQY")
used_glyphs = set("".join(entries))
assert used_glyphs <= original_glyphs | extended_glyphs, \
    f"English LCD text uses unsupported glyphs: {sorted(used_glyphs - original_glyphs - extended_glyphs)}"
assert 'ext_char8_map[] = "/GJKLQY"' in (ROOT / "firmware/src/02_lcd_display.c").read_text(encoding="utf-8")
assert '"INPUT:        %"' in LANG and '"OUTPUT:       V"' in LANG and '"CURRENT:      A"' in LANG
assert LANG.count('" PASS: ------"') == 2
assert 'UI_TEXT_MENU_LANGUAGE,"10.LANGUAGE     "' in LANG, "language menu must erase all 16 columns"
for address in (0x4814, 0x4824, 0x4834, 0x4844, 0x6488, 0x649c, 0x64b0, 0x64c4,
                0x64d8, 0x64ec, 0x6500, 0x6514, 0x6528):
    assert len(entry_map[address]) == 16, f"menu row must erase all columns: {address:#x}"
assert re.search(r'menu_language\[\].*\\x2e\\x20.*\\xf1 {4}"',
                 (ROOT / "firmware/src/strpool.c").read_text(encoding="utf-8")), \
    "Chinese language menu must align GBK glyphs and erase the whole row"
display_source = (ROOT / "firmware/src/02_lcd_display.c").read_text(encoding="utf-8")
for gbk_pair in ("{0xd3,0xef}", "{0xd1,0xd4}", "{0xd1,0xa1}", "{0xd4,0xf1}", "{0xce,0xc4}"):
    assert gbk_pair in display_source, f"missing language UI glyph: {gbk_pair}"
assert "gbase + tbl_idx * 0x20" in display_source
for address in (0x6018, 0x6020, 0x6028, 0x6030, 0x6038, 0x6040, 0x6048, 0x6050,
                0x6058, 0x6060, 0x6594, 0x659c, 0x65a4, 0x6af8, 0x6b08, 0x6b14,
                0x6b24, 0x7998, 0x79a0, 0x79a8, 0x79b4, 0x79bc):
    assert len(entry_map[address]) <= 5, f"value at LCD column 11 overflows: {address:#x}"
for address in (0x47dc, 0x47e8, 0x47f0, 0x8f6c, 0x8f78, 0x8f88, 0x8f9c):
    assert len(entry_map[address]) <= 6, f"status at LCD column 10 overflows: {address:#x}"
for address in (0x6554, 0x6568, 0x657c, 0x65bc, 0x65d0, 0x65e4, 0x65f8,
                0x6fe4, 0x6ff8, 0x700c, 0x7020, 0x7e10, 0x7e24, 0x7e38,
                0x7e4c, 0x7e74):
    assert len(entry_map[address]) == 16 and entry_map[address][11:15] == "    ", \
        f"numeric field at columns 11..14 is not reserved: {address:#x}"
for address in (0x0710, 0x4334, 0x6A8C, 0x6A98, 0xA070, 0xA0B0):
    assert f"0x{address:04x}" in LANG.lower(), f"12p-only text 0x{address:04x} is untranslated"
assert "ui_language_load();" in START
assert "EEPROM_UI_LANGUAGE = 0xff" in (ROOT / "firmware/inc/firmware_language.h").read_text(encoding="utf-8")
assert "*m2 > 9" in STATE and "*MENU == 0x0d" in STATE
assert "ui_language_translate(canonical, mapped)" in STRPOOL
print(f"BILINGUAL_UI_12: PASS translations={len(entries)} max_width={max(map(len, entries))}")
