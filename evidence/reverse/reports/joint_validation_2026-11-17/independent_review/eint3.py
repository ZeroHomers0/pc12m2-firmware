from common import *
import struct,json
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_PC
results={}
for label in ('working','before'):
 select(label);sample=V.machine(True)
 names=('phase_cnt','debounce_count','freq_hz','hold_count','cfg_word','input_locked','out_param')
 addresses={n:struct.unpack('<I',sample.mem_read(V.SYMS[n],4))[0] for n in names}
 print('ADDRESSES',addresses)
 records=[]
 for hz in (50,60):
  pair=[]
  for new in (False,True):
   u=V.machine(new)
   for n,value,width in [('phase_cnt',0,1),('debounce_count',9,1),('freq_hz',hz,1),('hold_count',4,4),('cfg_word',1,1),('input_locked',2,4),('out_param',0,4)]:u.mem_write(addresses[n],value.to_bytes(width,'little'))
   targets={V.SYMS['fio0_pin22_ctrl']:'P0.22',V.SYMS['fio1_pin22_ctrl']:'P1.22'} if new else {0xe6c6:'P0.22',0xe6a6:'P1.22'}
   calls=[];writes=[]
   def hook(m,a,n,x):
    if a in targets:calls.append(targets[a])
   u.hook_add(UC_HOOK_CODE,hook)
   def mmio(m,access,a,size,value,user):
    if a in (0x2009c01c,0x2009c03c):writes.append((hex(a),value))
   u.hook_add(UC_HOOK_MEM_WRITE,mmio,begin=0x2009c000,end=0x2009cfff)
   V.run(u,V.SYMS['EINT3_IRQHandler'] if new else 0xf78c,max_insn=100000)
   assert u.reg_read(UC_ARM_REG_PC)==V.STOP
   pair.append((writes,bytes(u.mem_read(0x10000000,0x2200))))
  assert [a for a,v in pair[0][0]]==['0x2009c01c','0x2009c03c'],pair[0][0]
  print(label,hz,'OLD',pair[0][0],'NEW',pair[1][0],'SRAM_MATCH',pair[0][1]==pair[1][1])
  records.append({'hz':hz,'old':pair[0][0],'new':pair[1][0],'sram_equal':pair[0][1]==pair[1][1]})
 results[label]=records
(AREA/'eint3_results.json').write_text(json.dumps(results,indent=2))
