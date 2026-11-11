#!/usr/bin/env python3
"""tools/sync_from_rig.py - refresh this kit from a RIG/01 checkout.

    python3 tools/sync_from_rig.py ../DIY-multi-effect-

Copies, unchanged, everything a plugin is built and checked against (the list is FILES below), and
writes rig.json: the built-in effect and source ids a plugin must not take, and the category tabs.
RIG_VERSION records the RIG/01 commit they came from. Run it whenever the rig's plugin API changes,
then `python3 tools/check.py --all` to see that every plugin still builds and passes.
"""
import json, os, re, shutil, subprocess, sys

FILES = [
    'sdk/rigfx.h', 'sdk/rig_clap_ext.h',                              # the plugin wrapper and the rig's CLAP extension
    'engine/core/param.h', 'engine/core/dsp.h',                       # what a plugin includes
    'engine/core/module.h', 'engine/core/source.h',
    'engine/core/eq.h', 'engine/core/vaf.h', 'engine/core/fft.h',     # helpers a plugin may include
    'engine/core/notes.h', 'engine/core/polytrack.h', 'engine/core/tap.h',
    'engine/core/plugin.h', 'engine/core/plugin_clap.h',              # the rig's own loader (rigcheck uses it)
    'tools/build_plugin.py',
    'third_party/clap/include', 'third_party/clap/LICENSE',
    'third_party/nlohmann/json.hpp',
    'plugins/trem', 'plugins/rand',                                   # the examples
    'docs/PLUGINS.md',
    'dist/rig-test.html',                                             # -> preview/rig-test.html
    'LICENSE',
]

def categories(path):
    out = []
    for line in open(path):
        line = line.strip()
        if not line or line.startswith('#'): continue
        cid, label, what = [x.strip() for x in line.split('|')]
        out.append({'id': cid, 'label': label, 'what': what})
    return out

def ids(tree, desc):
    found = []
    for dirpath, _, names in os.walk(tree):
        for n in sorted(names):
            if not n.endswith('.h'): continue
            for m in re.finditer(desc + r'\s+\w+\s*=\s*\{\s*"([a-z][a-z0-9_]*)"', open(os.path.join(dirpath, n)).read()):
                found.append(m.group(1))
    return sorted(set(found))

def main():
    if len(sys.argv) != 2: sys.exit(__doc__)
    rig = os.path.abspath(sys.argv[1]); kit = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if not os.path.exists(os.path.join(rig, 'sdk', 'rigfx.h')): sys.exit(f'{rig}: not a RIG/01 checkout (no sdk/rigfx.h)')
    for f in FILES:
        src = os.path.join(rig, f)
        dst = os.path.join(kit, 'preview/rig-test.html' if f == 'dist/rig-test.html' else 'docs/rig/PLUGINS.md' if f == 'docs/PLUGINS.md' else f)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if os.path.isdir(src):                                        # the rig's files replace ours; the kit's own extras
            if f.startswith('third_party'): shutil.rmtree(dst, ignore_errors=True)   # (an example's test.cpp) stay
            shutil.copytree(src, dst, dirs_exist_ok=True)
        else:
            shutil.copy2(src, dst)
    info = {
        'builtinEffects': ids(os.path.join(rig, 'engine', 'modules'), 'ModuleDesc'),
        'builtinSources': ids(os.path.join(rig, 'engine', 'sources'), 'SourceDesc'),
        'effectCategories': categories(os.path.join(rig, 'engine', 'modules', 'categories.txt')),
        'sourceCategories': categories(os.path.join(rig, 'engine', 'sources', 'categories.txt')),
    }
    json.dump(info, open(os.path.join(kit, 'rig.json'), 'w'), indent=2)
    commit = subprocess.run(['git', '-C', rig, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    branch = subprocess.run(['git', '-C', rig, 'rev-parse', '--abbrev-ref', 'HEAD'], capture_output=True, text=True).stdout.strip()
    open(os.path.join(kit, 'RIG_VERSION'), 'w').write(f'{commit} ({branch})\n')
    print(f'synced {len(FILES)} entries from {rig} at {commit[:10]} ({branch})')
    print(f'  {len(info["builtinEffects"])} built-in effects, {len(info["builtinSources"])} built-in sources: rig.json')

if __name__ == '__main__':
    main()
