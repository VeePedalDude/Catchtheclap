#!/usr/bin/env python3
"""tools/check.py - build a plugin and check it the way the rig will treat it.

    python3 tools/check.py plugins/<id>            build the .rigfx (Windows, Linux, browser), then check it
    python3 tools/check.py plugins/<id> --fast     only the Linux .clap, with g++ (seconds): for iterating
    python3 tools/check.py --all [--fast]          every folder in plugins/
    python3 tools/check.py some.rigfx              check a bundle someone sent (no build)
    python3 tools/check.py plugins/<id> --render [--set knob=value ...] [--in in.wav] [--out out.wav]
    python3 tools/check.py plugins/<id> --info     the knobs as the rig will show them

Checking is three steps; the last line says PASSED or FAILED:
  1. build    the plugin compiles with -Wall -Wextra -Werror (all three targets unless --fast)
  2. rigcheck the rig's conformance checks on the built .clap (tools/rigcheck/rigcheck.cpp)
  3. tests    the plugin's own tests, plugins/<id>/test.cpp, if it has one (tools/rigtest/rigtest.h)
Options passed through to rigcheck: --quick (fewer random settings, no CPU timing), --json FILE.
Needs python3, g++ (or c++) and, without --fast, zig: pip install ziglang==0.17.0.
"""
import glob, json, os, subprocess, sys, zipfile

KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(KIT, 'build')
CXX = os.environ.get('CXX', 'g++')
INC = ['-I', os.path.join(KIT, 'sdk'), '-I', os.path.join(KIT, 'engine', 'core'), '-isystem', os.path.join(KIT, 'third_party', 'clap', 'include')]
WARN = ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O2']

def run(cmd, **kw):
    return subprocess.run(cmd, text=True, capture_output=True, **kw)

def newer(out, srcs):
    return os.path.exists(out) and all(os.path.getmtime(s) < os.path.getmtime(out) for s in srcs)

def rigcheck():
    """build/rigcheck, rebuilt when its sources change."""
    exe = os.path.join(BUILD, 'rigcheck')
    srcs = glob.glob(os.path.join(KIT, 'tools', 'rigcheck', '*')) + glob.glob(os.path.join(KIT, 'tools', 'rigtest', '*')) + glob.glob(os.path.join(KIT, 'engine', 'core', '*'))
    if not newer(exe, srcs):
        os.makedirs(BUILD, exist_ok=True)
        r = run([CXX] + WARN + ['-rdynamic', '-I', os.path.join(KIT, 'engine', 'core'), '-isystem', os.path.join(KIT, 'third_party', 'clap', 'include'),
                 '-isystem', os.path.join(KIT, 'third_party', 'nlohmann'), '-o', exe, os.path.join(KIT, 'tools', 'rigcheck', 'rigcheck.cpp'), '-ldl'])
        if r.returncode: sys.exit('building rigcheck failed:\n' + r.stderr)
    return exe

def entry(folder):
    """The few lines tools/build_plugin.py generates for a plugin folder."""
    meta = json.load(open(os.path.join(folder, 'plugin.json')))
    pid = os.path.basename(folder.rstrip('/'))
    kind = meta.get('kind', 'effect')
    q = lambda s: '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
    macro = 'RIGFX_SOURCE' if kind == 'source' else 'RIGFX_EFFECT'
    return pid, (f'#include "rigfx.h"\n#include {q(os.path.join(folder, pid + ".h"))}\n'
                 f'{macro}({meta["struct"]}, {q(meta.get("category", "experimental"))}, {q(json.dumps(meta.get("tags", [])))}, '
                 f'{q(meta.get("author", ""))}, {q(meta.get("version", "1.0.0"))})\n')

def build(folder, fast):
    """-> (path of a Linux .clap to check, error text or None)"""
    pid, src = entry(folder)
    out = os.path.join(BUILD, 'plugins', pid); os.makedirs(out, exist_ok=True)
    clap = os.path.join(out, 'x86_64-linux.clap')
    if fast:
        cpp = os.path.join(out, 'entry.cpp'); open(cpp, 'w').write(src)
        r = run([CXX] + WARN + ['-shared', '-fPIC', '-fvisibility=hidden', '-fno-exceptions', '-fno-rtti'] + INC + ['-o', clap, cpp])
        return (clap, None) if r.returncode == 0 else (None, r.stderr)
    bundle = os.path.join(KIT, 'dist', 'plugins', pid + '.rigfx')
    r = run([sys.executable, os.path.join(KIT, 'tools', 'build_plugin.py'), folder, '-o', bundle])
    sys.stdout.write(r.stdout)
    if r.returncode: return None, r.stderr or r.stdout
    return unpack(bundle, out), None

