#!/usr/bin/env python3
"""Read LinPlug VST2 presets (.fxp) into their module sections.

Written from observation of the preset bytes and of container behaviour seen in
the plugins' disassembly. No LinPlug code is copied. The format is described in
../docs/linplug_findings.md.

Layout:
  FXP header (big-endian, standard VST2 'CcnK' / 'FPCh', opaque chunk)
  Spectral ('LSP1') chunks are run-length packed first (see unpack_runs).
  chunk = sequence of sections:
      tag   4 bytes, little-endian four-character code (e.g. 'ME05')
      size  uint32 LE, INCLUDES the 8-byte tag+size header
      body  int32 LE words. Three token kinds:
            int    one word
            string [len][len bytes of text + NUL padding], len = strlen+1 rounded up to 4;
                   empty is [4][0], or in CronoX sometimes a bare [0]
            list   [1000000 + n][n words]
  CronoX ('CrX4', 'C2x3') and MorphoX ('LwA5') chunks are not packed.

Schemas (tools/schemas/<plugin id>.json) give each section tag its token
pattern, learnt from a preset corpus, so presets parse strictly. Without a
schema a heuristic guesses, and an int 4 followed by a small value can be
misread as a one-character string.

Usage:
  python3 -I linplug_fxp.py dump   PRESET.fxp [TAG ...]
  python3 -I linplug_fxp.py json   PRESET.fxp
  python3 -I linplug_fxp.py fields PRESET.fxp [TAG ...]   (named, scaled values)
  python3 -I linplug_fxp.py verify PRESET_DIR [PRESET_DIR ...] [--write-schemas]
  python3 -I linplug_fxp.py roundtrip PRESET_DIR [PRESET_DIR ...]
      re-encode every preset from its parsed tokens and compare with the original
"""
import collections
import json
import os
import struct
import sys

SCHEMA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'schemas')
LIST_BASE = 1_000_000
PACKED_PLUGINS = {'LSP1'}  # Spectral


def read_fxp(path):
    b = open(path, 'rb').read()
    magic, _size, kind, _ver, fxid, fxver, nparams = struct.unpack('>4sI4sI4sII', b[:28])
    if magic != b'CcnK':
        raise ValueError(f'{path}: not an FXP (magic {magic!r})')
    if kind != b'FPCh':
        raise ValueError(f'{path}: kind {kind!r}; only opaque-chunk presets are handled')
    name = b[28:56].split(b'\0')[0].decode('latin1')
    csize = struct.unpack('>I', b[56:60])[0]
    chunk = b[60:60 + csize]
    if len(chunk) != csize:
        raise ValueError(f'{path}: chunk truncated ({len(chunk)} of {csize})')
    return {'plugin_id': fxid.decode('latin1'), 'fx_version': fxver, 'num_params': nparams,
            'name': name, 'chunk': chunk, 'trailing_bytes': len(b) - 60 - csize,
            'fxp_version': _ver, 'name_field': b[28:56].hex(),
            # LinPlug writes 0 here in some presets instead of the byte count
            'size_field': _size}


def unpack_runs(data):
    """Expand a run-length packed chunk (Spectral).

    u16 LE expanded size, then control bytes c:
      0x00-0x7F  copy the next c+1 bytes
      0x80-0x9F  (c&31)+2 bytes of 0x00
      0xA0-0xBF  (c&31)+2 bytes of 0x01
      0xC0-0xDF  (c&31)+2 copies of the next byte
      0xE0-0xFF  next byte L, then (c&31)+2 copies of the following L bytes
    Returns (expanded bytes, unconsumed input byte count).
    """
    if len(data) < 2:
        raise ValueError('packed chunk shorter than its header')
    want = data[0] | data[1] << 8
    out = bytearray()
    p = 2
    while p < len(data) and len(out) < want:
        c = data[p]
        p += 1
        if c < 0x80:
            n = c + 1
            if p + n > len(data):
                raise ValueError(f'literal run past end at {p - 1}')
            out += data[p:p + n]
            p += n
            continue
        n = (c & 0x1f) + 2
        kind = c >> 5
        if kind == 4:
            out += b'\0' * n
        elif kind == 5:
            out += b'\x01' * n
        elif kind == 6:
            out += data[p:p + 1] * n
            p += 1
        else:
            plen = data[p]
            out += data[p + 1:p + 1 + plen] * n
            p += 1 + plen
    if len(out) != want:
        raise ValueError(f'expanded to {len(out)} bytes, header says {want}')
    return bytes(out), len(data) - p


