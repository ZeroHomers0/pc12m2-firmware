"""验证门禁分支会运行，且分支/PR/手动/外仓不能触发滚动发布。"""
import copy,re
from pathlib import Path
import yaml
ROOT=Path(__file__).resolve().parents[2]

def eligible(expression,event,ref,repo):
    values={'event_name':event,'ref':ref,'repository':repo}
    terms=expression.split(' && ')
    assert len(terms)==3,'发布条件结构变化，必须重新审查'
    found={}
    for term in terms:
        m=re.fullmatch(r"github\.(event_name|ref|repository) == '([^']+)'",term)
        assert m and m[1] not in found,('不支持或重复的发布条件',term)
        found[m[1]]=m[2]
    assert set(found)==set(values)
    return all(values[k]==v for k,v in found.items())

def validate(w):
    is12=(ROOT/'pc12m2_orig.bin').exists();slug='pc12m2' if is12 else 'pc6m10'
    repo='ZeroHomers0/'+slug+'-firmware'
    assert set(w['on']['push']['branches'])=={'main','firmware-menu-display-2026-11-13'}
    assert 'pull_request' in w['on'] and 'workflow_dispatch' in w['on']
    build=w['jobs']['build'];publish=w['jobs']['publish']
    assert build['permissions']=={'contents':'read'} and publish['permissions']=={'contents':'write'}
    assert publish['needs']=='build'
    assert any(s.get('run')=='python tools/verification/run_software_gates.py --extended' for s in build['steps'])
    assert not any('gh release' in s.get('run','') for s in build['steps'])
    expression=publish['if']
    cases=(('push','refs/heads/main',repo,True),
           ('push','refs/heads/firmware-menu-display-2026-11-13',repo,False),
           ('pull_request','refs/pull/1/merge',repo,False),
           ('workflow_dispatch','refs/heads/main',repo,False),
           ('push','refs/heads/main','someone/fork',False))
    for event,ref,repository,expected in cases:assert eligible(expression,event,ref,repository)==expected

def main():
    w=yaml.load((ROOT/'.github/workflows/build.yml').read_text(encoding='utf-8'),Loader=yaml.BaseLoader)
    validate(w)
    bad=copy.deepcopy(w);bad['jobs']['publish']['if']="github.event_name == 'push'"
    try:validate(bad)
    except AssertionError:pass
    else:raise AssertionError('缺少分支/仓库门控仍被接受')
    print('WORKFLOW_RELEASE_POLICY five_event_cases + missing_guard_negative PASS')
    return 0
if __name__=='__main__':raise SystemExit(main())
