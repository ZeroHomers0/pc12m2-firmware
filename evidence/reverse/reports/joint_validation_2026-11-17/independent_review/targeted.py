from common import *
import struct,json,itertools
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_LR,UC_ARM_REG_PC

def call(u,entry,key=0):
 u.reg_write(UC_ARM_REG_R0,key);V.run(u,entry,max_insn=2000000)
 assert u.reg_read(UC_ARM_REG_PC)==V.STOP,'未完成执行'
def crc(b):
 v=65535
 for x in b:
  v^=x
  for _ in range(8):v=(v>>1)^0xa001 if v&1 else v>>1
 return v
old_funcs={0xCEC:'disp_string',0xD44:'disp_digit',0xDF2:'disp_number3',0xE80:'disp_uint4',0xF3C:'disp_uint5',0x1042:'disp_number',0x116C:'disp_signed_angle',0x1260:'disp_offset',0x1398:'disp_uint2',0x13EC:'disp_fixed_1dec',0x14A6:'disp_decimal1',0x942:'disp_clear',0x40B0:'disp_splash_screen'}

def state(new,menu,item,edit,key,changes,timer=0,stats=250):
 u=V.machine(new);V._seed_state_machine(u,menu,item,edit)
 u.mem_write(0x10001744,struct.pack('<I',timer));u.mem_write(0x10001758,struct.pack('<I',stats))
 for a,b in changes:u.mem_write(a,b)
 for a in (0x2009c014,0x2009c034):u.mem_write(a,b'\xff'*4)
 table={V.SYMS[n]:n for n in old_funcs.values()} if new else old_funcs
 seq=[];skip={V.SYMS['Delay'],V.SYMS['param_sync_live_to_eeprom']} if new else {0x758,0x3534}
 def hook(m,a,n,x):
  if a in table:
   name=table[a];args=() if name in ('disp_clear','disp_splash_screen') else tuple(m.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3))
   seq.append((name,)+args)
  if a in table or a in skip:m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
 u.hook_add(UC_HOOK_CODE,hook);call(u,V.SYMS['state_machine'] if new else 0x4464,key)
 return seq,bytes(u.mem_read(0x10000000,0x2200))

def protocol(new,fc,badcrc=False):
 u=V.machine(new);body=bytes((1,fc,16,1,0,1));c=crc(body)^int(badcrc);frame=body+struct.pack('<H',c)
 for a,b in ((0x100016f7,b'\1'),(0x10001770,b'\5'),(0x10001772,bytes((len(frame),))),(0x10002278,frame)):u.mem_write(a,b)
 sent=[];tx=V.SYMS['uart3_tx_byte'] if new else 0xab7c
 def hook(m,a,n,x):
  if a==tx:
   sent.append(bytes(m.mem_read(0x10002340,m.reg_read(UC_ARM_REG_R0))));m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
 u.hook_add(UC_HOOK_CODE,hook);call(u,V.SYMS['modbus_dispatch'] if new else 0xb3b2)
 return sent

def monitor(new,flag,timer):
 u=V.machine(new)
 for a,b in ((0x100016fd,bytes((flag,))),(0x1000176c,struct.pack('<I',timer)),(0x10001798,b'\0'*4),(0x1000161c,b'\0'*4),(0x10001770,b'\0')):u.mem_write(a,b)
 call(u,V.SYMS['uart3_rx_timeout_monitor'] if new else 0xabc0)
 return bytes(u.mem_read(0x10000000,0x2200))

cases=[]
for val in (0,1,2,3,128,255):
 for addr in (0x1000162c,0x1000164d,0x1000164f):
  cases.append(('基本参数枚举',3,0,0,0,[(addr,bytes((val,)))],0,250))
 for addr in (0x1000164f,0x10001650):
  for item in (5,6):cases.append(('标定枚举',0x63,item,0,0,[(addr,bytes((val,)))],0,250))
 for edit in (0,1):
  for item in (0,2,3):
   for addr in (0x100016fc,0x100016fd):cases.append(('通信枚举',5,item,edit,0,[(addr,bytes((val,)))],0,250))
 cases.append(('通信入口',2,2,0,1,[(0x100016fd,bytes((val,)))],0,0))
for page,timer in itertools.product((0,0x64,0xff),(0,4998,4999,5000)):
 cases.append(('非法页超时',page,0,0,0,[],timer,0))
results={}
for label in ('working','before'):
 select(label);bad_protocol=[];bad_monitor=[];bad_state=[]
 for fc in range(256):
  if fc in (3,6,16):continue
  if protocol(False,fc)!=protocol(True,fc):bad_protocol.append(fc)
 for fc in (3,6,16):assert protocol(False,fc,True)==protocol(True,fc,True)
 for flag,timer in itertools.product(range(256),(0,30000,30001)):
  if monitor(False,flag,timer)!=monitor(True,flag,timer):bad_monitor.append((flag,timer))
 for i,case in enumerate(cases):
  group,*args=case;a=state(False,*args);b=state(True,*args)
  if a!=b:
   extra={'index':i,'group':group,'menu':args[0],'item':args[1],'edit':args[2],'seed':[(hex(x),y.hex()) for x,y in args[4]],'calls_old':a[0],'calls_new':b[0]}
   bad_state.append(extra)
 result={'protocol_total':256,'protocol_bad':bad_protocol,'monitor_total':768,'monitor_bad':bad_monitor,'state_total':len(cases),'state_bad':bad_state}
 results[label]=result
 print(label,'protocol_bad',len(bad_protocol),'monitor_bad',len(bad_monitor),'state_bad',len(bad_state),'of',len(cases),flush=True)
 if label=='working':print('STATE_EXAMPLES',json.dumps(bad_state[:5],ensure_ascii=False),flush=True)
(AREA/'targeted_results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
