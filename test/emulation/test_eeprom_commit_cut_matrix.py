"""关键多字节参数每个实际EEPROM提交之前/之后断电，新CPU只继承介质。

字节提交为已声明的模型边界，不推导真实EEPROM内部写周期的半写电气结果。
"""
import sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'test/support'))
from software_ab_support import *

class CommitLog(list):
    def __init__(self,store,after):super().__init__();self.store=store;self.after=after
    def append(self,event):
        super().append(event)
        if self.after is not None and event[0]=='commit' and self.store.commits==self.after:
            raise PowerCut('已提交指定字节之后断电')

CASES=((2,0x03e8),(3,0x0456),(5,0x0155),(6,0x0222),(9,0x00ad),(0x33,0x0fa0),(0x3e,0x0080))

def observe(new,reg,value,cut=None,mode='complete'):
    s=Eeprom();u=setup(new,s);call(u,new,'load_config')
    s.commits=0;s.events=CommitLog(s,cut if mode=='after' else None)
    s.cut_after=cut if mode=='before' else None
    interrupted=False;sent=[]
    try:sent=dispatch(u,new,frame((1,6,0x10,reg,value>>8,value&255)))
    except PowerCut:interrupted=True
    assert interrupted==(mode!='complete'),('提交边界未实际掉电',reg,cut,mode,s.commits)
    if interrupted:assert s.commits==cut,('切点字节计数错误',reg,cut,mode,s.commits)
    else:assert sent==[frame((1,6,0x10,reg,value>>8,value&255))],'正常完成路径未正确应答'
    pre=bytes(u.mem_read(0x10000000,0x2900));events=list(s.events);image=bytes(s.image)
    media=Eeprom(image);v=setup(new,media);call(v,new,'load_config')
    return image,events,pre,bytes(v.mem_read(0x10000000,0x2900)),media.events,bytes(media.image),sent,s.commits

def main():
    rows=[];cases=0
    for reg,value in CASES:
        gold=observe(False,reg,value);assert gold==observe(True,reg,value),('完整写入',reg)
        commits=gold[-1];assert commits>=2,('没有实际写入多字节参数',reg,commits)
        for mode,cuts in (('before',range(commits)),('after',range(1,commits+1))):
            for cut in cuts:
                a=observe(False,reg,value,cut,mode);b=observe(True,reg,value,cut,mode)
                assert a==b,('字节提交断电差异',hex(reg),cut,mode,[i for i,(x,y) in enumerate(zip(a,b)) if x!=y])
                rows.append({'register':reg,'value':value,'mode':mode,'committed_bytes':cut});cases+=1
        cases+=1
    out={'project':ROOT.name,'status':'PASS','cases':cases,'cuts':rows,'byte_commit_model_only':True}
    (ROOT/'tmp/eeprom_commit_cut_matrix.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    print('EEPROM_COMMIT_CUT_MATRIX',cases,'before/after each_actual_byte + fresh_CPU_reload PASS',flush=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
