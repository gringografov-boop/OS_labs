#!/usr/bin/env python3
"""Real subprocess integration tests. Usage: python test_lab2.py <parent-binary>."""
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile

parent = Path(sys.argv[1]).resolve()
child = parent.with_name('child.exe' if parent.suffix.lower() == '.exe' else 'child')
passed = 0


def run_case(name, commands, expected, expected_code=0, filename='result.txt'):
    global passed
    with tempfile.TemporaryDirectory(prefix='lab2-') as folder:
        root = Path(folder)
        output = root / filename
        data = str(output) + '\n' + commands
        result = subprocess.run([str(parent)], input=data, text=True, encoding='utf-8',
                                capture_output=True, cwd=root, timeout=15)
        assert result.returncode == expected_code, (name, result.returncode, result.stdout, result.stderr)
        if expected is not None:
            assert output.read_bytes() == expected.encode('ascii'), (name, output.read_bytes())
        passed += 1
        print('PASS', name)


run_case('composites then prime', '4\n6\n9\n17\n25\n', '4\n6\n9\n')
run_case('negative stops both processes', '8\n-4\n10\n', '8\n')
run_case('two is prime', '2\n4\n', '')
run_case('zero and one ignored', '0\n1\n4\n-1\n', '4\n')
run_case('EOF after last value without newline', '4\n6', '4\n6\n')
run_case('EOF before first value', '', '')
run_case('invalid text and multiple values', 'abc\n4 6\n\n4\n-1\n', '4\n')
run_case('range overflow', '2147483648\n-2147483649\n4\n-1\n', '4\n')
run_case('int minimum stops', '4\n-2147483648\n6\n', '4\n')
run_case('int maximum is prime', '2147483647\n4\n', '')
run_case('largest even int', '2147483646\n-1\n', '2147483646\n')
run_case('whitespace plus and CRLF', '  +4  \r\n\t9\t\n+-4\n+\n--2\n-1\n', '4\n9\n')
run_case('large input and partial writes', 'x' * 200000 + '\n4\n-1\n', '4\n')
run_case('file name containing spaces', '4\n-1\n', '4\n', filename='output file.txt')
run_case('unicode file name', '4\n-1\n', '4\n', filename='результат.txt')
run_case('invalid output directory', '4\n', None, expected_code=1, filename='absent/result.txt')

rng = random.Random(5)
composites = [2 * rng.randint(2, 1000000) for _ in range(300)]
run_case('300 consecutive request-response exchanges', ''.join(f'{n}\n' for n in composites) + '-1\n',
         ''.join(f'{n}\n' for n in composites))

with tempfile.TemporaryDirectory(prefix='lab2 copies ') as folder:
    root = Path(folder)
    copy_parent = root / parent.name
    copy_child = root / child.name
    shutil.copy2(parent, copy_parent)
    shutil.copy2(child, copy_child)
    result = subprocess.run([str(copy_parent)], input=f'{root / "out.txt"}\n4\n-1\n',
                            text=True, encoding='utf-8', capture_output=True, timeout=15)
    assert result.returncode == 0, result.stderr
    assert (root / 'out.txt').read_bytes() == b'4\n'
    passed += 1
    print('PASS executable path containing spaces')
    copy_child.unlink()
    result = subprocess.run([str(copy_parent)], input='out.txt\n4\n', text=True,
                            capture_output=True, cwd=root, timeout=15)
    assert result.returncode != 0
    passed += 1
    print('PASS missing child executable')
    if os.name == 'posix':
        copy_child.write_text('not an executable\n')
        copy_child.chmod(0o755)
        result = subprocess.run([str(copy_parent)], input='out.txt\n4\n', text=True,
                                capture_output=True, cwd=root, timeout=15)
        assert result.returncode != 0
        passed += 1
        print('PASS exec error reported to parent')

for data in ['', '\n']:
    result = subprocess.run([str(parent)], input=data, text=True, capture_output=True, timeout=15)
    assert result.returncode == 1
    passed += 1
    print('PASS empty file name / EOF')

result = subprocess.run([str(child)], capture_output=True, timeout=15)
assert result.returncode == 1
passed += 1
print('PASS child checks argument count')
print(f'{passed} integration tests passed')
