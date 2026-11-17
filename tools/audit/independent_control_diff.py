"""独立复核探针：固定种子、正常返回、完整业务 SRAM/外设末态。"""
import sys, random, struct, json
from pathlib import Path
root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools/verification'))
is12 = (root / 'pc12m2_orig.bin').exists()
V = __import__('verify_firmware_equivalence_12' if is12 else 'verify_firmware_equivalence')
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3
PAGES = ((0x2009C000, 0x100), (0x40008000, 0x100), (0x40090000, 0x100), (0x400FC000, 0x200))
BOUND = (0,1,2,9,10,11,49,50,51,99,100,101,219,220,221,549,550,551,999,1000,1499,1500,1999,2000,2499,2500,2999,3000,3999,4000,4999,5000,5999,6000,6001,65535,0x7fffffff,0x80000000,0xffffffff)
def word(u,a,v): u.mem_write(a,struct.pack('<I',v & 0xffffffff))
def execute(new, name, old, seed, args=()):
    u=V.machine(new); seed(u)
    for r,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args): u.reg_write(r,v)
    V.run(u,V.SYMS[name] if new else old,max_insn=300000)
    return (bytes(u.mem_read(0x10000000,0x2200)),*(bytes(u.mem_read(a,n)) for a,n in PAGES)),u.reg_read(UC_ARM_REG_R0)
def compare(name, old, seed, args=(), returns=False):
    a,ar=execute(False,name,old,seed,args); b,br=execute(True,name,old,seed,args)
    if a != b:
        k=next(k for k in range(len(a)) if a[k]!=b[k]); j=next(j for j in range(len(a[k])) if a[k][j]!=b[k][j])
        return {'region':k,'address':hex((0x10000000 if k==0 else PAGES[k-1][0])+j),'old':a[k][j],'new':b[k][j]}
    if returns and ar!=br: return {'old_return':ar,'new_return':br}

BYTE_SLOTS = (0x10002075, 0x10002076, 0x10002077, 0x10002080, 0x1000203D,
              0x10001785, 0x10001628, 0x10001634, 0x10001655, 0x10001657,
              0x10001659, 0x1000165B, 0x10001710, 0x100020C0, 0x100020C1,
              0x100020C2, 0x100015CE, 0x10002074, 0x1000207C)

WORD_SLOTS = (0x10001590, 0x10001594, 0x100015A8, 0x100015B4, 0x100015B8,
              0x100015BC, 0x100015C0, 0x100015D4, 0x100015D8, 0x10001FFC,
              0x10002000, 0x10002004, 0x10002008, 0x10002018, 0x1000201C,
              0x10002020, 0x1000202C, 0x1000203C, 0x10002058, 0x1000205C,
              0x10002068, 0x1000206C, 0x10002070, 0x10002078, 0x100020A8,
              0x100020AC, 0x100020BC, 0x100020C4, 0x100020CC, 0x100020F4,
              0x100020F8, 0x100020FC, 0x10002100, 0x10002104, 0x1000210C,
              0x10002120, 0x10002130, 0x10001624, 0x10001660, 0x10001774,
              0x10001788)

def seed6(uc, rng):
    for addr in BYTE_SLOTS:
        uc.mem_write(addr, bytes((rng.randrange(0, 256),)))
    for addr in WORD_SLOTS:
        uc.mem_write(addr, struct.pack("<I", rng.choice(
            (0, 1, 2, 3, 5, 10, 100, 1000, 3000, 5000, 0x7FFF, 0xFFFFFFFF,
             rng.randrange(0, 0x10000), rng.randrange(0, 0x2000)))))

bad=[]; counts={}
def check(name,i,old,seed,args=(),returns=False):
    try: diff=compare(name,old,seed,args,returns)
    except Exception as e: diff={'incomplete_or_exception':str(e)}
    counts[name]=counts.get(name,0)+1
    if diff:
        bad.append({'target':name,'case':i,'diff':diff,'args':args}); print(bad[-1],flush=True)
    if len(bad)>=12: raise RuntimeError('保留首12个反例，停止扩散')

try:
    # 任意 32 位寄存器索引的等价类：覆盖高位 + 每一个低字节。
    for i in range(1024):
        idx=(0,0x100,0xf00,0xffff0000)[i//256] | (i%256)
        def seed(u):
            for a in range(0x10001580,0x10001720,4): word(u,a,(a*31337)^0x55aa33cc)
            word(u,0x10003000,0xaabbccdd)
        for name,old in (('modbus_read_reg',0xad04 if is12 else 0xaf94),('modbus_write_multi',0xb050 if is12 else 0xb2e0)):
            check(name,i,old,seed,(0x10003000,idx),True)
    print('modbus index complete',counts,flush=True)
    for i in range(1800):
        rng=random.Random(0x17650000+i)
        args=tuple(rng.choice(BOUND) for _ in range(4))
        data=bytes(rng.randrange(256) for _ in range(0x80))
        mode=rng.randrange(5); source=rng.randrange(3); ra=rng.choice(BOUND); rb=rng.choice(BOUND)
        def seed(u):
            u.mem_write(0x100020b0,data)
            u.mem_write(0x1000162c if is12 else 0x10001634,bytes((mode,)))
            word(u,0x1000200c if is12 else 0x10002024,source)
            word(u,0x10001630 if is12 else 0x1000163c,ra)
            word(u,0x10001634 if is12 else 0x10001638,rb)
        check('closed_loop_integral',i,0x10a9c if is12 else 0x108b0,seed,args,True)
    print('PID complete',counts,flush=True)
    # 两通道、限流/压、阈值与计时器用不同量值；不用 GPIO 写 hook。
    for i in range(1800):
        def seed(u):
            rng=random.Random(0x6a0000+i)
            if is12:
                V._seed_output_stage(u,rng.randrange(3),rng.randrange(4),rng.choice((0,1,4,5)),rng.randrange(2),rng.choice(BOUND[:24]),rng.choice(BOUND[:24]),rng.choice(BOUND[:24]))
                for a in (0x100015b8,0x100015bc,0x100015b4,0x100015d0,0x10001ffc,0x10002030,0x10002000,0x10002004,0x10001630,0x10001634,0x1000163c): word(u,a,rng.choice(BOUND))
            else:
                seed6(u,rng)
                u.mem_write(0x10001628,bytes((rng.randrange(3),)))
                u.mem_write(0x10001634,bytes((rng.randrange(4),)))
                word(u,0x10002078,10)
                word(u,0x100015a8,rng.choice((9,10,11,100,3000)))
                word(u,0x10001624,rng.choice((0,0,0,8,16,32,0x8000)))
        check('output_stage',i,0xe70c if is12 else 0xe9ac,seed)
    print('output complete',counts,flush=True)
finally:
    import hashlib, unicorn
    out={'root':str(root),'counts':counts,'failures':bad, 'unicorn':unicorn.__version__, 'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest(), 'firmware_sha256':hashlib.sha256(V.NEW).hexdigest(), 'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (root/'tmp/independent_diff_results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(out,ensure_ascii=False),flush=True)
sys.exit(bool(bad))
