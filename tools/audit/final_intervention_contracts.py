"""按已复核的层次契约登记每个干预；机制扫描不自动等于等价通过。

历史诊断脚本不作为最终验收入口，原文件/日志全部保留。
"""
import sys,json,hashlib,runpy,collections,ast
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def main():
    runpy.run_path(str(ROOT/'tools/audit/test_intervention_inventory.py'))
    j=json.loads((ROOT/'tmp/test_intervention_inventory.json').read_text(encoding='utf-8'))
    is12=(ROOT/'pc12m2_orig.bin').exists()
    proofs={
        'DISPLAY_LAYER':(['test/emulation/test_numeric_leaf_contract.py','test/emulation/test_basic_navigation_bounds_contract.py'],
            '显示替身只用于上层参数/顺序；数字函数真实执行至字符叶，原版完整渲染和定制字形另测。不得据替身证明GPIO整链。'),
        'EEPROM_DEVICE':(['test/emulation/test_i2c_bit_contract.py','test/emulation/test_parameter_bank_profile_contract.py'],
            '旧寄存器/字节替身限于上层写入意图；持久介质、ACK/NACK/忙、位收发与重启装载分别真实执行验证。'),
        'ADC_DEVICE':(['test/emulation/test_remaining_peripheral_contract.py','test/emulation/test_peripheral_polling.py'],
            '旧start/wait替身限于扫描换算；最终按通道输入GDR，实际start/wait/scan及未就绪等待均执行。'),
        'AUTH_MAIN_CUSTOM':(['test/emulation/test_auth_response_device_contract.py','test/emulation/test_main_modes_contract.py'],
            'main用外部认证结果边界替身；认证器24拍实际读取/延时与响应另测，永久放行单列PASS_CUSTOM，联锁不得绕过。'),
        'DELAY_EQ_SUMMARY':(['test/emulation/test_password_inner_iteration_contract.py','tools/audit/completion_reset_inner_probe.py'],
            '仅缩短已证实纯空转内层；密码首轮1000、复位首轮2000真实执行，外层/喂狗/终态与精确自跳独立断言。不推导真实耗时。'),
        'GPIO_MODEL':(['test/emulation/test_gpio_readback_probe_contract.py','test/emulation/test_peripheral_semantic_contract.py'],
            '用CODE指令观察规避FIO内存hook伪影；SET/CLR/W1C/NVIC为明示模型，WO三读回探针不证明硅片读取行为。'),
        'EXECUTION_BOUNDARY':(['test/emulation/test_execution_completion.py','test/emulation/test_runner_hook_lifetime.py'],
            '正常调用必须返回哨兵；预算耗尽拒绝。等待/自跳/半途观察须断言确切PC/次数，不与正常返回合并。'),
        'STARTUP_ADAPTATION':(['test/emulation/test_reset_initialization_contract.py'],
            '固定原BIN散装载与GCC替代链从非零RAM执行至main；仅原IAR已核实8字节临时栈和独立SRAM1布局按ABI说明。'),
        'FINITE_PREEMPTION':(['test/emulation/test_interrupt_preemption_contract.py','test/emulation/test_peripheral_semantic_contract.py'],
            'context保存/恢复模拟明确的共享store前后插入ISR；检查中断返回点与被中断上下文，不证明所有可能抢占。'),
        'NEGATIVE_CONTROL':(['tools/audit/branch_gap_negative_controls.py','tools/audit/completion_bounds_negative.py'],
            '缺陷注入/旧构建仅用于证明检出能力；不得计入正常等价；finally恢复镜像/符号，恢复后须通过。'),
        'OBSERVATION_ONLY':(['tools/audit/run_branch_gap_batch.py'],
            '分支/写迹采集不改业务值和PC；other后继失败，入口参数独立，失败批次不得进入增量基线。'),
        'LAYERED_COMPOSITION':(['test/emulation/test_main_software_chain.py','test/emulation/test_menu_persistent_workflow.py'],
            '参数种子与R0入参属于声明输入；显示/EEPROM/延时替身按各独立层契约组合，连续多拍保留SRAM，不以每拍重置掩盖累计误差。')}
    if is12:proofs['FINITE_PREEMPTION'][0][:]=[x for x in proofs['FINITE_PREEMPTION'][0] if (ROOT/x).exists()]
    special={
        'test_adc_scan_sequence_equivalence.py':'ADC_DEVICE','test_remaining_peripheral_contract.py':'ADC_DEVICE',
        'test_main_software_chain.py':'AUTH_MAIN_CUSTOM','test_main_modes_contract.py':'AUTH_MAIN_CUSTOM',
        'test_auth_response_device_contract.py':'AUTH_MAIN_CUSTOM',
        'test_reset_initialization_contract.py':'STARTUP_ADAPTATION','verify_startup_adaptation_contract.py':'STARTUP_ADAPTATION',
        'test_interrupt_preemption_contract.py':'FINITE_PREEMPTION',
        'test_eeprom_load_config_equivalence.py':'EEPROM_DEVICE','test_eeprom_sync_matrix_equivalence.py':'EEPROM_DEVICE',
        'test_parameter_sync_execution.py':'EEPROM_DEVICE','software_ab_support.py':'EEPROM_DEVICE',
        'test_i2c_bit_contract.py':'EEPROM_DEVICE',
        'test_init_trace.py':'DELAY_EQ_SUMMARY','test_init_trace_12.py':'DELAY_EQ_SUMMARY',
        'test_password_inner_iteration_contract.py':'DELAY_EQ_SUMMARY','test_state_lifecycle_contract.py':'DELAY_EQ_SUMMARY',
        'menu_audit_support.py':'DELAY_EQ_SUMMARY','completion_reset_inner_probe.py':'DELAY_EQ_SUMMARY',
        'mmio_code_trace.py':'GPIO_MODEL','peripheral_semantics.py':'GPIO_MODEL',
        'test_gpio_readback_probe_contract.py':'GPIO_MODEL','test_peripheral_semantic_contract.py':'GPIO_MODEL',
        'branch_gap_negative_controls.py':'NEGATIVE_CONTROL','completion_bounds_negative.py':'NEGATIVE_CONTROL',
        'software_branch_inventory.py':'OBSERVATION_ONLY','run_branch_gap_batch.py':'OBSERVATION_ONLY',
        'extend_menu_branch_inventory.py':'OBSERVATION_ONLY',
        'completion_terminal_probe.py':'LAYERED_COMPOSITION','completion_final_candidates.py':'STARTUP_ADAPTATION',
        'independent_control_diff.py':'LAYERED_COMPOSITION','lifecycle_negative_controls.py':'NEGATIVE_CONTROL',
        'test_execution_completion.py':'EXECUTION_BOUNDARY','test_direct_execution_completion.py':'EXECUTION_BOUNDARY',
        'test_execution_completion_12.py':'EXECUTION_BOUNDARY','test_runner_hook_lifetime.py':'EXECUTION_BOUNDARY',
        'unicorn_harness.py':'EXECUTION_BOUNDARY'}
    rows=[]
    for r in j['entries']:
        p=ROOT/r['source'];name=p.name;source=p.read_text(encoding='utf-8-sig')
        if '/full_review_2026-11-16/' in r['source'] or name=='modbus_short_frame_underflow.py':
            role='HISTORICAL_DIAGNOSTIC_NOT_ACCEPTANCE';reason='保留历史诊断；最终结论由有返回/目标命中门禁的当前用例代替，历史无门禁PASS不再转用。';evidence=[]
        elif r['mechanism']=='MASK_OR_SKIP_HELPER' and r['expression']=='_mask_fio1_p23':
            tree=ast.parse(source);calls=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id=='_mask_fio1_p23']
            assert not calls,'旧P1.23宽泛mask重新进入验收路径'
            role='DORMANT_MASK_NOT_CALLED';reason='当前入口没有调用该历史mask；GPIO另由完整CODE写迹/叶函数/三读回模型验证。';evidence=proofs['GPIO_MODEL'][0]
        else:
            role=special.get(name)
            if role is None:
                if 'display' in name or 'numeric' in name or 'menu_text' in name:role='DISPLAY_LAYER'
                elif name.startswith(('test_','verify_','scan_')) or name in ('menu_complete_matrix.py','completion_state_probe.py','calibration_state_gap_probe.py'):role='LAYERED_COMPOSITION'
                else:raise AssertionError(('未登记干预来源',r['source'],r['line']))
            evidence,reason=proofs[role]
        for f in evidence:assert (ROOT/f).exists(),('缺少替代证明入口',f)
        rows.append({**r,'review_status':role,'contract':reason,'alternative_evidence':evidence,'source_sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
    out={'project':ROOT.name,'status':'REVIEWED_WITH_EXPLICIT_LAYER_LIMITS','counts':dict(collections.Counter(r['review_status'] for r in rows)),
         'unclassified':0,'entries':rows,'all_hardware_interleavings_proven':False,'no_automatic_approval_inferred':True}
    (ROOT/'tmp/completion_intervention_contracts.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    print('FINAL_INTERVENTION_CONTRACTS',len(rows),'unclassified=0',out['counts'],flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
