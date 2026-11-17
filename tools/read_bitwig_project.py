#!/usr/bin/env python3
"""Bounded, read-only dump of old BWPROJECT typed object graphs.

Format research: jaxter184/bwEdit-Python decoder.py (typed field encodings).
Additional format evidence: austinginder/wavelength src/bitwig.cpp (reference
numbering and typed object-key pairs). Newer class-group layouts are deliberately
not used: this reader is limited to the observed old format 0x75.
This independent reader keeps numeric IDs, offsets and types; it has no writer,
plugin loading, application connection or network code. It rejects unsupported
types and does not infer musical meaning from a successfully decoded graph.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct


class Reader:
    def __init__(self, data, start, end):
        self.data, self.pos, self.end = data, start, end
        self.objects = []
        self.blobs = []
        self.next_ref = 1

    def take(self, n):
        if n < 0 or self.pos + n > self.end:
            raise ValueError(f'Out of bounds at {self.pos}: {n} bytes')
        b = self.data[self.pos:self.pos+n]
        self.pos += n
        return b

    def number(self, code):
        return struct.unpack('>' + code, self.take(struct.calcsize('>' + code)))[0]

    def string(self):
        n = self.number('I')
        wide, n = bool(n & 0x80000000), n & 0x7fffffff
        if n > 1000000:
            raise ValueError(f'Oversized string at {self.pos}')
        return self.take(n * (2 if wide else 1)).decode('utf-16-be' if wide else 'utf-8')

    def obj(self, depth=0):
        if depth > 200 or len(self.objects) > 200000:
            raise ValueError('Graph limits exceeded')
        start = self.pos
        kind = self.number('I')
        if kind == 1:
            return {'ref': self.number('I')}
        if kind == 0 or kind == 3 or kind > 65536:
            raise ValueError(f'Unsupported object marker {kind} at {start}')
        node = {'ordinal': len(self.objects), 'ref_id': self.next_ref,
                'class': kind, 'offset': start, 'fields': {}}
        self.next_ref += 1
        self.objects.append(node)
        return self.fields(node, depth)

    def fields(self, node, depth):
        if depth > 200:
            raise ValueError('Inline graph depth exceeded')
        for _ in range(100000):
            key = self.number('I')
            if not key:
                node['end'] = self.pos
                return {'node': node['ordinal']}
            if key <= 10:
                start = self.pos-4
                name = self.take(key).decode('utf-8')
                inline = {'ordinal': len(self.objects), 'ref_id': self.next_ref,
                          'class': None, 'name': name, 'offset': start, 'fields': {}}
                self.next_ref += 1
                self.objects.append(inline)
                node['fields']['inline:' + name] = {'type': 'inline', 'value': self.fields(inline, depth+1)}
                continue
            if str(key) in node['fields']:
                raise ValueError(f'Duplicate field {key} at {self.pos-4}')
            typ = self.number('B')
            try:
                value = self.value(typ, depth+1)
            except Exception as e:
                raise ValueError(f'{e}; class {node["class"]}, field {key}, object {node["offset"]}') from e
            node['fields'][str(key)] = {'type': typ, 'value': value}
        raise ValueError('Field limit exceeded')

    def value(self, typ, depth):
        codes = {1: 'b', 2: 'h', 3: 'i', 4: 'q', 5: '?', 6: 'f', 7: 'd'}
        if typ in codes:
            return self.number(codes[typ])
        if typ == 8:
            return self.string()
        if typ == 9:
            return self.obj(depth)
        if typ in (0, 10):
            return None
        if typ == 11:
            return {'ref': self.number('I')}
        if typ == 13:
            n = self.number('I')
            start = self.pos
            b = self.take(n)
            blob = {'offset': start, 'length': n, 'sha256': hashlib.sha256(b).hexdigest(), 'head': b[:40].hex()}
            self.blobs.append(blob)
            return {'blob': len(self.blobs)-1}
        if typ == 18:
            values = []
            while True:
                marker = self.number('I')
                if marker == 3:
                    return values
                self.pos -= 4
                values.append(self.obj(depth))
                if len(values) > 200000:
                    raise ValueError('Array limit exceeded')
        if typ == 20:
            values = []
            for _ in range(100000):
                flag = self.number('B')
                if flag == 0:
                    return values
                if flag != 1:
                    raise ValueError(f'Unsupported old map tag {flag}')
                key = self.string()
                values.append([key, self.obj(depth)])
            raise ValueError('Map limit exceeded')
        if typ == 21:
            return self.take(16).hex()
        if typ == 22:
            return list(struct.unpack('>ffff', self.take(16)))
        if typ in (15, 23, 25):
            n = self.number('I')
            if n > 1000000:
                raise ValueError('Vector limit exceeded')
            return [self.string() if typ == 25 else self.number('f' if typ == 23 else 'I') for _ in range(n)]
        if typ == 26:
            node = self.obj(depth)
            return {'object': node, 'key': self.string()}
        raise ValueError(f'Unsupported type 0x{typ:02x} at {self.pos-1}')


def read(path):
    b = path.read_bytes()
    if b[:4] != b'BtWg' or len(b) < 40:
        raise ValueError('Not a supported Bitwig header')
    if int(b[12:16], 16) != 0x75:
        raise ValueError('Only observed format 0x75 is supported')
    start, end = int(b[16:24], 16), int(b[24:40], 16)
    if not 40 <= start < end <= len(b) or b[end:end+4] != b'PK\x03\x04':
        raise ValueError('Unverified object/ZIP boundaries')
    reader = Reader(b, start, end)
    root = reader.obj()
    if reader.pos != end:
        raise ValueError(f'{end-reader.pos} bytes remain after root')
    return {'source': str(path), 'sha256': hashlib.sha256(b).hexdigest(),
            'header': b[:40].decode('ascii'), 'root': root, 'objects': reader.objects,
            'blobs': reader.blobs, 'body_bytes_consumed': end-start,
            'class_counts': dict(collections.Counter(o['class'] for o in reader.objects))}


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    if args.output.resolve() == args.source.resolve():
        raise ValueError('Cannot overwrite source')
    result = read(args.source)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ['source', 'body_bytes_consumed', 'class_counts']}, indent=2))
