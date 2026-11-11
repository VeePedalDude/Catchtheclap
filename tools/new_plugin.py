#!/usr/bin/env python3
"""tools/new_plugin.py - start a plugin folder from the templates.

    python3 tools/new_plugin.py <id> [--source] [--name "NAME"] [--sub "what it is"] [--category CAT]

Makes plugins/<id>/ with <id>.h (a working effect, or with --source a modulation source), plugin.json and
test.cpp. It passes every check as it is: `python3 tools/check.py plugins/<id> --fast`. Then make it yours.
The id: a-z, 0-9 and _, starting with a letter, 12 characters at most, not one the rig has (rig.json).
"""
import argparse, json, os, re, sys

KIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('id'); ap.add_argument('--source', action='store_true')
    ap.add_argument('--name'); ap.add_argument('--sub'); ap.add_argument('--category'); ap.add_argument('--author', default='')
    a = ap.parse_args()
    rig = json.load(open(os.path.join(KIT, 'rig.json')))
    if not re.fullmatch(r'[a-z][a-z0-9_]{0,11}', a.id): sys.exit(f'"{a.id}": an id is a-z, 0-9 and _, starting with a letter, 12 characters at most')
    if a.id in rig['builtinEffects'] + rig['builtinSources']: sys.exit(f'"{a.id}" is one of the rig\'s built-ins: pick another')
    folder = os.path.join(KIT, 'plugins', a.id)
    if os.path.exists(folder): sys.exit(f'plugins/{a.id} exists already')
    cats = [c['id'] for c in rig['sourceCategories' if a.source else 'effectCategories']]
    cat = a.category or ('time' if a.source else 'experimental')
    if cat not in cats: sys.exit(f'"{cat}" isn\'t a category; one of: {", ".join(cats)}')
    struct = ''.join(w.capitalize() for w in a.id.split('_')) or 'Plugin'
    name = (a.name or a.id.replace('_', ' ')).upper()[:24]
    sub = a.sub or ('a modulation source' if a.source else 'an effect')
    fill = lambda t: (t.replace('{{ID}}', a.id).replace('{{UPPER}}', a.id.upper()).replace('{{STRUCT}}', struct)
                       .replace('{{NAME}}', name).replace('{{SUB}}', sub.replace('"', "'")))
    os.makedirs(folder)
    kind = 'source' if a.source else 'effect'
    open(os.path.join(folder, a.id + '.h'), 'w').write(fill(open(os.path.join(KIT, 'templates', kind + '.h')).read()))
    open(os.path.join(folder, 'test.cpp'), 'w').write(fill(open(os.path.join(KIT, 'templates', 'test_' + kind + '.cpp')).read()))
    json.dump({'struct': 'rig::' + struct, 'kind': kind, 'category': cat, 'tags': [], 'author': a.author, 'version': '0.1.0'},
              open(os.path.join(folder, 'plugin.json'), 'w'), indent=2)
    open(os.path.join(folder, 'plugin.json'), 'a').write('\n')
    print(f'made plugins/{a.id}/: {a.id}.h ({kind} rig::{struct}, "{name}"), plugin.json ({cat}), test.cpp')
    print(f'next: python3 tools/check.py plugins/{a.id} --fast')

if __name__ == '__main__':
    main()
