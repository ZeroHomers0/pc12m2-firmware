"""最终候选/函数台账：不把数据、适配或不可达出口伪装为动态通过。"""
import sys,json,re,struct,hashlib,collections
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation')]
from software_ab_support import *
from test_main_software_chain import old_map,DISPLAY12
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
from capstone.arm import ARM_OP_MEM,ARM_REG_PC

def main():
    p=ROOT/'tmp/completion_closed_inventory.json';j=json.loads(p.read_text(encoding='utf-8'))
    assert j['batch_exit_code']==0 and j['firmware_sha256']==hashlib.sha256(V.NEW).hexdigest()
    assert j['original_sha256']==hashlib.sha256(V.ORIGINAL).hexdigest()
    guard=0xe0d0 if IS12 else 0xe3aa
    data=(0x112fa,0x112fc) if IS12 else (0x11156,0x11158)
    hi_base=0x112d8 if IS12 else 0x11134;lo_base=hi_base-256
    table=[]
    for n in range(256):
        value=n
        for _ in range(8):value=(value>>1)^0xa001 if value&1 else value>>1
        table.append(value)
    assert V.ORIGINAL[lo_base:lo_base+256]==bytes(v&255 for v in table)
    assert V.ORIGINAL[hi_base:hi_base+256]==bytes(v>>8 for v in table)
    cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);cs.detail=True
    crc=old_map()['crc16'];refs=[]
    for ins in cs.disasm(V.ORIGINAL[crc:crc+0x30],crc):
        for op in ins.operands:
            if op.type==ARM_OP_MEM and op.mem.base==ARM_REG_PC:
                literal=((ins.address+4)&~3)+op.mem.disp
                value=struct.unpack_from('<I',V.ORIGINAL,literal)[0]
                if value in (lo_base,hi_base):refs.append({'instruction':hex(ins.address),'literal':hex(literal),'table':hex(value)})
    assert len(refs)==2,'原CRC两张表指针证据不完整'
    rows=[]
    for address,b in j['branches'].items():
        a=int(address,16);both=all(b['outcomes'][k] for k in ('taken','fallthrough'))
        assert b['outcomes']['other']==0
        if both:
            status='BOTH_EXECUTED'
            if a==0x12a:status='BOTH_EXECUTED_ORIGINAL_RUNTIME_ADAPTATION'
            if a==(0x448a if IS12 else 0x45b2):status='BOTH_EXECUTED_AUTHORIZED_BACKLIGHT_CUSTOM'
            proof='当前固件完整回归及completion增量链；运行库/背光分别按适配或定制契约，非PASS_EQ。'
        elif a in data:
            assert hi_base<=a<hi_base+256 and not any(b['outcomes'].values())
            status='DATA_MISDISASSEMBLED_NOT_EXECUTABLE_BRANCH'
            proof='位于完整256字节标准CRC高字节表；原crc16两处LDR literal指向该表。候选来自反汇编把常量表误当指令。'
        elif a==guard:
            assert b['outcomes']['fallthrough'] and not b['outcomes']['taken']
            status='UNREACHABLE_PROVEN_IN_READY_FRAME_CONTRACT'
            proof=('到达读尾前功能码只可能3/6/16；非法码与CRC失败已返回，6/16处理块无论成功/错误都返回，'
                   '故读尾仅剩3。帧态5关闭RX接收；真实UART RX ISR全256字节连续输入不能改已完成帧。'
                   'THRE发送恢复在应答之后；本契约不允许调试器/DMA任意改RAM或伪造已禁用的发送中断。')
        else:raise AssertionError(('仍有未核销候选',address,b))
        rows.append({'address':address,**b,'status':status,'proof':proof})
    branches={'project':ROOT.name,'inventory_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
              'raw_candidate_summary':j['summary'],'classification':dict(collections.Counter(r['status'] for r in rows)),
              'pending_unexplained':0,'CRC_table_evidence':{'low':hex(lo_base),'high':hex(hi_base),'literal_reads':refs},'entries':rows}
    (ROOT/'tmp/completion_branch_acceptance.json').write_text(json.dumps(branches,ensure_ascii=False,indent=2),encoding='utf-8')
    lookup=old_map()
    if IS12:
        lookup.update({n:a for a,n in DISPLAY12.items()})
        lookup.update({'main':0x5cc,'lcd_ctrl_line':0x766,'disp_data':0x8a4,'disp_cmd':0x8fa,
                       'disp_render_char16':0xbea,'disp_digit':0xd44,'disp_decimal1':0x14a6,'freq_adjust_sync':0xa856})
    mapping={a:n for n,a in lookup.items()}
    runtime={0xcc,0x180,0x184,0x188,0x19a,0x1c4,0x2ee,
             *( (0x11178,0x111c4,0x111cc) if IS12 else (0x10fd4,0x11020,0x11028))}
    source=ROOT/('tools/_ghidra_proj/_pc12m2_functions.txt' if IS12 else 'evidence/reverse/reports/_all_functions.txt')
    functions=[]
    for line in source.read_text(encoding='utf-8').splitlines():
        m=re.match(r'([0-9a-fA-F]+).*body=(\d+)',line)
        if not m:continue
        a=int(m[1],16)
        if a in runtime:status='IAR_RUNTIME_REPLACED_BOOT_ABI_CONTRACT';name='原IAR运行库/初始化/退出'
        elif a==0x5ca:status='ORIGINAL_BX_LR_OPTIMIZED_AWAY';name='startup_idle_hook'
        else:
            assert a in mapping,('原函数未分类',hex(a));name=mapping[a]
            assert name in V.SYMS,('新函数符号缺失且未证明内联',hex(a),name)
            status='SOURCE_SYMBOL_AND_EXECUTION_LAYER_CONTRACT'
        functions.append({'original_entry':hex(a),'original_bytes':int(m[2]),'name':name,'status':status,
                          'new_entry':hex(V.SYMS[name]) if name in V.SYMS else None})
    # 两份Ghidra普通函数表都漏掉8个向量ISR，必须从金标准向量表另行核对。
    interrupts={0:('WDT_IRQHandler',0x1e4),1:('TIMER0_IRQHandler',0x29a),
                2:('TIMER1_IRQHandler',0xfb0c if IS12 else 0xff6c),
                3:('TIMER2_IRQHandler',0xfae8 if IS12 else 0xff48),
                8:('UART3_IRQHandler',0xac78 if IS12 else 0xaf08),
                19:('EINT1_IRQHandler',0xf748 if IS12 else 0xf9e8),
                20:('EINT2_IRQHandler',0xf76a if IS12 else 0xfa0a),
                21:('EINT3_IRQHandler',0xf78c if IS12 else 0xfa2c)}
    seen={int(r['original_entry'],16) for r in functions}
    for irq,(name,a) in interrupts.items():
        assert struct.unpack_from('<I',V.ORIGINAL,4*(16+irq))[0]&~1==a,('原向量映射错误',irq,name)
        assert name in V.SYMS and struct.unpack_from('<I',V.NEW,4*(16+irq))[0]&~1==V.SYMS[name]
        if a not in seen:functions.append({'original_entry':hex(a),'original_bytes':None,'name':name,
            'status':'VECTOR_ISR_EXECUTION_AND_FINITE_MODEL_CONTRACT','new_entry':hex(V.SYMS[name]),'IRQ':irq})
    f={'project':ROOT.name,'source_list':str(source.relative_to(ROOT)),
       'counts':dict(collections.Counter(r['status'] for r in functions)),'unclassified':0,'entries':functions}
    (ROOT/'tmp/completion_function_contracts.json').write_text(json.dumps(f,ensure_ascii=False,indent=2),encoding='utf-8')
    print('ACCEPTANCE_LEDGER branches',branches['classification'],'pending=0 functions',len(functions),'unclassified=0',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