def section_bytes(d):
    if d['plugin_id'] in PACKED_PLUGINS:
        raw, left = unpack_runs(d['chunk'])
        if left:
            raise ValueError(f'{left} packed bytes left over')
        return raw
    return d['chunk']


def split_sections(chunk):
    out, p = [], 0
    while p + 8 <= len(chunk):
        tag = chunk[p:p + 4][::-1].decode('latin1')
        size = struct.unpack('<I', chunk[p + 4:p + 8])[0]
        if size < 8 or p + size > len(chunk):
            raise ValueError(f'bad section at {p}: tag {tag!r} size {size}')
        out.append((tag, chunk[p + 8:p + size]))
        p += size
    if p != len(chunk):
        raise ValueError(f'{len(chunk) - p} stray bytes after last section')
    return out


def _word(body, p):
    return struct.unpack('<i', body[p:p + 4])[0]


class ZeroLenStr(str):
    """An empty string saved as a bare length word 0 (no padding word), seen in CronoX."""


def _string_at(body, p, strict=False):
    """A string as written by the plugin: len = strlen+1 rounded up to 4, NUL padded.

    Heuristic reading (strict=False) accepts only printable ASCII up to 256 bytes,
    so ints are rarely misread as text. When a schema says a string belongs here
    (strict=True) it also accepts:
      [4][0]  an empty string, which looks like two ints (CronoX empty paths/names)
      [0]     an empty string with no padding word (returned as ZeroLenStr)
      Latin-1 text (sample paths such as 'loops f\xfcr cronox3') and longer paths.
    """
    if p + 4 > len(body):
        return None
    n = _word(body, p)
    if strict and n == 0:
        return 0, ZeroLenStr()
    if n < 4 or n > (4096 if strict else 256) or n % 4 or p + 4 + n > len(body):
        return None
    raw = body[p + 4:p + 4 + n]
    if strict and raw == b'\0' * n and n == 4:
        return 4, ''
    z = raw.find(b'\0')
    if z < 1 or raw[z:].strip(b'\0') or (z + 4) // 4 * 4 != n:
        return None
    text = raw[:z]
    if not strict and any(c < 0x20 or c > 0x7e for c in text):
        return None
    if strict and any(c < 0x20 for c in text):
        return None
    return n, text.decode('latin1')


def _list_at(body, p):
    if p + 4 > len(body):
        return None
    w = _word(body, p)
    n = w - LIST_BASE
    if n < 0 or p + 4 + 4 * n > len(body):
        return None
    return n, list(struct.unpack(f'<{n}i', body[p + 4:p + 4 + 4 * n]))


def tokenize(body, sig=None):
    """Split a section body into ints, strings and int lists.

    sig is an optional pattern, one letter per token: i (int), s (string),
    l (list). With a pattern the parse is strict. Older saves end early because
    later versions appended parameters (of any kind) to a section; a body that
    ends cleanly on a token boundary is accepted and the caller reports how
    many trailing tokens were missing.
    """
    if len(body) % 4:
        raise ValueError(f'section body length {len(body)} is not a multiple of 4')
    out, p = [], 0
    if sig is not None:
        for k, kind in enumerate(sig):
            if p == len(body):
                return out
            if p + 4 > len(body):
                raise ValueError(f'body ends at token {k} of {len(sig)}')
            if kind == 's':
                s = _string_at(body, p, strict=True)
                if not s:
                    raise ValueError(f'token {k}: expected string')
                out.append(s[1])
                p += 4 + s[0]
            elif kind == 'l':
                lst = _list_at(body, p)
                if lst is None:
                    raise ValueError(f'token {k}: expected list')
                out.append(lst[1])
                p += 4 + 4 * lst[0]
            else:
                out.append(_word(body, p))
                p += 4
        if p != len(body):
            raise ValueError(f'{len(body) - p} bytes left after schema')
        return out
    while p < len(body):
        lst = _list_at(body, p)
        if lst is not None:
            out.append(lst[1])
            p += 4 + 4 * lst[0]
            continue
        s = _string_at(body, p)
        if s:
            out.append(s[1])
            p += 4 + s[0]
        else:
            out.append(_word(body, p))
            p += 4
    return out


