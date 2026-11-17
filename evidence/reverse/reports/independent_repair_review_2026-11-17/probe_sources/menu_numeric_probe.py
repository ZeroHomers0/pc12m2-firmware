import sys, itertools, json
from pathlib import Path
root=Path(sys.argv[1]); is12=root.name=='PC12M-2'
sys.path.insert(0,str(root/'tmp'))
if is12:
    import probe_sm12_matrix_v4 as S
    base=dict(S.BASE_FIXED)
    slots=(0x10001628,0x10001630,0x10001634,0x10001638,0x1000163c,0x10001640,0x10001644,0x10001648,0x10001654,0x10001658,0x10001690,0x10001694,0x10001698,0x1000169c,0x100016a0)
else:
    import sm_matrix as S
    base=dict(S.BASE)
    S.MASK_ADDRS=[]
    slots=(0x1000162c,0x10001638,0x1000163c,0x10001640,0x10001644,0x10001648,0x10001650,0x10001654,0x10001660,0x10001698,0x1000169c,0x100016a0,0x100016a4,0x100016a8)
bad=[]; counts=0
for value,menu,item,key,st in itertools.product((0,1,0x7fffffff,0x80000000,0xffffffff),(1,3,4,5,7,0x14,0x1e,0x63),range(17),(0,2,3,0x16,0x21),(0xfa,0x1f4)):
    params=dict(base); params.update({a:value for a in slots})
    try:
        if is12:
            S.BASE_FIXED=params
            a=S.trace(False,menu,item,1,key,st); b=S.trace(True,menu,item,1,key,st)
        else:
            a=S.run_case(False,menu,item,1,key,params,st); b=S.run_case(True,menu,item,1,key,params,st)
            assert a[2] and b[2], 'incomplete'
        if a!=b:
            diffs=[]
            for k,(aa,bb) in enumerate(zip(a,b)):
                if aa!=bb:
                    j=next((i for i in range(min(len(aa),len(bb))) if aa[i]!=bb[i]),min(len(aa),len(bb)))
                    diffs.append((k,j,aa[j:j+4],bb[j:j+4]))
            bad.append((value,menu,item,key,st,repr(diffs)))
    except Exception as e: bad.append((value,menu,item,key,st,str(e)))
    counts+=1
    if bad and len(bad)<=16: print('DIFF',bad[-1],flush=True)
    if len(bad)>=16: break
print('menu_numeric cases=',counts,'bad=',len(bad),flush=True)
(root/'tmp/menu_numeric_results.json').write_text(json.dumps({'cases':counts,'bad':bad},indent=2),encoding='utf-8')
sys.exit(bool(bad))
