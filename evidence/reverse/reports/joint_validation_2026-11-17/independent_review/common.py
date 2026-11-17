from pathlib import Path
import sys
ROOT=Path.cwd()
AREA=ROOT/'tmp/codex_review_2026-11-17'
sys.path.insert(0,str(ROOT/'tools/verification'))
import verify_firmware_equivalence_12 as V
def select(label):
 V.FW=AREA/label/'firmware'
 symbols=V.symbols();V.SYMS.clear();V.SYMS.update(symbols);V.SECS=V.sections();V.NEW=(V.FW/'firmware.bin').read_bytes()
