"""本批门禁负向反证：仿真内代码注入，必须正常返回/退出后检出有效语义差异。"""
import sys,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'test/support'),str(ROOT/'test/emulation'),str(ROOT/'tools/audit')]
from software_ab_support import *
import test_modbus_boundary_contract as B
import test_parameter_bank_profile_contract as P
import menu_complete_matrix as M
import test_password_inner_iteration_contract as W
import test_gpio_readback_probe_contract as G
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
from capstone.arm import ARM_OP_IMM

def main():
    real=V.machine;detected=[]
    def inject(name,patch=b'\x70\x47',at=None):
        def machine(new):
            u=real(new)
            if new:u.mem_write(V.SYMS[name] if at is None else at,patch)
            return u
        V.machine=machine
    try:
        for new in (False,True):B.BASE[new]=B.initialize(new);M.BASE[new]=M.base(new)
        wire=frame((1,6,17,1,0,1));gold=B.observe(False,wire)
        inject('modbus_dispatch');bad=B.observe(True,wire)
        assert gold!=bad and gold[1] and not bad[1],'缺失应答未被检出'
        detected.append('modbus_missing_response_after_normal_return');V.machine=real
        assert B.observe(True,wire)==gold
        inject('load_config')
        try:P.observe(True,2,2,3)
        except AssertionError as e:assert '有效银行未实际加载PID档位' in str(e),repr(e)
        else:raise AssertionError('缺失有效银行装载未被检出')
        detected.append('missing_second_magic_byte_valid_bank_load');V.machine=real
        assert P.observe(False,2,2,3)==P.observe(True,2,2,3)
        gold=M.observe(False,3,8,2,1,250)
        inject('state_machine');bad=M.observe(True,3,8,2,1,250)
        assert gold!=bad and gold[1] and not bad[1],'菜单业务空跑未被检出'
        detected.append('menu_missing_state_and_display_after_normal_return');V.machine=real
        assert M.observe(True,3,8,2,1,250)==gold
        at=W.NEW_DELAY[1];raw=V.NEW[at:at+4];cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB);cs.detail=True
        first=next(cs.disasm(raw,at));mutation=None
        for bit in range(32):
            code=bytearray(raw);code[bit//8]^=1<<(bit%8)
            i=next(cs.disasm(bytes(code),at),None)
            if i and i.size==4 and i.mnemonic.startswith('cmp') and i.operands[0].reg==first.operands[0].reg and len(i.operands)==2 and i.operands[1].type==ARM_OP_IMM and 900<=i.operands[1].imm<1000:
                mutation=(bytes(code),i.operands[1].imm);break
        assert mutation,'编译器指令变化，需重新核对内层突变'
        inject('state_machine',mutation[0],at)
        try:W.observe(True,W.NEW_DELAY)
        except AssertionError as e:assert '第一轮内层被提前跳过' in str(e),repr(e)
        else:raise AssertionError('真实内层少于1000次未被检出')
        detected.append('inner_threshold_1000_to_'+str(mutation[1])+'_detected_after_full_exit');V.machine=real
        W.observe(True,W.NEW_DELAY)
        inject('fio0_pin22_ctrl')
        try:G.leaf(True,'fio0_pin22_ctrl',0,0xffffffff)
        except AssertionError as e:assert '没有实际读取FIOCLR' in str(e),repr(e)
        else:raise AssertionError('GPIO读写观察空跑未被检出')
        detected.append('GPIO_clear_leaf_not_executed');V.machine=real
        assert G.leaf(False,'fio0_pin22_ctrl',0,0xffffffff)==G.leaf(True,'fio0_pin22_ctrl',0,0xffffffff)
    finally:V.machine=real
    out={'project':ROOT.name,'injections_detected':detected,'restored_baseline_pass':True,
         'firmware_sha256':hashlib.sha256(V.NEW).hexdigest(),'original_sha256':hashlib.sha256(V.ORIGINAL).hexdigest()}
    (ROOT/'tmp/branch_gap_negative_controls.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
    print('BRANCH_GAP_NEGATIVE_CONTROLS actual_semantic_failures_detected=%d restored_baselines PASS'%len(detected),flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
