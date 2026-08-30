# -*- coding: utf-8 -*-
# =============================================================================
# run_tests.py — host 虚拟数据测试 runner
#
# 用法：cd firmware/test && python run_tests.py [测试名...]
#   不带参数：跑 test_*.py 全部；带名：只跑匹配的（前缀匹配，如 crc16）
#
# 每个测试模块约定：main() 返回 0=全过，非0=有失败；print 用 UTF-8。
# =============================================================================
import sys, os, importlib.util, glob

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'support'))
try:
    sys.stdout.reconfigure(encoding='utf-8')  # Win 控制台中文乱码规避
except Exception:
    pass

def discover(prefixes):
    files = sorted(glob.glob(os.path.join(HERE, '**', 'test_*.py'), recursive=True))
    if prefixes:
        files = [f for f in files if any(p in os.path.basename(f) for p in prefixes)]
    return files

def main():
    args = sys.argv[1:]
    files = discover(args)
    if not files:
        print(f"[run_tests] 未找到匹配的测试（args={args}）。可用：")
        for f in sorted(glob.glob(os.path.join(HERE, '**', 'test_*.py'), recursive=True)):
            print("  ", os.path.relpath(f, HERE))
        return 1
    total_p = total_f = 0
    for f in files:
        modname = "test_" + os.path.relpath(f, HERE).replace(os.sep, "_").replace(".py", "")
        spec = importlib.util.spec_from_file_location(modname, f)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        if not hasattr(mod, 'main'):
            print(f"[run_tests] {modname}: 无 main()，跳过"); continue
        print(f"\n=== {modname} ===")
        rc = mod.main()
        if rc == 0:
            print(f"[run_tests] {modname}: OK")
        else:
            print(f"[run_tests] {modname}: FAIL (rc={rc})")
        total_f += (rc != 0)
        total_p += (rc == 0)
    print(f"\n[run_tests] 模块通过 {total_p}/{total_p+total_f}")
    return 0 if total_f == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