def unpack(bundle, out):
    os.makedirs(out, exist_ok=True)
    clap = os.path.join(out, 'x86_64-linux.clap')
    with zipfile.ZipFile(bundle) as z:
        if 'clap/x86_64-linux.clap' not in z.namelist(): sys.exit(f'{bundle}: no clap/x86_64-linux.clap inside (not a .rigfx?)')
        data = z.read('clap/x86_64-linux.clap')
    tmp = clap + '.new'; open(tmp, 'wb').write(data); os.replace(tmp, clap)   # never write over a file another run may have loaded
    return clap

def own_tests(folder):
    """plugins/<id>/test.cpp, compiled against the header itself (tools/rigtest/rigtest.h). -> (ok, text) or None."""
    test = os.path.join(folder, 'test.cpp')
    if not os.path.exists(test): return None
    pid = os.path.basename(folder.rstrip('/'))
    exe = os.path.join(BUILD, 'plugins', pid, 'test')
    r = run([CXX] + WARN + ['-I', folder, '-I', os.path.join(KIT, 'tools', 'rigtest')] + INC + ['-o', exe, test])
    if r.returncode: return False, 'test.cpp did not compile:\n' + r.stderr
    r = run([exe], cwd=folder)
    return r.returncode == 0, r.stdout + r.stderr

def check_one(target, a):
    """-> True if it passed."""
    target = os.path.abspath(target)
    is_bundle = target.endswith('.rigfx') or target.endswith('.clap')
    name = os.path.basename(target.rstrip('/'))
    print(f'== {name}')
    if is_bundle:
        clap = target if target.endswith('.clap') else unpack(target, os.path.join(BUILD, 'bundles', os.path.splitext(name)[0]))
    else:
        if not os.path.exists(os.path.join(target, 'plugin.json')): print(f'  {target}: no plugin.json (a plugin folder holds <id>.h and plugin.json)'); return False
        print('-- 1. build' + (' (Linux .clap only: --fast)' if a['fast'] else ' (Windows, Linux and browser)'))
        clap, err = build(target, a['fast'])
        if err: print('  [FAIL] build: it does not compile\n' + err); print('FAILED: does not build'); return False
        print('  [PASS] build: compiles with -Wall -Wextra -Werror' + ('' if a['fast'] else f' for all three targets: dist/plugins/{name}.rigfx'))
    exe = rigcheck()
    if a['info'] or a['render']:
        cmd = [exe, 'info' if a['info'] else 'render', clap] + a['pass']
        return subprocess.run(cmd).returncode == 0
    print('-- 2. rigcheck')
    r = subprocess.run([exe, 'check', clap, '--rig', os.path.join(KIT, 'rig.json')] + a['pass'])
    ok = r.returncode == 0
    if not is_bundle:
        t = own_tests(target)
        print('-- 3. tests' + ('' if t else ': none (plugins/%s/test.cpp checks what the effect claims: see tools/rigtest/rigtest.h)' % name))
        if t:
            print(t[1].rstrip()); ok = ok and t[0]
    print(('PASSED' if ok else 'FAILED') + f': {name}' + ('' if a['fast'] or is_bundle else ' (built for all targets)') + (' -- run without --fast before handing it over' if a['fast'] and ok else ''))
    return ok

def main():
    args = sys.argv[1:]
    if not args or args[0] in ('-h', '--help'): sys.exit(__doc__)
    a = {'fast': False, 'info': False, 'render': False, 'pass': [], 'targets': []}
    i = 0
    while i < len(args):
        x = args[i]
        if x == '--fast': a['fast'] = True
        elif x == '--info': a['info'] = True
        elif x == '--render': a['render'] = True
        elif x == '--all': a['targets'] += sorted(d for d in glob.glob(os.path.join(KIT, 'plugins', '*')) if os.path.isdir(d))
        elif x == '--quick': a['pass'].append(x)
        elif x in ('--json', '--set', '--in', '--out', '--sr') and i + 1 < len(args): a['pass'] += [x, args[i + 1]]; i += 1
        elif x.startswith('--'): sys.exit(f'check.py: what is {x}? (python3 tools/check.py --help)')
        else: a['targets'].append(x)
        i += 1
    if not a['targets']: sys.exit('check.py: which plugin? (a folder in plugins/, a .rigfx, or --all)')
    fails = [t for t in a['targets'] if not check_one(t, a)]
    if len(a['targets']) > 1: print(f'== {len(a["targets"]) - len(fails)} of {len(a["targets"])} passed' + (': failed ' + ', '.join(os.path.basename(f.rstrip('/')) for f in fails) if fails else ''))
    sys.exit(1 if fails else 0)

if __name__ == '__main__':
    main()
