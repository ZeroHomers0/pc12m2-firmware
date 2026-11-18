"""执行现有验证入口并核对仿真器真正加载的金标准/当前BIN或ELF节。

只在加载完成时观察，不安装执行hook，不干预业务、PC、返回值或比较结果。
故障注入可在已核实基线之后显式改仿真内代码，单独由其负向测试负责。
"""
import sys,json,hashlib,runpy,ast,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools/verification'),str(ROOT/'test/support'),str(ROOT/'test/emulation')]

def attest_memory(u,expected,address=0):
    actual=bytes(u.mem_read(address,len(expected)))
    assert actual==expected,('LOADED_IMAGE_MISMATCH',hex(address),hashlib.sha256(actual).hexdigest(),hashlib.sha256(expected).hexdigest())

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',required=True);parser.add_argument('entry');parser.add_argument('args',nargs=argparse.REMAINDER)
    args=parser.parse_args();entry=(ROOT/args.entry).resolve();assert entry.is_relative_to(ROOT) and entry.exists()
    is12=(ROOT/'pc12m2_orig.bin').exists();V=__import__('verify_firmware_equivalence_12' if is12 else 'verify_firmware_equivalence')
    original=bytes(V.ORIGINAL);current=bytes(V.NEW)
    assert current==(ROOT/'firmware/firmware.bin').read_bytes()
    assert original==(ROOT/('pc12m2_orig.bin' if is12 else 'LPC1765.bin')).read_bytes()
    counts={'original_BIN':0,'current_BIN':0,'current_ELF_sections':0};real=V.machine;restore=[]
    def machine(new):
        u=real(new);attest_memory(u,current if new else original)
        counts['current_BIN' if new else 'original_BIN']+=1;return u
    V.machine=machine;rc=1
    if (ROOT/'test/support/unicorn_harness.py').exists() and not is12:
        import unicorn_harness as H
        old_load=H.load_original;new_load=H.load_firmware
        def old_loader(*a,**kw):
            u=old_load(*a,**kw);attest_memory(u,original);counts['original_BIN']+=1;return u
        def new_loader(*a,**kw):
            path=Path(kw.get('elf_path',a[0] if a else H.FW)).resolve()
            assert path==(ROOT/'firmware/firmware.elf').resolve(),'未声明的ELF替换'
            u=new_load(*a,**kw);sections,raw=H.parse_sections(str(path))
            for name in ('.text','.isr_vector','.fw_image','.data'):
                if name in sections:
                    address,offset,size=sections[name]
                    if size:attest_memory(u,raw[offset:offset+size],address)
            counts['current_ELF_sections']+=1;return u
        H.load_original=old_loader;H.load_firmware=new_loader
        restore.append((H,old_load,new_load))
    saved_argv=sys.argv[:];sys.argv=[str(entry),*args.args]
    try:
        if entry==Path(V.__file__).resolve():
            tree=ast.parse(entry.read_text(encoding='utf-8-sig'))
            blocks=[n for n in tree.body if isinstance(n,ast.If) and '__name__' in ast.unparse(n.test)]
            assert len(blocks)==1,'官方入口结构变化，不能静默绕过'
            exec(compile(ast.Module(body=blocks[0].body,type_ignores=[]),str(entry),'exec'),V.__dict__)
        else:
            try:runpy.run_path(str(entry),run_name='__main__')
            except SystemExit as e:
                if e.code not in (None,0):raise
        assert sum(counts.values())>0,'验证入口未实际加载任何被验镜像'
        rc=0
    finally:
        V.machine=real;sys.argv=saved_argv
        for H,old_load,new_load in restore:H.load_original=old_load;H.load_firmware=new_load
        record={'entry':args.entry,'args':args.args,'exit_code':rc,'loads':counts,
                'original_sha256':hashlib.sha256(original).hexdigest(),'firmware_sha256':hashlib.sha256(current).hexdigest(),
                'ELF_sections_only_note':'旧ELF加载器只验其实际装载节，不把Flash填充区未装载伪装为整BIN装载。'}
        out=ROOT/args.output;out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
        print('LOADED_IMAGE_ATTESTATION',counts,'exit',rc,flush=True)
    return rc
if __name__=='__main__':raise SystemExit(main())