def signature(tokens):
    return ''.join('s' if isinstance(t, str) else 'l' if isinstance(t, list) else 'i'
                   for t in tokens)


def load_schema(plugin_id):
    path = os.path.join(SCHEMA_DIR, f'{plugin_id}.json')
    if os.path.exists(path):
        return json.load(open(path))
    return None


def parse(path, schema=None, use_saved=True):
    """schema: {tag: pattern}. When None and use_saved, tools/schemas/<id>.json is used if present."""
    d = read_fxp(path)
    if schema is None and use_saved:
        schema = load_schema(d['plugin_id'])
    seen = collections.Counter()
    sections = []
    for tag, body in split_sections(section_bytes(d)):
        sig = schema.get(tag) if schema else None
        toks = tokenize(body, sig)
        sections.append({'tag': tag, 'instance': seen[tag], 'bytes': len(body) + 8,
                         'tokens': toks, 'strict': sig is not None,
                         'missing_trailing': (len(sig) - len(toks)) if sig else 0})
        seen[tag] += 1
    d.pop('chunk')
    d['sections'] = sections
    return d


def decode_levels(values, count, with_phase):
    """Decode a Spectral level string (stored as an int list of byte values).

    Layout (see docs/spectral_architecture.md): `count` entries of two bytes
    (coarse, fine), then, if with_phase, `count` phase bytes. No byte is zero
    (the data is a C string), so every field is offset by one.
      dB    = ((coarse - 195) * 250 + (fine - 1)) / 500, coarse <= 1 is silent
      phase = (byte - 1) * 0.004 of a cycle
    Returns a list of (dB or None, phase or None).
    """
    b = [v & 0xff for v in values]
    need = count * (3 if with_phase else 2)
    if len(b) != need:
        raise ValueError(f'level string has {len(b)} bytes, expected {need}')
    out = []
    for i in range(count):
        coarse, fine = b[2 * i], min(b[2 * i + 1], 250)
        db = None if coarse <= 1 else ((coarse - 195) * 250 + (fine - 1)) / 500.0
        ph = None
        if with_phase:
            p = b[2 * count + i]
            ph = (min(p, 250) - 1) * 0.004
        out.append((db, ph))
    return out


def spectral_oscillators(parsed):
    """Per oscillator (SAW1 sections) of a parsed LSP1 preset: filter curve and spectra A/B."""
    oscs = []
    for s in parsed['sections']:
        if s['tag'] != 'SAW1':
            continue
        t = s['tokens']
        lists = [x for x in t if isinstance(x, list)]
        ints = [x for x in t if isinstance(x, int)]
        if len(lists) != 3:
            raise ValueError(f'SAW1#{s["instance"]}: expected 3 lists, found {len(lists)}')
        oscs.append({
            'osc': s['instance'] + 1,
            'version': ints[1] if len(ints) > 1 else None,
            'filter_db': [db for db, _ in decode_levels(lists[0], 241, False)],
            'wave_a': decode_levels(lists[1], 257, True),
            'wave_b': decode_levels(lists[2], 257, True),
            # ints: instance, Version, two undeclared slots (always 0), Wave Mix (0-100 %)
            'wave_mix': ints[4] if len(ints) > 4 else None,
        })
    return oscs


def load_fields(plugin_id):
    path = os.path.join(SCHEMA_DIR, f'{plugin_id}_fields.json')
    return json.load(open(path))['sections'] if os.path.exists(path) else {}


