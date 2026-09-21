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
assert re.search(r'menu_language\[\].*\\x31\\x30\\x2e\\xd3.*\\xf1 {5}"',
                 (ROOT / "firmware/src/strpool.c").read_text(encoding="utf-8")), \
    "Chinese language menu must align GBK glyphs and erase the whole row"
display_source = (ROOT / "firmware/src/02_lcd_display.c").read_text(encoding="utf-8")
for gbk_pair in ("{0xd3,0xef}", "{0xd1,0xd4}", "{0xd1,0xa1}", "{0xd4,0xf1}", "{0xce,0xc4}"):
    assert gbk_pair in display_source, f"missing language UI glyph: {gbk_pair}"
assert "gbase + tbl_idx * 0x20" in display_source
assert display_source.count("if (col == 0xb)") >= 4, "variable-width numeric fields must clear through column 15"
for address in (0x6018, 0x6020, 0x6028, 0x6030, 0x6038, 0x6040, 0x6048, 0x6050,
                0x6058, 0x6060, 0x6594, 0x659c, 0x65a4, 0x6af8, 0x6b08, 0x6b14,
                0x6b24, 0x7998, 0x79a0, 0x79a8, 0x79b4, 0x79bc):
    assert len(entry_map[address]) == 5, f"value at LCD column 11 must repaint all 5 columns: {address:#x}"
for address in (0x47dc, 0x47e8, 0x47f0, 0x8f6c, 0x8f78, 0x8f88, 0x8f9c):
    assert len(entry_map[address]) == 6, f"status at LCD column 10 must repaint all 6 columns: {address:#x}"
for address in (0x6554, 0x6568, 0x657c, 0x65bc, 0x65d0, 0x65e4, 0x65f8,
                0x6fe4, 0x6ff8, 0x700c, 0x7020, 0x7e10, 0x7e24, 0x7e38,
                0x7e4c, 0x7e74):
    assert len(entry_map[address]) == 16 and entry_map[address][11:15] == "    ", \
        f"numeric field at columns 11..14 is not reserved: {address:#x}"
for address in (0x0710, 0x4334, 0x6A8C, 0x6A98, 0xA070, 0xA0B0):
    assert f"0x{address:04x}" in LANG.lower(), f"12p-only text 0x{address:04x} is untranslated"
# Automatically cover every translated literal used as a right-side field.
# Such fields must reach column 15 so shorter values cannot leave stale pixels
# or stale inverse-video spaces behind.
canonical_map = {int(raw, 16): int(canonical, 16) for raw, canonical in re.findall(
    r'if \(addr == 0x([0-9a-fA-F]+)u\) canonical = 0x([0-9a-fA-F]+)u;', STRPOOL)}
for source_path in (ROOT / "firmware/src").glob("*.c"):
    source = source_path.read_text(encoding="utf-8", errors="ignore")
    for address_text, col_text in re.findall(
            r'disp_string\(\s*(0x[0-9a-fA-F]+),\s*[^,]+,\s*(0x[0-9a-fA-F]+|\d+)', source):
        raw_address = int(address_text, 16)
        address = canonical_map.get(raw_address, raw_address)
        col = int(col_text, 0)
        if address in entry_map and col >= 10:
            assert col + len(entry_map[address]) == 16, \
                f"right-side field must repaint through column 15: {source_path.name} {address_text} col={col} text={entry_map[address]!r}"
assert "ui_language_load();" in START
assert "disp_string(0xa0b0, 0, 2, 0)" in STATE
assert "sm4_draw_page(0);" in STATE
assert "if (addr == 0x7488u) canonical = 0x7974u;" in STRPOOL
assert "if (addr == 0x7490u) canonical = 0x7980u;" in STRPOOL
assert "if (addr == 0x8638u) canonical = 0x86e0u;" in STRPOOL
assert "EEPROM_UI_LANGUAGE = 0xff" in (ROOT / "firmware/inc/firmware_language.h").read_text(encoding="utf-8")
assert "*m2 > 9" in STATE and "*MENU == 0x0d" in STATE
assert "ui_language_translate(canonical, mapped)" in STRPOOL
print(f"BILINGUAL_UI_12: PASS translations={len(entries)} max_width={max(map(len, entries))}")
