"""确定性事件序列缩减；只接受同类已复现差异，异常/超时不能充当重现。"""
def minimize(events,reproduces):
    current=list(events)
    assert current and reproduces(current),'原始事件未有效复现目标差异'
    granularity=2
    while len(current)>=2:
        size=(len(current)+granularity-1)//granularity;reduced=False
        for start in range(0,len(current),size):
            candidate=current[:start]+current[start+size:]
            if candidate and reproduces(candidate):
                current=candidate;granularity=max(2,granularity-1);reduced=True;break
        if not reduced:
            if granularity>=len(current):break
            granularity=min(len(current),granularity*2)
    return current