def _scaled(value, f):
    """Display value of a stored integer.

    Rule checked against Spectral's own parameter display (info run, 5 Nov):
    value / 10**decimals, except percent parameters declared with 2 decimals
    and a range within +-100, which display the raw value (Symmetry 50 -> 50 %).
    """
    dec = f.get('decimals')
    if not isinstance(value, int) or f.get('kind') != 'Num' or not dec:
        return value
    lo, hi = f.get('min'), f.get('max')
    if f.get('unit') == '%' and dec == 2 and lo is not None and hi is not None \
            and max(abs(lo), abs(hi)) <= 100:
        return value
    return value / 10 ** dec


def decode_step(words):
    """Decode one Spectral arpeggiator step ("Step n Data", SEQ2).

    The step is a 7-character C string (so no byte is zero) of offset bytes; the
    corpus schema sees it as three words: the string length 8, then 8 bytes.
    Meanings come from how the arpeggiator reads each byte (see
    docs/spectral_architecture.md). Bytes 1 and 2 are rewritten on every change
    with the current step and the pattern length, so they carry no step data.
    """
    if len(words) != 3 or words[0] != 8:
        raise ValueError(f'step record {words!r}: expected [8, word, word]')
    b = struct.pack('<ii', words[1], words[2])
    if len(b[:7].split(b'\0')[0]) != 7 or b[7]:
        raise ValueError(f'step record {words!r}: not a 7-character string')
    vel = b[3] - 1
    return {'current step (copy)': b[0] - 1, 'pattern length (copy)': b[1] - 1,
            'transpose': (b[2] - 61), 'velocity': vel,
            'velocity meaning': 'rest' if vel == 0 else 'hold previous note' if vel == 128
            else 'play', 'length': b[4] - 1, 'note advance': b[5] - 1, 'legato': b[6] != 1}


STEP_UNITS = {'transpose': 'semitones', 'velocity': '', 'length': '%', 'note advance': 'notes'}


def named_fields(parsed):
    """[(tag, instance, field name, value, unit)] using schemas/<id>_fields.json."""
    fields = load_fields(parsed['plugin_id'])
    rows = []
    for s in parsed['sections']:
        spec = fields.get(s['tag'], {}).get('fields')
        toks = s['tokens'][1:]  # token 0 is the instance index
        for k, tok in enumerate(toks):
            f = spec[k] if spec and k < len(spec) else {'name': f'#{k + 1}', 'kind': '?'}
            if 'step' in f:
                if f['step_word'] == 0 and k + 2 < len(toks):
                    for key, val in decode_step(toks[k:k + 3]).items():
                        if '(copy)' not in key:
                            rows.append((s['tag'], s['instance'], f"Step {f['step']} {key}",
                                         val, STEP_UNITS.get(key, '')))
                continue
            if isinstance(tok, list):
                tok = f'<{len(tok)} values>'
            rows.append((s['tag'], s['instance'], f['name'], _scaled(tok, f), f.get('unit', '')))
    return rows


def fxp_files(root):
    return sorted(os.path.join(dp, f) for dp, _, fs in os.walk(root) for f in fs
                  if f.lower().endswith('.fxp'))


def _parses(body, sig):
    try:
        tokenize(body, sig)
        return True
    except ValueError:
        return False


def learn_schemas(files):
    """Per plugin id, per tag: the token pattern that strictly parses the most bodies.

    The heuristic reading of each body proposes candidate patterns. Field kinds are
    fixed per position, so the true pattern parses every body: older saves end
    early, and an empty string ([4][0], read heuristically as two ints) parses as
    a string under it. Ties go to the longer, then the more common, pattern. With
    one candidate, or when the longest is also the most common, this is the same
    as the earlier rule (most common among the longest).
    """
    shapes = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
    bodies = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
    for f in files:
        d = read_fxp(f)
        pid = d['plugin_id']
        for tag, body in split_sections(section_bytes(d)):
            shapes[pid][tag][signature(tokenize(body))] += 1
            bodies[pid][tag][body] += 1
    schemas = {}
    for pid, tags in shapes.items():
        schemas[pid] = {}
        for tag, c in tags.items():
            if len(c) == 1:
                schemas[pid][tag] = next(iter(c))
                continue
            def score(sig):
                ok = sum(n for b, n in bodies[pid][tag].items() if _parses(b, sig))
                return ok, len(sig), c[sig]
            best = max(c, key=score)
            schemas[pid][tag] = _refine(best, list(bodies[pid][tag]))
    return schemas, shapes


