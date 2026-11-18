"""端子事件契约与去抖边界：直接执行本仓原BIN与新ELF。"""
def main():
    import sys,struct,itertools
    from pathlib import Path
    root=Path(__file__).resolve().parents[2];is12=(root/'pc12m2_orig.bin').exists();sys.path.insert(0,str(root/'tools/verification'))
    V=__import__('verify_firmware_equivalence_12' if is12 else 'verify_firmware_equivalence')
    modeaddr=0x1000164e if is12 else 0x10001656
    old=0x1976 if is12 else 0x19c6
    count=bad=0;examples=[]
    for mode,pin,c1,c2,latch in itertools.product((0,1),(0,0x08000000,0x10000000,0x18000000),(0,48,49,50,51,254,255),(0,48,49,50,51,254,255),(0,7,8)):
     vals=[]
     for new in (False,True):
      u=V.machine(new);u.mem_write(modeaddr,bytes([mode]));u.mem_write(0x10001578,bytes([c1,c2,latch]));u.mem_write(0x2009c014,struct.pack('<I',pin));V.run(u,V.SYMS['scan_run_stop'] if new else old,max_insn=10000)
      assert u.reg_read(V.UC_ARM_REG_PC)==0x3ff00
      vals.append((u.reg_read(V.UC_ARM_REG_R0)&255,bytes(u.mem_read(0x10001578,3))))
     count+=1
     if vals[0]!=vals[1]:
      bad+=1
      if len(examples)<6:examples.append((mode,hex(pin),c1,c2,latch,vals))
    print(root.name,'SCAN_RUN_STOP',count,'mismatches',bad,'examples',examples)
    # 双沿去抖逐一验证，覆盖阈值前后及溢出。
    funcs=[('debounce_p09',0x1ab8,0x2009c014,0x200),('debounce_p116',0x1ae6,0x2009c034,0x10000),('debounce_p117',0x1b3e,0x2009c034,0x20000),('debounce_p06',0x1b96,0x2009c014,0x40)]
    n=err=0
    for name,entry,port,mask in funcs:
     if is12:entry=V.PAIRS[name][0]
     for level,seed in itertools.product((0,mask),(0,14,15,16,49,50,51,249,250,251,255)):
      pair=[]
      for new in (False,True):
       u=V.machine(new);u.mem_write(0x10001578,bytes([seed])*12);u.mem_write(port,struct.pack('<I',level));V.run(u,V.SYMS[name] if new else entry,max_insn=10000);pair.append((u.reg_read(V.UC_ARM_REG_R0)&255,bytes(u.mem_read(0x10001578,12))))
      n+=1;err+=pair[0]!=pair[1]
    print(root.name,'DEBOUNCE',n,'mismatches',err)

    assert bad == 0, "terminal scan differs from original BIN"
    assert err == 0, "debounce differs from original BIN"
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
