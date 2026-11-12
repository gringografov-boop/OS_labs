#!/usr/bin/env python3
"""Usage: python3 lab3/tests/test_lab3.py path/to/lab3"""
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
checks = 0


def reference(matrix, iterations, window, minimum):
    result = [row[:] for row in matrix]
    rows, cols = len(matrix), len(matrix[0])
    r = window // 2
    operation = min if minimum else max
    for _ in range(iterations):
        result = [[operation(result[x][y]
                            for x in range(max(0, i-r), min(rows, i+r+1))
                            for y in range(max(0, j-r), min(cols, j+r+1)))
                   for j in range(cols)] for i in range(rows)]
    return result


def write_matrix(path, matrix):
    path.write_text(f'{len(matrix)} {len(matrix[0])}\n' +
                    '\n'.join(' '.join(map(str, row)) for row in matrix) + '\n')


def read_matrix(path):
    tokens = path.read_text().split()
    rows, cols = map(int, tokens[:2])
    values = list(map(float, tokens[2:]))
    assert len(values) == rows * cols
    return [values[i*cols:(i+1)*cols] for i in range(rows)]


with tempfile.TemporaryDirectory(prefix='lab3 test ') as directory:
    root = Path(directory)
    source = root / 'input matrix.txt'
    prefix = root / 'output matrix'
    matrix = [[4, -2, 7, 10], [8, 3, 0, 6], [9, 5, 1, -4]]
    for threads, iterations, window in [(1, 1, 3), (2, 1, 3), (3, 1, 3), (20, 1, 3),
                                        (3, 3, 3), (2, 2, 5), (3, 2, 1), (3, 0, 3)]:
        write_matrix(source, matrix)
        command = [str(binary), '-i', str(source), '-o', str(prefix), '-t', str(threads),
                   '-k', str(iterations), '-w', str(window), '-v']
        result = subprocess.run(command, text=True, capture_output=True, timeout=15)
        assert result.returncode == 0, (command, result.stderr)
        for suffix, minimum in [('erosion', True), ('dilation', False)]:
            assert read_matrix(Path(str(prefix) + '_' + suffix + '.txt')) == reference(
                matrix, iterations, window, minimum), (threads, iterations, window, suffix)
        actual = 1 if iterations == 0 else min(threads, len(matrix))
        assert f'THREADS={actual}' in result.stdout
        if iterations > 0:
            assert result.stdout.count('START PID=') == actual
            assert result.stdout.count('DONE PID=') == actual
        checks += 1
    for matrix in [[[42]], [[1, 2, 3, 4]], [[1], [2], [3]]]:
        write_matrix(source, matrix)
        result = subprocess.run([str(binary), '-i', str(source), '-o', str(prefix),
                                 '-t', '9', '-k', '2', '-w', '5'], capture_output=True, timeout=15)
        assert result.returncode == 0, result.stderr
        for suffix, minimum in [('erosion', True), ('dilation', False)]:
            assert read_matrix(Path(str(prefix)+'_'+suffix+'.txt')) == reference(matrix, 2, 5, minimum)
        checks += 1
    for text in ['0 2\n', '2 2\n1 2 3\n', '1 1\n1 2\n', '1 1\nnan\n', '1 1\ninf\n']:
        source.write_text(text)
        result = subprocess.run([str(binary), '-i', str(source), '-o', str(prefix)],
                                capture_output=True, timeout=15)
        assert result.returncode != 0, text
        checks += 1
    write_matrix(source, [[1, 2], [3, 4]])
    for extra in [['-w', '2'], ['-t', '0'], ['-k', '-1'], ['-z'], []]:
        command = [str(binary)] if not extra else [str(binary), '-i', str(source)] + extra
        result = subprocess.run(command, capture_output=True, timeout=15)
        assert result.returncode != 0, command
        checks += 1
    result = subprocess.run([str(binary), '-h'], capture_output=True, timeout=15)
    assert result.returncode == 0 and b'Usage:' in result.stdout
    checks += 1
print(f'{checks} checks passed; both filters matched the independent sequential reference.')
