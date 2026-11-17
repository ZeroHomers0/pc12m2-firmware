# -*- coding: utf-8 -*-
"""12p state machine full matrix (v4): display calls + pin/relay calls + SRAM end state.

History of this probe (each version fixed a defect that made the matrix blind):

  v1  probe_sm12_all.py          612/6732 "failures" were budget truncation;
                                 6732 cases all ran the SAME case (see v2 fix 1).
  v2  probe_sm12_matrix.py       compared SRAM only while SKIPPING every disp_*
                                 body -> blind to any display-only defect.
  v3  probe_sm12_matrix_v3.py    added GPIO MEM_WRITE trace -> **new artifact**:
                                 with a MEM_WRITE hook installed on 0x2009C000,
                                 the NEW fio1_pin23_ctrl body never runs at all
                                 (Unicorn TCG cache bug, seen earlier in 6M),
                                 so OLD showed 3 writes and NEW 2. Not a defect.
  v4  this file                  no MEM_WRITE hook anywhere. Records
                                 (a) every disp_* call with r0..r3,
                                 (b) every pin/relay/LED control call with r0,
                                 (c) SRAM + peripheral end state.
                                 All three are observed via UC_HOOK_CODE only,
                                 which is stable under fast-forward hooks.

Case-parameter fixes carried over from v2:
  1. probe_sm12_state.BASE has 0x10001724=30 / 0x10001725=20 / 0x10001726=10,
     which ARE MENU / MENU2(item) / MENU3(edit). Writing BASE after the case
     bytes overwrote every case with menu=0x1e item=0x14 view=0x0a. v4 drops
     those keys from BASE and writes the case last.
  2. password delay 10000x1000 -> fast-forward (probe_sm12_delay.install).
  3. EEPROM write-cycle delay FUN_00001c32 / i2c_delay 0x1CD0 -> pure spin,
     skipped.
  4. TIMEOUT3 regime 0xFA so the first tick increments to 0xFB and triggers the
     full-page redraw; 0x1F4 for the ">0x1F4 erase" path.

Usage: python tmp\\probe_sm12_matrix_v4.py [menus] [keys] [t3regimes]
       python tmp\\probe_sm12_matrix_v4.py 63        # single-screen smoke test
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import probe_sm12_delay as D  # noqa: E402
import probe_sm12_state as P  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,  # noqa: E402
                               UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)

MENUS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0x14, 0x1E, 0x5A, 0x62, 0x63)
ITEMS = tuple(range(0, 17))
KEYS = (0, 1, 2, 3, 4, 5, 6, 0x0B, 0x0E, 0x16, 0x21, 0x17)
VIEWS = (0, 1)
T3_REGIMES = (0xFA, 0x1F4)
VOLATILE = (0x1614, 0x161C)

CASE_BYTES = (0x10001724, 0x10001725, 0x10001726)
BASE_FIXED = {k: v for k, v in P.BASE.items() if k not in CASE_BYTES}

OLD_DISP = {
    0xCEC: "disp_string", 0xD44: "disp_digit", 0xDF2: "disp_number3",
    0xE80: "disp_uint4", 0xF3C: "disp_uint5", 0x1042: "disp_number",
    0x116C: "disp_signed_angle", 0x1260: "disp_offset", 0x1398: "disp_uint2",
    0x13EC: "disp_fixed_1dec", 0x14A6: "disp_decimal1", 0x942: "disp_clear",
    # 0x40B0 is splash (PAIRS). Earlier versions used 0x1486, which is a MID
    # address inside disp_fixed_1dec (0x13EC..0x14A2) -- so OLD never skipped
    # the splash body while NEW did, making key=4 look asymmetric.
    0x40B0: "disp_splash_screen",
}
NEW_DISP_NAMES = tuple(OLD_DISP.values())
ARGLESS = ("disp_clear", "disp_splash_screen")

# pin / relay / LED control: OLD entry -> name. These are the observable
# hardware side effects of the state machine, recorded at call level (r0 =
# level), which is robust and needs no MEM_WRITE hook.
OLD_PIN = {
    0x107E0: "out_relay_p021", 0x10800: "fio1_pin20_ctrl",
    0x10820: "fio1_pin21_ctrl", 0x10840: "fio1_pin23_ctrl",
    # FIO1 base 0x2009C020: SET +0x38 / CLR +0x3C -> P1.22
    0xE6A6: "fio1_pin22_ctrl",
    # FIO0 base 0x2009C000: SET +0x18 / CLR +0x1C -> P0.22
    0xE6C6: "fio0_pin22_ctrl",
    0xE4FA: "gpio_outputs_set",
}
NEW_PIN_NAMES = tuple(OLD_PIN.values())
PIN_ARGLESS = ("gpio_outputs_set",)

EEPROM_DELAY_OLD = 0x1C32
EEPROM_DELAY_NEW = 0x1CD0


def _skip(uc, site):
    def hook(m, a, s, u):
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE, hook, begin=site, end=site + 1)


def _disp_map(is_new):
    if not is_new:
        return dict(OLD_DISP)
    return {P.SYMS[n]: n for n in NEW_DISP_NAMES if n in P.SYMS}


def _pin_map(is_new):
    if not is_new:
        return dict(OLD_PIN), set()
    m = {P.SYMS[n]: n for n in NEW_PIN_NAMES if n in P.SYMS}
    # GCC split fio[01]_pin22_ctrl into a public wrapper plus a `.part.0`
    # body, and callers such as run_stop_preset emit `bl ...part.0` directly
    # (tail-branch on the level<1 path). Hooking only the public symbol misses
    # those calls, which made NEW look like it skipped run_stop_preset's two
    # pin22 writes.
    parts = set()
    for sym, name in (("fio1_pin22_ctrl.part.0", "fio1_pin22_ctrl"),
                      ("fio0_pin22_ctrl.part.0", "fio0_pin22_ctrl")):
        if sym in P.SYMS:
            parts.add(P.SYMS[sym])
            m[P.SYMS[sym]] = name
    return m, parts


def trace(is_new, menu, item, view, key, t3, max_insn=8_000_000):
    """Return (disp_calls, pin_calls, end_state). CODE hooks only."""
    uc = P.machine(is_new)

    disp = []
    for addr, name in _disp_map(is_new).items():
        def hook(m, a, s, u, nm=name):
            if nm in ARGLESS:
                disp.append(nm)
            else:
                disp.append((nm,
                             m.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF,
                             m.reg_read(UC_ARM_REG_R1) & 0xFF,
                             m.reg_read(UC_ARM_REG_R2) & 0xFF,
                             m.reg_read(UC_ARM_REG_R3) & 0xFF))
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
        uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr + 1)

    pins = []
    pin_map, pin_parts = _pin_map(is_new)
    for addr, name in pin_map.items():
        def hook(m, a, s, u, nm=name, ad=addr):
            if nm in PIN_ARGLESS:
                pins.append(nm)
            elif ad in pin_parts:
                # `.part.0` is the level<1 (clear-PIN) body: callers tail-branch
                # into it *after* deciding the level, so R0 is stale garbage
                # there (observed 17216) rather than the argument.
                pins.append((nm, 0))
            else:
                pins.append((nm, m.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF))
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
        uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr + 1)

    _skip(uc, EEPROM_DELAY_NEW if is_new else EEPROM_DELAY_OLD)
    D.install(uc, is_new)

    for address, value in BASE_FIXED.items():
        uc.mem_write(address, struct.pack("<I", value))
    uc.mem_write(0x10001724, bytes((menu, item, view)))
    uc.mem_write(0x10001744, struct.pack("<I", 0))     # TIMEOUT
    uc.mem_write(0x10001758, struct.pack("<I", t3))    # TIMEOUT3
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))

    state = P.SYMS["state_machine"] if is_new else 0x4464
    for _ in range(2):
        uc.reg_write(UC_ARM_REG_R0, key)
        uc.reg_write(UC_ARM_REG_LR, P.STOP + 1)
        P.run(uc, state, max_insn=max_insn)

    sram = bytearray(uc.mem_read(0x10000000, P.SRAM_END))
    sram[VOLATILE[0]:VOLATILE[1]] = b"\0" * (VOLATILE[1] - VOLATILE[0])
    for base, size in P.REGIONS:
        sram += bytes(uc.mem_read(base, size))
    return disp, pins, bytes(sram)


def _diff(a, b, label, menu, item, view, key, t3, extra=""):
    i = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]),
             min(len(a), len(b)))
    print("[DIFF-%s] menu=0x%X item=%d view=%d key=0x%X t3=%#x %sn=%d/%d at %d "
          "old=%r new=%r"
          % (label, menu, item, view, key, t3, extra, len(a), len(b), i,
             a[i] if i < len(a) else None, b[i] if i < len(b) else None))


def main():
    menus, keys, regimes = MENUS, KEYS, T3_REGIMES
    args = [a for a in sys.argv[1:] if a.strip()]
    if len(args) >= 1 and args[0] != "-":
        menus = tuple(int(x, 16) for x in args[0].split(",") if x.strip())
    if len(args) >= 2:
        keys = tuple(int(x, 16) for x in args[1].split(",") if x.strip())
    if len(args) >= 3:
        regimes = tuple(int(x, 16) for x in args[2].split(",") if x.strip())

    total = bad = err = 0
    n_disp = n_pin = 0
    for menu in menus:
        for item in ITEMS:
            for key in keys:
                for view in VIEWS:
                    for t3 in regimes:
                        try:
                            od, op, os_ = trace(False, menu, item, view, key, t3)
                            nd, np, ns = trace(True, menu, item, view, key, t3)
                        except Exception as exc:
                            err += 1
                            if err <= 8:
                                print("[ERR] menu=0x%X item=%d view=%d key=0x%X t3=%#x %s"
                                      % (menu, item, view, key, t3, str(exc)[:70]))
                            continue
                        total += 1
                        n_disp += len(od)
                        n_pin += len(op)
                        if od != nd:
                            bad += 1
                            if bad <= 12:
                                _diff(od, nd, "DISP", menu, item, view, key, t3)
                            continue
                        if op != np:
                            bad += 1
                            if bad <= 12:
                                _diff(op, np, "PIN", menu, item, view, key, t3)
                            continue
                        if os_ != ns:
                            bad += 1
                            if bad <= 12:
                                i = next(k for k in range(len(os_)) if os_[k] != ns[k])
                                print("[DIFF-SRAM] menu=0x%X item=%d view=%d key=0x%X t3=%#x "
                                      "at 0x%X old=0x%02X new=0x%02X"
                                      % (menu, item, view, key, t3, 0x10000000 + i,
                                         os_[i], ns[i]))
    print("12p state machine matrix v4: cases=%d bad=%d err=%d disp_calls=%d pin_calls=%d"
          % (total, bad, err, n_disp, n_pin))
    return 0 if bad == 0 and err == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
