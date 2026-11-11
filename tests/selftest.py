#!/usr/bin/env python3
"""tests/selftest.py - checks the checker: run by CI and after tools/sync_from_rig.py.

    python3 tests/selftest.py

  - every plugin in tests/bad/ has one fault; rigcheck must FAIL exactly the check its plugin.json
    "expect"s, and nothing else;
  - the examples in plugins/ pass, their own tests included;
  - a fresh effect and source from tools/new_plugin.py pass with no warnings (the templates work);
  - info and render run.
Uses --fast builds (g++, Linux .clap), so it needs no zig.
"""
import glob, json, os, shutil, subprocess, sys, tempfile

KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHECK = [sys.executable, os.path.join(KIT, 'tools', 'check.py')]
fails = []

def say(ok, what):
    print(f'  [{"PASS" if ok else "FAIL"}] {what}'); (None if ok else fails.append(what))

def check(folder, *extra):
    out = os.path.join(tempfile.mkdtemp(), 'r.json')
    r = subprocess.run(CHECK + [folder, '--fast', '--quick', '--json', out] + list(extra), capture_output=True, text=True)
    rep = json.load(open(out)) if os.path.exists(out) else None
    return r, rep

print('1. each broken plugin fails the check it should, and only that')
for folder in sorted(glob.glob(os.path.join(KIT, 'tests', 'bad', '*'))):
    expect = json.load(open(os.path.join(folder, 'plugin.json')))['expect']
    r, rep = check(folder)
    failed = sorted({x['check'] for x in rep['results'] if x['status'] == 'FAIL'}) if rep else ['(no report: ' + r.stdout[-300:] + r.stderr[-300:] + ')']
    say(r.returncode == 1 and failed == [expect], f'{os.path.basename(folder)}: failed {", ".join(failed) or "nothing"} (expected {expect})')

print('2. the examples pass, their own tests too')
for folder in sorted(glob.glob(os.path.join(KIT, 'plugins', '*'))):
    r, rep = check(folder)
    say(r.returncode == 0 and 'TESTS PASSED' in r.stdout, f'{os.path.basename(folder)}: ' + (r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[-300:]))

print('3. fresh scaffolds pass with no warnings')
for kind, flag in (('effect', []), ('source', ['--source'])):
    pid = 'selftest' + kind[:3]
    folder = os.path.join(KIT, 'plugins', pid)
    shutil.rmtree(folder, ignore_errors=True)
    try:
        r0 = subprocess.run([sys.executable, os.path.join(KIT, 'tools', 'new_plugin.py'), pid, '--name', 'Self Test'] + flag, capture_output=True, text=True)
        r, rep = check(folder)
        warns = [x['check'] for x in rep['results'] if x['status'] != 'PASS'] if rep else ['(no report)']
        say(r0.returncode == 0 and r.returncode == 0 and not warns and 'TESTS PASSED' in r.stdout, f'new {kind}: ' + (', '.join(warns) + ' not clean' if warns else 'clean, its tests pass'))
    finally:
        shutil.rmtree(folder, ignore_errors=True); shutil.rmtree(os.path.join(KIT, 'build', 'plugins', pid), ignore_errors=True)

print('4. info and render')
r = subprocess.run(CHECK + [os.path.join(KIT, 'plugins', 'trem'), '--fast', '--info'], capture_output=True, text=True)
say(r.returncode == 0 and 'RATE' in r.stdout, 'info lists the knobs')
wav = os.path.join(tempfile.mkdtemp(), 'out.wav')
r = subprocess.run(CHECK + [os.path.join(KIT, 'plugins', 'trem'), '--fast', '--render', '--set', 'rate=6', '--set', 'depth=80%', '--out', wav], capture_output=True, text=True)
say(r.returncode == 0 and os.path.getsize(wav) > 1000000 and 'rate = 6.0 Hz' in r.stdout, 'render plays the riff through it with knobs set: ' + (r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr))

print('SELF-TEST ' + ('FAILED: ' + '; '.join(fails) if fails else 'PASSED'))
sys.exit(1 if fails else 0)
