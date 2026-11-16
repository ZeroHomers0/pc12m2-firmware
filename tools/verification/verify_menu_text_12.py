"""菜单定制执行验证：原 BIN 对照、字符覆盖、下标及列位置。"""
import struct
import verify_firmware_equivalence_12 as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_PC, UC_ARM_REG_LR

EXPECTED = {0x6b78: "9.αmin限制:   度", 0x6c04: "16αmax限制:   度",
            0x56c4: "11αmax限制:   度", 0x6b8c: "10主从移相:   度",
            0x6a80: "同步校准", 0x78ac: " 远程", 0x78bc: " 面板", 0x78b4: " 本地"}

EXPECTED.update({23348: ' 模选', 23356: ' 解限', 30920: ' 瞬动', 30928: ' 保持', 24592: '6.同步参数校准  ', 24652: '9.手动均流      ', 41072: '9.手动均流    ', 27404: '参数设置'})

def render(new, addr, invert=0):
    uc = V.machine(new)
    glyphs, data = [], []
    cmd = V.SYMS["disp_cmd"] if new else 0x8fa
    dat = V.SYMS["disp_data"] if new else 0x8a4
    delay = V.SYMS["Delay"] if new else 0x758
    ch8 = V.SYMS["disp_render_char8"] if new else 0xaf4
    ch16 = V.SYMS["disp_render_char16"] if new else 0xbea
    def hook(m, pc, size, user):
        if pc in (cmd, dat, delay):
            if pc == dat: data.append(m.reg_read(UC_ARM_REG_R0) & 255)
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
        elif pc == ch8:
            glyphs.append((m.reg_read(UC_ARM_REG_R0), m.reg_read(UC_ARM_REG_R2), len(data), 16))
        elif pc == ch16:
            glyphs.append(((m.reg_read(UC_ARM_REG_R0)<<8)|m.reg_read(UC_ARM_REG_R1), m.reg_read(UC_ARM_REG_R3), len(data), 32))
    uc.hook_add(UC_HOOK_CODE, hook)
    for reg, val in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), (addr,0,0,invert)):
        uc.reg_write(reg,val)
    V.run(uc, V.SYMS["disp_string"] if new else 0xcec)
    assert uc.reg_read(UC_ARM_REG_PC) == 0x3ff00
    return glyphs, data

def main():
    for addr, text in EXPECTED.items():
        uc=V.machine(True);uc.reg_write(UC_ARM_REG_R0,addr)
        V.run(uc,V.SYMS["strpool_map"])
        mapped=uc.reg_read(UC_ARM_REG_R0)
        assert bytes(uc.mem_read(mapped,len(text.encode("gbk"))+1)) == text.encode("gbk")+b"\0"
        for invert in (0,1):
            old, old_data=render(False,addr,invert)
            new, data=render(True,addr,invert)
            assert sum(g[3] for g in new)==len(data), (hex(addr),"missing glyph")
            if addr in (0x6b78,0x6c04,0x56c4):
                assert old[-1][1]==new[-1][1]==14
                alpha = new[2:4]
                assert [g[0] for g in alpha] == [0x80,0x81]
                assert all(any(data[g[2]+page*8:g[2]+page*8+8]) for g in alpha for page in (0,1))
                # α 必须在首行、末行均有像素，不能只拉高半角字形。
                assert any(v & 1 for g in alpha for v in data[g[2]:g[2]+8])
                assert any(v & 128 for g in alpha for v in data[g[2]+8:g[2]+16])
                for code,col,start,size in new:
                    if code in (0x82,0x83,0x84,0x85):
                        assert not any(data[start:start+8]), "subscript must be below row 8"
                        assert any(data[start+8:start+16])
            if addr==0x78b4: assert (old,old_data)==(new,data)
    print("MENU_TEXT: PASS 16 labels x normal/inverse; no missing glyphs; subscript and unit columns verified")

if __name__=="__main__": main()