def _offsets(body, sig):
    """Byte offset of each token when body is read with sig, as far as it reads."""
    out, p = [], 0
    for kind in sig:
        if p >= len(body):
            break
        out.append(p)
        if kind == 's':
            st = _string_at(body, p, strict=True)
            if not st:
                break
            p += 4 + st[0]
        elif kind == 'l':
            lst = _list_at(body, p)
            if lst is None:
                break
            p += 4 + 4 * lst[0]
        else:
            p += 4
    return out


def _refine(sig, bodies):
    """Upgrade int pairs to strings where some body holds real text there.

    A field that is an empty string in most presets reads heuristically as two
    ints ([4][0]), so the chosen pattern can say 'ii' where a few presets store a
    path; a newer save can also append fields. For each body the pattern cannot
    parse, repeatedly take the first offset where the body's own heuristic
    reading has a string but the pattern has an int pair, and make it 's'; when
    the whole pattern reads with bytes to spare, append the rest as read. Keep
    the result only if no body that parsed before stops parsing.
    """
    def heuristic_strings(body):
        pos, p = set(), 0
        for t in tokenize(body):
            if isinstance(t, str):
                pos.add(p)
            p += 4 + (len(t.encode('latin1')) + 4) // 4 * 4 if isinstance(t, str) else \
                4 + 4 * len(t) if isinstance(t, list) else 4
        return pos

    for body in bodies:
        if _parses(body, sig):
            continue
        strings = heuristic_strings(body)
        trial = sig
        for _ in range(256):
            if _parses(body, trial):
                break
            offs = _offsets(body, trial)
            k = next((k for k, off in enumerate(offs) if off in strings and trial[k] == 'i'
                      and trial[k + 1:k + 2] == 'i'), None)
            if k is not None:
                trial = trial[:k] + 's' + trial[k + 2:]
            elif len(offs) == len(trial):
                end = _offsets(body, trial + 'i')[-1]
                trial += signature(tokenize(body[end:]))
            else:
                break
        if trial != sig and _parses(body, trial) and \
                all(_parses(b, trial) for b in bodies if _parses(b, sig)):
            sig = trial
    return sig


def verify(roots, write=False):
    files = [f for r in roots for f in fxp_files(r)]
    root = os.path.commonpath(roots) if len(roots) > 1 else roots[0]
    by_id = collections.defaultdict(list)
    unreadable = []
    for f in files:
        try:
            by_id[read_fxp(f)['plugin_id']].append(f)
        except ValueError as e:
            unreadable.append((f, str(e)))
    print(f'presets: {len(files)}  unreadable headers: {len(unreadable)}')
    schemas, shapes = learn_schemas([f for fs in by_id.values() for f in fs])
    ok_all = not unreadable
    for pid, fs in sorted(by_id.items()):
        layouts = collections.Counter()
        short = collections.Counter()
        failures = []
        for f in fs:
            try:
                d = parse(f, schemas[pid])
            except ValueError as e:
                failures.append((f, str(e)))
                continue
            layouts[' '.join(s['tag'] for s in d['sections'])] += 1
            for s in d['sections']:
                if s['missing_trailing']:
                    short[(s['tag'], s['missing_trailing'])] += 1
        unstable = {t: c for t, c in shapes[pid].items() if len(c) > 1}
        print(f'\n[{pid}] {len(fs)} presets, {len(schemas[pid])} section tags')
        print(f'  heuristic disagreements resolved by the schema: {len(unstable)} tags')
        print(f'  strict parse: {len(fs) - len(failures)} ok, {len(failures)} failed')
        for f, e in failures[:10]:
            print('    FAIL', os.path.relpath(f, root), e)
        for (tag, n), v in sorted(short.items()):
            print(f'  older layout: {v} sections of {tag} lack their last {n} int(s)')
        print(f'  distinct section orders: {len(layouts)}')
        ok_all &= not failures
        if write and not failures:
            os.makedirs(SCHEMA_DIR, exist_ok=True)
            out = os.path.join(SCHEMA_DIR, f'{pid}.json')
            with open(out, 'w') as fh:
                json.dump(schemas[pid], fh, indent=1, sort_keys=True)
            print('  schema written to', out)
    return ok_all


