import math
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil

folder=Path(sys.argv[1]).resolve()
suffix='.exe' if sys.platform=='win32' else ''

def run(name,text,base=folder):
    p=subprocess.run([str(base/(name+suffix))],input=text,text=True,capture_output=True,timeout=20)
    return p

def close(a,b):
    assert math.isclose(float(a),b,rel_tol=1e-5,abs_tol=1e-5),(a,b)

p=run('lab5_static','1 1 0.5\n2 3 4\n')
assert p.returncode==0,p.stderr
v=p.stdout.splitlines()
close(v[0],(math.cos(1.5)-math.cos(1))/.5)
close(v[1],12)
print('PASS program 1 linked implementation')
p=run('lab5_dynamic','2 3 4\n0\n1 1 0.5\n2 3 4\n0\n2 3 4\n')
assert p.returncode==0,p.stderr
v=p.stdout.splitlines()
assert v[0]=='12' and v[1]=='Implementation 2' and v[3]=='6' and v[4]=='Implementation 1' and v[5]=='12',v
close(v[2],(math.cos(1.5)-math.cos(.5)))
print('PASS switching both contracts')
p=run('lab5_dynamic','1 1 0\n2 -1 2\n1 nan 1\n1 1e999 1\n2 3\n0 x\nbad\n2 2 3')
assert p.returncode==0 and p.stdout.strip()=='6' and len(p.stderr.splitlines())==7,(p.stdout,p.stderr)
print('PASS invalid input and EOF')
p=run('lab5_dynamic','0\n'*200+'2 3 4\n')
assert p.returncode==0 and p.stdout.splitlines()[-1]=='12',p.stderr
print('PASS 200 load/unload switches')
with tempfile.TemporaryDirectory(prefix='lab5 spaces ') as tmp:
    dest=Path(tmp)
    for f in folder.iterdir():
        if f.is_file() and (f.name.startswith('lab5_') or f.name.startswith('liblab5_')):
            shutil.copy2(f,dest/f.name)
    p=run('lab5_dynamic','2 3 4',dest)
    assert p.returncode==0 and p.stdout.strip()=='12',p.stderr
    for f in dest.glob('*impl2*'): f.unlink()
    p=run('lab5_dynamic','2 3 4\n0\n',dest)
    assert p.returncode!=0 and p.stdout.splitlines()[0]=='12',p.stderr
print('PASS relocation and missing replacement library')
print('All integration checks passed')
