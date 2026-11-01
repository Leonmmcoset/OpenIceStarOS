"""Shared real-x86/ordinary-C/installed-C execution for recovery batches."""
import hashlib
import json
import time
from pathlib import Path
from tools.kernel_image import Fat16,ORIGINAL_SHA256
from tests.kernel_font_outline import compile_variants
ROOT=Path(__file__).resolve().parents[1];BUILD=ROOT/'build';BASE=0x800000

def compare(module,slots,machine,cases,installed,checks,bindings=None):
    started=time.monotonic();original=Fat16((ROOT.parent/'IceStarOS_x86_32M.vhd').read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
    variants,sizes=compile_variants(original,installed,module,slots,source=f'kernel/{module}.c',bindings=bindings)
    machines=[machine(*spec,original,b'') for spec in [('original',original,BASE,[s for _,_,s,_ in slots]),*variants]]
    counts={n:0 for n,_,_,_ in slots}
    for i,case in enumerate(cases):
        if i%500==0:print(module,i,'/',len(cases),flush=True)
        try:
            expected=machines[0].execute(case)
            for m in machines[1:]:
                actual=m.execute(case);assert actual==expected,(m.label,expected,actual)
        except Exception:
            print('Failing',module,i,{k:v for k,v in case.items() if k!='setup'},flush=True);raise
        counts[slots[case['function']][0]]+=1
    summary=dict(installed=installed,functions=len(slots),scenarios=len(cases),executions=len(cases)*3,slots=sizes,
        scenarios_by_function=counts,seconds=round(time.monotonic()-started,3),checks=checks,
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in (f'kernel/{module}.c',f'kernel/{module}.h',f'tests/kernel_{module}.py','tests/kernel_recovery_compare.py')})
    (BUILD/f'{module}_summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