def encode_body(tokens):
    """Inverse of tokenize: section body bytes from its tokens."""
    out = bytearray()
    for t in tokens:
        if isinstance(t, ZeroLenStr):
            out += struct.pack('<i', 0)
        elif isinstance(t, str):
            raw = t.encode('latin1')
            n = (len(raw) + 4) // 4 * 4
            out += struct.pack('<i', n) + raw + b'\0' * (n - len(raw))
        elif isinstance(t, list):
            out += struct.pack(f'<i{len(t)}i', LIST_BASE + len(t), *t)
        else:
            out += struct.pack('<i', t)
    return bytes(out)


def encode_sections(sections):
    out = bytearray()
    for s in sections:
        body = encode_body(s['tokens'])
        out += s['tag'][::-1].encode('latin1') + struct.pack('<I', len(body) + 8) + body
    return bytes(out)


def pack_runs(raw):
    """Our own encoder for the run-length format that unpack_runs reads.

    Any valid encoding expands to the same bytes. This one is simple and greedy:
    runs of 3 or more equal bytes, then whole-word patterns repeated at least
    twice when that is shorter, otherwise literals. It does not try to match
    Spectral's own choices, so packed sizes differ slightly from the originals.
    """
    if len(raw) > 0xffff:
        raise ValueError('chunk too large for the 16-bit size header')
    out = bytearray([len(raw) & 0xff, len(raw) >> 8])
    lit = bytearray()

    def flush():
        for i in range(0, len(lit), 128):
            seg = lit[i:i + 128]
            out.append(len(seg) - 1)
            out.extend(seg)
        lit.clear()

    p = 0
    while p < len(raw):
        b = raw[p]
        run = 1
        while p + run < len(raw) and raw[p + run] == b and run < 33:
            run += 1
        if run >= 3:
            flush()
            out += bytes([0x80 | run - 2]) if b == 0 else bytes([0xa0 | run - 2]) if b == 1 \
                else bytes([0xc0 | run - 2, b])
            p += run
            continue
        best = None
        for plen in (4, 8, 12, 16, 20, 24, 28, 32):
            pat = raw[p:p + plen]
            if len(pat) < plen:
                break
            n = 1
            while n < 33 and raw[p + n * plen:p + (n + 1) * plen] == pat:
                n += 1
            saved = n * plen - (2 + plen)
            if n >= 2 and saved > 0 and (best is None or saved > best[2]):
                best = (plen, n, saved)
        if best:
            flush()
            plen, n, _ = best
            out += bytes([0xe0 | n - 2, plen]) + raw[p:p + plen]
            p += n * plen
            continue
        lit.append(b)
        p += 1
    flush()
    return bytes(out)


def build_fxp(d, chunk, packed=True):
    """FXP bytes for header fields from read_fxp/parse and a section chunk."""
    if packed and d['plugin_id'] in PACKED_PLUGINS:
        chunk = pack_runs(chunk)
    if 'name_field' in d:  # keep the original 28 bytes, including anything after the NUL
        name = bytes.fromhex(d['name_field'])
    else:
        name = d['name'].encode('latin1')[:27]
        name += b'\0' * (28 - len(name))
    head = struct.pack('>4sI4sI4sII', b'CcnK', 0, b'FPCh', d.get('fxp_version', 1),
                       d['plugin_id'].encode('latin1'), d['fx_version'], d['num_params'])
    head += name + struct.pack('>I', len(chunk))
    b = bytearray(head + chunk)
    # Keep LinPlug's 0 where the original had it; otherwise write the true size.
    struct.pack_into('>I', b, 4, 0 if d.get('size_field') == 0 else len(b) - 8)
    return bytes(b)


