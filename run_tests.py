#!/usr/bin/env python3
"""
Compile C test programs with kcc, run them in the headless KiT emulator,
and check the results.

Usage: python3 run_tests.py [-v] [tests/foo.c ...]   (default: tests/*.c)
       python3 run_tests.py --bench [--save-baseline] [tests/foo.c ...]
    -v, --verbose       print expected and actual results for every test
    --bench             print program size and cycle counts, compared to bench.json
                        (default tests: BENCH_TESTS)
    --save-baseline     with --bench, save the results to bench.json

Tests declare what they expect in comments:
    // return: 111          main's return value (A + 256 * X, as a signed int)
    // screen: 0 1 2 3      text that must appear somewhere on the screen

Build output for each test goes in build/tests/<name>/.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
KIT_EMU = Path(os.environ.get('KIT_EMU', ROOT.parent / 'kit-emu')).resolve()
KICKASS = Path(os.environ.get('KICKASS', ROOT.parent / 'KickAssembler' / 'KickAss.jar')).resolve()
EMU_CLASSES = ROOT / 'build' / 'kit-emu'
BUILD_DIR = ROOT / 'build' / 'tests'
BASELINE = ROOT / 'bench.json'
TIMEOUT_MS = int(os.environ.get('TIMEOUT_MS', 10000))

# Programs measured by --bench. Keep them unchanged so results stay comparable with bench.json.
BENCH_TESTS = ['everything_functions', 'everything_operators', 'milestone1', 'collatz', 'collatz_arithmetic', 'insertion_sort']

# Cycles are only counted for instructions in kcc's code segment (see template.asm),
# so the monitor and interrupt handlers in ROM don't affect the count.
CODE_RANGE = '1000-4fff'

GREEN = '\033[32m'
RED = '\033[1m\033[31m'
DIM = '\033[2m'
RESET = '\033[0m'


def parse_expectations(source):
    expected_return = None
    expected_screen = []

    for key, value in re.findall(r'^\s*//\s*(return|screen):[ \t]?(.*)$', source, re.MULTILINE):
        if key == 'return':
            expected_return = int(value.strip(), 0)
        else:
            expected_screen.append(value.rstrip())

    return expected_return, expected_screen


def run(cmd, cwd=ROOT, timeout=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)


def parse_emulator_output(output):
    match = re.search(r'A: (\d+)\s+X: (\d+)', output)
    if match is None:
        return None, [], None

    a, x = int(match.group(1)), int(match.group(2))
    value = (x << 8) | a
    if value >= 0x8000:
        value -= 0x10000

    lines = output.splitlines()
    start = lines.index('Screen:') + 1
    screen = lines[start:start + 16]

    cycles = re.search(r'^Cycles: (\d+)$', output, re.MULTILINE)

    return value, screen, cycles and int(cycles.group(1))


def format_screen(screen):
    return '\n'.join('    |' + line + '|' for line in screen)


def run_test(test):
    """
    Returns a dict with the test's expectations, its actual results (if it got
    that far), the log of the stage that failed (if any), and a list of failures.
    """
    out_dir = BUILD_DIR / test.stem
    out_dir.mkdir(parents=True, exist_ok=True)
    asm = out_dir / 'out.asm'
    prg = out_dir / 'out.prg'
    prg.unlink(missing_ok=True)

    expected_return, expected_screen = parse_expectations(test.read_text())
    info = {
        'expected_return': expected_return,
        'expected_screen': expected_screen,
        'actual_return': None,
        'screen': [],
        'cycles': None,
        'size': None,
        'log': None,
        'failures': [],
    }

    def fail(message, log=None):
        info['failures'].append(message)
        info['log'] = log
        return info

    result = run(['./kcc', str(test), str(asm)])
    log = result.stdout + result.stderr
    (out_dir / 'kcc.txt').write_text(log)
    if result.returncode != 0:
        return fail(f'kcc failed (exit {result.returncode}), see {out_dir / "kcc.txt"}', log)

    result = run(['java', '-jar', str(KICKASS), str(asm), '-define', 'KIT_TEST', '-odir', str(out_dir)])
    log = result.stdout + result.stderr
    (out_dir / 'kickass.txt').write_text(log)
    if result.returncode != 0 or not prg.is_file():
        return fail(f'assembly failed, see {out_dir / "kickass.txt"}', log)

    # .prg files start with a 2-byte load address
    info['size'] = prg.stat().st_size - 2

    emu_cmd = ['java', '-Djava.awt.headless=true', '-cp', str(EMU_CLASSES), 'KiT', '--headless', '--no-save-ssd',
               '--count-cycles', CODE_RANGE, '--prg', str(prg), '--timeout-ms', str(TIMEOUT_MS)]
    try:
        result = run(emu_cmd, cwd=KIT_EMU, timeout=TIMEOUT_MS / 1000 + 10)
    except subprocess.TimeoutExpired:
        return fail('emulator hung')

    log = result.stdout + result.stderr
    (out_dir / 'emu.txt').write_text(log)
    info['actual_return'], info['screen'], info['cycles'] = parse_emulator_output(result.stdout)

    if result.returncode == 2:
        return fail(f'timed out before reaching the end of main, see {out_dir / "emu.txt"}')
    if result.returncode != 0:
        return fail(f'emulator failed (exit {result.returncode}), see {out_dir / "emu.txt"}', log)

    screen_text = ''.join(info['screen'])
    failures = info['failures']

    if expected_return is not None and info['actual_return'] != expected_return:
        failures.append(f'returned {info["actual_return"]}, expected {expected_return}')

    for text in expected_screen:
        if text not in screen_text:
            failures.append(f'screen is missing {text!r}')

    return info


def print_details(info):
    expected_return = info['expected_return']
    actual_return = info['actual_return']
    print(f'    expected return: {"(any)" if expected_return is None else expected_return}')
    print(f'    actual return:   {"(did not run)" if actual_return is None else actual_return}')
    if info['size'] is not None:
        print(f'    size:            {info["size"]} bytes')
    if info['cycles'] is not None:
        print(f'    cycles:          {info["cycles"]}')

    if info['expected_screen']:
        print('    expected on screen:')
        for text in info['expected_screen']:
            print(f'        {text!r}')

    if info['screen']:
        print('    screen:')
        print(format_screen(info['screen']))

    if info['log']:
        print(f'    {DIM}log:')
        print('\n'.join('        ' + line for line in info['log'].rstrip().splitlines()) + RESET)


def format_change(new, old):
    if old is None:
        return ''
    diff = new - old
    if diff == 0:
        return f'{DIM}(same){RESET}'
    color = GREEN if diff < 0 else RED
    return f'{color}({diff:+d}, {100 * diff / old:+.1f}%){RESET}'


def pad(text, width):
    visible = len(re.sub(r'\033\[[0-9;]*m', '', text))
    return text + ' ' * max(0, width - visible)


def bench(tests, save_baseline):
    baseline = json.loads(BASELINE.read_text()) if BASELINE.is_file() else {}
    results = {}
    num_failed = 0

    print(f'{"test":<24}{"bytes":>7}  {"":<18}{"cycles":>9}')
    for test in tests:
        info = run_test(test)
        if info['failures'] or info['cycles'] is None:
            num_failed += 1
            print(f'{test.stem:<24}{RED}FAIL{RESET} {info["failures"][0] if info["failures"] else "no cycle count"}')
            continue

        results[test.stem] = {'bytes': info['size'], 'cycles': info['cycles']}
        old = baseline.get(test.stem, {})
        print(f'{test.stem:<24}{info["size"]:>7}  {pad(format_change(info["size"], old.get("bytes")), 18)}'
              f'{info["cycles"]:>9}  {format_change(info["cycles"], old.get("cycles"))}')

    if num_failed:
        print(f'\n{num_failed} test(s) failed; fix them before comparing performance.')
        sys.exit(1)

    if save_baseline:
        BASELINE.write_text(json.dumps(results, indent=4) + '\n')
        print(f'\nSaved baseline to {os.path.relpath(BASELINE, ROOT)}')
    elif not baseline:
        print(f'\nNo baseline yet. Save one with `make bench-save`.')


def main():
    if not (EMU_CLASSES / 'KiT.class').is_file():
        sys.exit(f'Emulator not built in {EMU_CLASSES}. Run `make test`.')

    parser = argparse.ArgumentParser(description='Run kcc tests in the headless KiT emulator.')
    parser.add_argument('-v', '--verbose', action='store_true', help='print expected and actual results for every test')
    parser.add_argument('--bench', action='store_true', help='print size and cycle counts compared to bench.json')
    parser.add_argument('--save-baseline', action='store_true', help='with --bench, save results to bench.json')
    parser.add_argument('tests', nargs='*', help='test files (default: tests/*.c, or BENCH_TESTS with --bench)')
    args = parser.parse_args()

    if args.bench:
        tests = [Path(arg).resolve() for arg in args.tests] or [ROOT / 'tests' / f'{name}.c' for name in BENCH_TESTS]
        bench(tests, args.save_baseline)
        return

    tests = [Path(arg).resolve() for arg in args.tests] or sorted((ROOT / 'tests').glob('*.c'))
    num_failed = 0

    for test in tests:
        info = run_test(test)
        failures = info['failures']
        if failures:
            num_failed += 1
            print(f'{RED}FAIL{RESET} {os.path.relpath(test, ROOT)}')
            for failure in failures:
                print(f'    {failure}')
        else:
            print(f'{GREEN}PASS{RESET} {os.path.relpath(test, ROOT)}')

        if args.verbose:
            print_details(info)
            print()
        elif any(f.startswith('screen') for f in failures):
            print('    screen:')
            print(format_screen(info['screen']))

    print(f'\n{len(tests) - num_failed}/{len(tests)} tests passed')
    sys.exit(1 if num_failed else 0)


if __name__ == '__main__':
    main()
