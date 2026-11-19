#!/usr/bin/env python3
"""Convert an Arturia preset (Boost text archive) into a render_au settings file.

arturia_preset.py <preset file> <params.json from render_au> <out settings.txt> [resources dir]

With a resources dir, internal names (e.g. CUTOFF_FILTER) are translated to AU
display names through the product's XML <param name=... display_name=...> entries.

Entries are "<length> <name> <value>"; only names that the Audio Unit exposes as
parameters are written. Prints how many preset entries were matched, so state that
is not reachable through AU parameters is reported rather than silently dropped.
Reads the preset as data; nothing is executed.
"""
import json
import pathlib
import re
import sys


def entries(text, known):
    found, i, n = {}, 0, len(text)
    while i < n:
        j = i
        while j < n and text[j].isdigit():
            j += 1
        if j > i and j < n and text[j] == ' ':
            length = int(text[i:j])
            name = text[j + 1:j + 1 + length]
            k = j + 1 + length
            if 0 < length < 80 and k < n and text[k] == ' ' and name in known:
                m = k + 1
                while m < n and text[m] not in ' \n':
                    m += 1
                try:
                    found[name] = float(text[k + 1:m])
                    i = m
                    continue
                except ValueError:
                    pass
        i = j + 1 if j > i else i + 1
    return found


def display_names(resources):
    mapping = {}
    for xml in pathlib.Path(resources).rglob('*.xml'):
        for name, display in re.findall(r'<param\s+name="([^"]+)"\s+display_name="([^"]+)"', xml.read_text(errors='ignore')):
            mapping.setdefault(name, display)
    return mapping


def main():
    preset, params, out = sys.argv[1:4]
    text = open(preset, 'rb').read().decode('latin-1')
    known = {p['name'] for p in json.load(open(params))['parameters']}
    mapping = display_names(sys.argv[4]) if len(sys.argv) > 4 else {}
    internal = entries(text, set(mapping)) if mapping else {}
    values = entries(text, known)
    for name, value in internal.items():
        if mapping[name] in known:
            values.setdefault(mapping[name], value)
    with open(out, 'w') as f:
        f.write(f'# From Arturia preset: {preset}\n# {len(values)} values matched to AU parameters\n')
        for name in sorted(values):
            f.write(f'{name}={values[name]:.8g}\n')
    print(json.dumps({'preset': preset, 'matched': len(values), 'au_parameters': len(known)}))


if __name__ == '__main__':
    main()
