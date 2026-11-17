from common import *
from unicorn.arm_const import UC_ARM_REG_PC
select('current')
for name in ('uart3_tx_byte','uart3_rx_timeout_monitor','load_config','param_sync_live_to_eeprom'):
 for ready in (False,True):
  for new in (False,True):
   u=V.machine(new)
   if ready:u.mem_write(0x4009c014,b'\x20\0\0\0')
   V.run(u,V.SYMS[name] if new else V.PAIRS[name][0],max_insn=500000)
   print(name,'ready',ready,'new',new,'pc',hex(u.reg_read(UC_ARM_REG_PC)),'returned',u.reg_read(UC_ARM_REG_PC)==V.STOP,flush=True)