def roundtrip(roots):
    """Re-encode every preset from its parsed tokens. Proves the token model loses nothing.

    Checks per preset: (1) the re-encoded section chunk equals the original
    (unpacked) chunk byte for byte; (2) for packed plugins, our packing expands
    back to the same chunk; (3) the rebuilt file equals the original file, which
    is expected only where the chunk is not packed (packing choices differ).
    """
    files = [f for r in roots for f in fxp_files(r)]
    stats = collections.defaultdict(collections.Counter)
    bad = []
    for f in files:
        orig = open(f, 'rb').read()
        head = read_fxp(f)
        pid = head['plugin_id']
        raw = section_bytes(head)
        d = parse(f)
        chunk = encode_sections(d['sections'])
        st = stats[pid]
        st['presets'] += 1
        if chunk != raw:
            bad.append((f, 'section chunk differs'))
            continue
        st['chunk exact'] += 1
        if pid in PACKED_PLUGINS:
            ours = pack_runs(chunk)
            if unpack_runs(ours) != (chunk, 0):
                bad.append((f, 'our packing does not expand back'))
                continue
            st['repack expands back'] += 1
            st['original packed bytes'] += len(head['chunk'])
            st['our packed bytes'] += len(ours)
        rebuilt = build_fxp(d, chunk) + orig[len(orig) - head['trailing_bytes']:] \
            if head['trailing_bytes'] else build_fxp(d, chunk)
        if rebuilt == orig:
            st['file exact'] += 1
        elif pid not in PACKED_PLUGINS:
            bad.append((f, 'rebuilt file differs'))
    for pid, st in sorted(stats.items()):
        print(f'[{pid}] ' + ', '.join(f'{k}: {v}' for k, v in st.items()))
    for f, why in bad[:20]:
        print('  FAIL', f, why)
    print(f'failures: {len(bad)}')
    return not bad


def _fmt(t):
    if isinstance(t, str):
        return repr(t)
    if isinstance(t, list):
        head = ' '.join(map(str, t[:12]))
        return f'[{len(t)}: {head}{" …" if len(t) > 12 else ""}]'
    return str(t)


def main(argv):
    if len(argv) < 3 or argv[1] not in ('dump', 'json', 'verify', 'fields', 'roundtrip'):
        print(__doc__)
        return 2
    cmd, target = argv[1], argv[2]
    if cmd == 'verify':
        roots = [a for a in argv[2:] if not a.startswith('--')]
        return 0 if verify(roots, '--write-schemas' in argv[3:]) else 1
    if cmd == 'roundtrip':
        return 0 if roundtrip(argv[2:]) else 1
    d = parse(target)
    if cmd == 'fields':
        as_json = '--json' in argv[3:]
        want = {a for a in argv[3:] if not a.startswith('--')}
        rows = [r for r in named_fields(d) if not want or r[0] in want]
        if as_json:
            print(json.dumps({'name': d['name'], 'plugin_id': d['plugin_id'], 'fields': [
                {'section': t, 'instance': i, 'name': n,
                 'value': v.strip() if isinstance(v, str) else v, 'unit': u}
                for t, i, n, v, u in rows]}, indent=1))
            return 0
        for tag, inst, name, value, unit in rows:
            print(f'{tag}#{inst:<2} {name:<44} {value!r} {unit}'.rstrip())
        return 0
    if cmd == 'json':
        print(json.dumps(d, indent=1))
        return 0
    want = set(argv[3:])
    print(f"{d['name']!r}  plugin={d['plugin_id']}  fxver={d['fx_version']}")
    for s in d['sections']:
        if want and s['tag'] not in want:
            continue
        print(f"[{s['tag']}#{s['instance']}] {s['bytes']}B  " + ' '.join(_fmt(t) for t in s['tokens']))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
