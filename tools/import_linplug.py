#!/usr/bin/env python3
"""Convert a LinPlug Spectral preset (.fxp) into a partials patch (.ptch).

Uses the preset reader in reference/tools (our own code, written from the file
format). Maps the fields M1 uses; everything else is listed in a comment block at the end of
the patch, so nothing is silently dropped.

Usage:
  python3 -I import_linplug.py PRESET.fxp OUT.ptch [--linplug-tools DIR]
  python3 -I import_linplug.py --batch DIR_OF_FXP OUT_DIR [--linplug-tools DIR]

Value conventions (see ../reverse-engineer/docs/spectral_architecture.md):
  Volume parameters are stored in 0.2 dB steps (stored -30 = -6.0 dB, as Spectral displays).
  Percentages become 0..1; Pan becomes -1..1; LP Offset stays in semitones (61 = off).
  Envelopes ME05 #0-3 are Amp 1-4, #4-7 Cut 1-4, #8-10 Mod 1-3, #11 Main (host order).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TOOLS = os.path.normpath(os.path.join(HERE, '..', 'reference', 'tools'))
LP_TYPES = {'LP+ 6': 1, 'LP+12': 2, 'LP+18': 3, 'LP+24': 4}
OSC_XMOD = {'AM': 0, 'FM': 1, 'PM': 2}
FIL_XMOD = {'FM': 0, 'RM': 1}


def xmod_source(text_value):
    """'OSC 2' -> 1, 'FIL 3' -> 6 (older presets also write 'Osc 1')."""
    t = str(text_value).strip().upper()
    try:
        n = int(t.split()[-1]) - 1
    except (ValueError, IndexError):
        return 0
    return max(0, min(3, n)) + (4 if t.startswith('FIL') else 0)


LFO_WAVES = ['Sine', 'Triangle', 'Sawtooth', 'Pulse', 'Impuls', 'Peak', 'Noise', 'Sample&Hold', 'RandomPulse']
LFO_POLAR = ['NORMAL', 'UNI +', 'UNI -', 'CUT +', 'CUT -']
LFO_FREE = ['Phase', 'Free', 'Pha/M']


def slug(text):
    return '_'.join(str(text).replace('$', ' ').replace('-', ' ').replace('/', ' ').lower().split()) or 'none'


def matrix_source(text_value):
    t = ' '.join(str(text_value).split())
    low = t.lower()
    if low in ('- - -', ''):
        return 'none'
    for prefix, key in (('lfo ', 'lfo'), ('midi cc ', 'cc')):
        if low.startswith(prefix) and low[len(prefix):].isdigit():
            return f'{key}{low[len(prefix):]}'
    for kind in ('mod', 'amp', 'cut'):
        if low.startswith(kind + ' ') and low.endswith(' envelope') and low.split()[1].isdigit():
            return f'{kind}env{low.split()[1]}'
    named = {'mod wheel': 'modwheel', 'aftertouch': 'aftertouch', 'velocity': 'velocity', 'note': 'note',
             'pitch wheel +': 'pitchwheel+', 'pitch wheel -': 'pitchwheel-', 'arp velocity': 'arp_velocity',
             'arp length': 'arp_length', 'arp transpose': 'arp_transpose', 'alternate': 'alternate',
             'constant': 'constant', 'random bipolar': 'random_bi', 'random unipolar': 'random_uni',
             'main envelope': 'mainenv', 'glide curve': 'glide'}
    return named.get(low, 'raw_' + slug(t))


def matrix_dest(text_value):
    t = str(text_value)
    if '- - -' in t or not t.strip():
        return 'none'
    left, _, right = t.partition('$')
    left, right = ' '.join(left.split()).lower(), ' '.join(right.split()).lower()
    words = left.split()
    if len(words) == 2 and words[0] in ('osc', 'filter') and words[1].isdigit():
        kind = {'frequency': 'cutoff', 'pitch': 'pitch', 'symmetry': 'symmetry', 'wave mix': 'wavemix',
                'volume': 'volume', 'pan': 'pan', 'resonance': 'resonance', 'env depth': 'envdepth',
                'env-depth': 'envdepth',
                'xmod amt 1': 'xmod1', 'xmod amt 2': 'xmod2'}.get(right)
        if kind:
            return f'{words[0]}{words[1]}.{kind}'
    if left == 'main' and right in ('volume', 'pitch'):
        return f'main.{right}'
    if left == 'matrix' and right.startswith('depth ') and right[6:].isdigit():
        return f'matrix.depth{right[6:]}'
    if left == 'lfo speed' and right.startswith('lfo ') and right[4:].isdigit():
        return f'lfo{right[4:]}.speed'
    return 'raw_' + slug(t)


VOICES = {'2': (2, 0), '3': (3, 0), '4a': (4, 0), '4b': (4, 1), '5': (5, 0), '6': (6, 0)}
MULTIMODE = {'LP12': 0, 'HP12': 1, 'BP12': 2, 'BR12': 3}
FX_TYPES = {'REVERB': 'reverb', 'DELAY (MOD)': 'delay_mod', 'DELAY STEREO': 'delay_stereo', 'STEREO DELAY': 'delay_stereo',
            'DELAY PING PONG': 'delay_pingpong', 'CHORUS 1': 'chorus', 'CHORUS 2': 'chorus', 'PHASER': 'phaser',
            'FLANGER': 'flanger', 'FILTER': 'filter', 'COMPRESSOR': 'compressor', 'GATOR': 'gator',
            'CRUSHER': 'crusher', 'EQ (PARAM.)': 'eq', 'WAH-WAH': 'wah'}
# Effect sections in a preset, in slot order (the master EQ VEQ4 and limiter VL01 are not slots).
FX_TAG_PREFIXES = ('VCH', 'VCP', 'VCX', 'VDM', 'VDP', 'VDS', 'VFL', 'VGT', 'VGF', 'VPH', 'VR0', 'VWW', 'VEQ3', 'VEQ5')
# "CutLo" / "CutHi" in the delays are the low-pass and high-pass corners (inferred: CutLo
# ranges to 20,480 Hz and sits high in presets; CutHi ranges to 11,000 or 2,750 Hz and sits low).
# Spectral field name -> (our key, how): 'pct' = percent to 0..1 where the field is shown in
# percent, 'beats' = tempo-sync text to beats, 'flag' = on/off, 'num' = as shown.
FX_FIELDS = {
    'reverb': [('RevTime', 'time_s', 'num'), ('Damping', 'damping', 'pct'), ('Predelay', 'predelay_ms', 'num'),
               ('Frequency', 'predelay_beats', 'beats'), ('Modulation On/Off', 'modulation', 'flag'),
               ('Diffusors', 'diffusors', 'num'), ('Mod Depth', 'mod_depth', 'num')],
    'delay_stereo': [('TimeL', 'time_l_ms', 'num'), ('SyncL', 'sync_l_beats', 'beats'), ('TimeR', 'time_r_ms', 'num'),
                     ('SyncR', 'sync_r_beats', 'beats'), ('Feedback', 'feedback', 'pct'), ('CutLo', 'lowpass_hz', 'num'),
                     ('CutHi', 'highpass_hz', 'num'), ('Stereo', 'stereo', 'pct'), ('Crossdelay', 'cross', 'flag'),
                     ('Filters in Feedback', 'filters_in_feedback', 'flag')],
    'delay_mod': [('Time', 'time_ms', 'num'), ('Feed', 'feedback', 'pct'), ('ModRate', 'mod_rate_hz', 'num'),
                  ('ModDept', 'mod_depth', 'num'), ('Delay Sync', 'sync_beats', 'beats'),
                  ('Mod Speed Sync', 'mod_sync_beats', 'beats'), ('Stereo', 'stereo', 'pct'),
                  ('CutLo', 'lowpass_hz', 'num'), ('CutHi', 'highpass_hz', 'num')],
    'delay_pingpong': [('Time Center', 'time_c_ms', 'num'), ('Sync Center', 'sync_c_beats', 'beats'),
                       ('Time Left', 'time_l_ms', 'num'), ('Sync Left', 'sync_l_beats', 'beats'),
                       ('Pan Left', 'pan_l', 'pan'), ('Time Right', 'time_r_ms', 'num'),
                       ('Sync Right', 'sync_r_beats', 'beats'), ('Pan Right', 'pan_r', 'pan'),
                       ('Feedback L+R', 'feedback', 'pct'), ('Balance Pre/LR', 'balance', 'pct')],
    'chorus': [('Chorus Time', 'time_ms', 'num'), ('Chorus Speed', 'rate_hz', 'num'),
               ('Chorus Stereo', 'stereo', 'pct'), ('Invert Signal', 'invert', 'flag'),
               # the trailing later-version field looks like a 0..100 % value; read as Wet (inferred)
               ('unknown (later version)', 'wet', 'pct')],
    'phaser': [('Phaser Speed', 'rate_hz', 'num'), ('Phaser Center', 'center_hz', 'num'), ('Phaser Depth', 'depth', 'pct'),
               ('Phaser Feedb', 'feedback', 'pct'), ('Phaser FBInv', 'fb_invert', 'flag'), ('Phaser Stages', 'stages', 'num'),
               ('Phaser Stereo', 'stereo', 'pct'), ('Sync', 'sync_beats', 'beats')],
    'flanger': [('Time', 'time_ms', 'num'), ('Feed', 'feedback', 'pct'), ('Invert Feed+Wet', 'invert', 'flag'),
                ('Stereo', 'stereo', 'pct'), ('ModRate', 'rate_hz', 'num'), ('Frequency', 'sync_beats', 'beats'),
                ('Time Depth', 'time_depth', 'pct'), ('Pan Depth', 'pan_depth', 'pct')],
    'filter': [('Type', 'type', MULTIMODE), ('Filter Sat', 'saturation', 'pct'), ('Cutoff', 'cutoff_hz', 'num'),
               ('Resonance', 'resonance', 'pct'), ('Filter Mod Speed', 'mod_rate_hz', 'num'),
               ('Filter Mod Depth', 'mod_depth', 'pct'), ('Level (Wet only)', 'level_db', 'vol'),
               ('Frequency', 'sync_beats', 'beats')],
    # Attack and Release are declared in seconds but hold milliseconds (presets: 50.0, 500).
    'compressor': [('Ratio', 'ratio', 'num'), ('Threshold', 'threshold_db', 'num'), ('Release', 'release_ms', 'num'),
                   ('Attack', 'attack_ms', 'num'), ('Out Vol.', 'out_db', 'vol')],
    # Older saves have no Divider; their "Unused" field (1..32, equal to Divider where both are
    # written at conversion) holds it.
    'crusher': [('Resolution', 'bits', 'num'), ('Dither On/Off', 'dither', 'flag'),
                ('Divider Mode', 'divider_mode', {'AVG': 0, 'STP': 1, 'SLD': 2}), ('Filter Type', 'filter_type', MULTIMODE),
                ('Cutoff', 'cutoff_hz', 'num'), ('Resonance', 'resonance', 'pct'),
                ('FilterPostDiv On/Off', 'filter_post', 'flag'), ('Auto Volume compensation', 'auto_volume', 'flag'),
                (('Divider', 'Unused'), 'divider', 'num')],
    'gator': [('Step On/Off [left]', 'pattern_l', 'pattern'), ('Step On/Off [right]', 'pattern_r', 'pattern'),
              ('Frequency', 'step_beats', 'beats'), ('Smooth', 'smooth', 'num'),
              ('Link', 'link', {'OFF': 0, 'LINK': 1, 'XLINK': 2})],
    'eq': [('Freq [band 1]', 'freq1_hz', 'num'), ('Gain [band 1]', 'gain1_db', 'num'), ('Q [band 1]', 'q1', 'num'),
           ('Freq [band 2]', 'freq2_hz', 'num'), ('Gain [band 2]', 'gain2_db', 'num'), ('Q [band 2]', 'q2', 'num'),
           ('Freq Lo', 'low_hz', 'num'), ('Gain Lo [low shelf]', 'low_db', 'vol'), ('Freq Hi', 'high_hz', 'num'),
           ('Gain Lo [high shelf (declared as Gain Lo)]', 'high_db', 'vol')],
    # Level (D+W) is declared as a volume but holds a linear percentage (presets: 100, 41, 492;
    # inferred). Sensitivity is signed (presets: -0.03 .. 0.03).
    'wah': [('Cutoff', 'cutoff_hz', 'num'), ('Resonance', 'resonance', 'pct'), ('Sensivety', 'sensitivity', 'num'),
            ('Level (D+W)', 'level', 'lin'), ('Attack', 'attack_s', 'num'), ('Release', 'release_s', 'num')],
}
# Wet. Version-5 sections write an old 0..200 % field and a newer 0..100 % one; where both are
# present the newer one is used. Re-saved presets show the newer value as half the older one
# at conversion (200 -> 100, 50 -> 25, 158 -> 79), so a section that predates the newer field
# reads the older one divided by 200 (inferred from the corpus, 8 November). Chorus keeps its
# own rule above.
WET_FIELDS = ('Wet', 'Filter Wet', 'Unused Wet', 'Phaser Wet')


def pattern_bits(text):
    """'1010...' (16 steps, first step first) -> integer with step 1 in bit 15."""
    t = str(text).strip()[:16].ljust(16, '0')
    return sum(1 << (15 - i) for i, ch in enumerate(t) if ch == '1')


def sync_beats(text):
    """Tempo-sync text such as ' 1/8 ', '1/16*' (dotted) or '1/8T' (triplet) to beats; 0 = off."""
    t = str(text).strip()
    mult = 1.0
    if t.endswith('*'):
        t, mult = t[:-1], 1.5
    elif t.endswith('T'):
        t, mult = t[:-1], 2.0 / 3.0
    try:
        a, b = t.split('/')
        return 4.0 * float(a) / float(b) * mult
    except ValueError:
        return 0.0


ENV_KEYS = [('A Time', 'attack', 1), ('H Time', 'hold', 1), ('D Time', 'decay', 1),
            ('S Level', 'sustain', 100), ('F Time', 'fade', 1), ('R Time', 'release', 1),
            ('A c', 'attack_curve', 1), ('D c', 'decay_curve', 1), ('F c', 'fade_curve', 1),
            ('R c', 'release_curve', 1)]


def load_reader(tools):
    sys.path.insert(0, tools)
    import linplug_fxp  # type: ignore  # our own reader, found on the path given above
    return linplug_fxp


def fmt(v):
    return f'{v:.6g}'


def dbs(values):
    return ' '.join('x' if v is None else fmt(v) for v in values)


def convert(L, path):
    parsed = L.parse(path)
    if parsed['plugin_id'] != 'LSP1':
        raise ValueError(f"{path}: plugin {parsed['plugin_id']}, not Spectral (LSP1)")
    rows = L.named_fields(parsed)
    val = {(t, i, n): v for t, i, n, v, _u in rows}
    used = set()

    def get(tag, inst, name, default=None):
        k = (tag, inst, name)
        if k not in val:
            return default
        used.add(k)
        v = val[k]
        return v.strip() if isinstance(v, str) else v

    def num(tag, inst, name, default):
        v = get(tag, inst, name)
        return float(v) if isinstance(v, (int, float)) else float(default)

    def text(tag, inst, name, default):
        v = get(tag, inst, name)
        return v if isinstance(v, str) else default

    fields = L.load_fields('LSP1')

    def shown_max(tag, name):
        for f in fields.get(tag, {}).get('fields', []):
            if f['name'] == name and isinstance(f.get('max'), int):
                return L._scaled(f['max'], f)
        return None

    out = ['partials-patch 1', f"# imported from {os.path.basename(path)} (LinPlug Spectral preset)",
           f"name {parsed['name']}"]
    out.append(f"main.volume_db {fmt(num('SYG3', 0, 'Main Vol.', -30) / 5)}")
    oscs = L.spectral_oscillators(parsed)
    for o in range(1, 5):
        def g(n, d, o=o):
            return num('GOS0', 0, f'{n} [osc {o}]', d)
        p = f'osc.{o}'
        out += [f'{p}.on {int(bool(g("Osc On/Off", 0)))}',
                f'{p}.semitone {int(g("Osc Semitone", 0))}',
                f'{p}.cent {fmt(g("Osc Cent", 0))}',
                f'{p}.key_track {int(bool(g("Osc Track", 1)))}',
                f'{p}.symmetry {fmt(g("Osc Symmetry", 50) / 100)}',
                f'{p}.start_phase {fmt(g("Osc Phase", 0) / 100)}',
                f'{p}.free_run {int(bool(g("osc FreeRun", 0)))}',
                f'{p}.volume_db {fmt(g("Osc Volume", 0) / 5)}']
        voices, layout = VOICES.get(text('GOS0', 0, f'Osc Voices/Mode [osc {o}]', '3').strip(), (3, 0))
        out += [f'{p}.unison_voices {voices}', f'{p}.unison_layout {layout}',
                f'{p}.unison_detune_cents {fmt(g("Osc Spread", 0))}',
                f'{p}.unison_volume {fmt(g("Osc Spread Vol", 100) / 100)}',
                f'{p}.unison_stereo {fmt(g("Osc Stereo", 50) / 100)}']
        for x in (1, 2):
            kind = OSC_XMOD.get(text('GOS0', 0, f'Osc XMod {x} Type [osc {o}]', 'AM').strip().upper(), 0)
            src = xmod_source(text('GOS0', 0, f'Osx XMod {x} Src [osc {o}]', 'OSC 1'))
            out += [f'{p}.xmod{x}.type {kind}', f'{p}.xmod{x}.source {src}',
                    f'{p}.xmod{x}.amount {fmt(g(f"Osc XMod {x} Amt", 0) / 100)}']
            kind = FIL_XMOD.get(text('GOS0', 0, f'Fil XMod {x} Type [osc {o}]', 'FM').strip().upper(), 0)
            src = xmod_source(text('GOS0', 0, f'Fil XMod {x} Src [osc {o}]', 'OSC 1'))
            out += [f'{p}.filter.xmod{x}.type {kind}', f'{p}.filter.xmod{x}.source {src}',
                    f'{p}.filter.xmod{x}.amount {fmt(g(f"Fil XMod {x} Amt", 0) / 100)}']
        for r in (1, 2):
            dest = text('GOS0', 0, f'Fil Route {r} [osc {o}]', 'OUT').strip().upper()
            out.append(f'{p}.route{r} {int(dest[3:]) if dest.startswith("FX ") and dest[3:].isdigit() else 0}')
        out.append(f'{p}.route_balance {fmt(g("Fil Route Balance", -100) / 100)}')
        sp = oscs[o - 1] if o - 1 < len(oscs) else None
        if sp:
            for k in (('SAW1', o - 1, n) for n in ('Wave Mix', 'Filter', 'Wave A', 'Wave B')):
                used.add(k)
            mix = sp['wave_mix'] if sp['wave_mix'] is not None else 50
            out.append(f'{p}.wave_mix {fmt(mix / 100)}')
            for w, key in (('a', 'wave_a'), ('b', 'wave_b')):
                out.append(f'{p}.wave_{w}.db {dbs(db for db, _ in sp[key])}')
                out.append(f'{p}.wave_{w}.phase {" ".join(fmt(ph or 0) for _, ph in sp[key])}')
        f = f'{p}.filter'
        out += [f'{f}.on {int(bool(g("Fil On/Bypass", 0)))}',
                f'{f}.cutoff_hz {fmt(g("Fil Freq", 20480))}',
                f'{f}.key_track {fmt(g("Fil Pit Track", 0) / 100)}',
                f'{f}.env_depth {fmt(g("Fil Env Depth", 0) / 100)}',
                f'{f}.vel_track {fmt(g("Fil Vel Track", 0) / 100)}',
                f'{f}.resonance {fmt(g("Fil Resonance", 0) / 100)}',
                f'{f}.lp_offset {fmt(g("LP Offset", 61))}',
                f'{f}.lp_type {LP_TYPES.get(text("GOS0", 0, f"LP Type [osc {o}]", "LP+18"), 3)}',
                # Fil Volume is declared -54..+12 dB with an "off" flag; presets store values
                # just below -54 dB, which we read as off (inferred).
                f'{f}.volume_db {fmt(fil_db) if (fil_db := g("Fil Volume", -30) / 5) >= -54 else -200}',
                f'{f}.pan {fmt(g("Fil Pan", 0) / 100)}']
        if sp:
            out.append(f'{f}.curve_db {dbs(sp["filter_db"])}')
    names = [f'amp.{i}' for i in range(1, 5)] + [f'cut.{i}' for i in range(1, 5)] + \
            [f'mod.{i}' for i in range(1, 4)] + ['main']
    for inst, nm in enumerate(names):
        if ('ME05', inst, 'Version') not in val:
            continue
        for src, dst, div in ENV_KEYS:
            if ('ME05', inst, src) in val:
                out.append(f'env.{nm}.{dst} {fmt(num("ME05", inst, src, 0) / div)}')
        out.append(f"env.{nm}.ignore_note_off {int(bool(num('ME05', inst, 'Ignore MIDI Note Off', 0)))}")

    fx_sections = [s for s in parsed['sections'] if s['tag'].startswith(FX_TAG_PREFIXES)]
    for slot in range(1, 7):
        kind = FX_TYPES.get(text('SYG3', 0, f'Fx Type [slot {slot}]', '').strip().upper(), 'none')
        on = int(bool(num('SYG3', 0, f'Fx On [slot {slot}]', 0)))
        dest = text('SYG3', 0, f'Fx Route [slot {slot}]', 'OUT').strip().upper()
        route = int(dest[3:]) if dest.startswith('FX ') and dest[3:].isdigit() else 0
        get('SYG3', 0, f'unused, former Fx Wet [slot {slot}]')
        out += [f'fx.{slot}.type {kind}', f'fx.{slot}.on {on}', f'fx.{slot}.route {route}']
        if slot - 1 >= len(fx_sections):
            continue
        sec = fx_sections[slot - 1]
        tag, inst = sec['tag'], sec['instance']
        named = [(n, v) for t, i, n, v, _u in rows if t == tag and i == inst]
        for src, key, how in FX_FIELDS.get(kind, []):
            hits = []
            for alt in (src if isinstance(src, tuple) else (src,)):
                hits = [v for n, v in named if n.strip() == alt]
                if hits:
                    src = alt
                    break
            if not hits:
                continue
            v = hits[-1] if key == 'stereo' else hits[0]  # later duplicates replace retired fields
            used.add((tag, inst, src))
            if how == 'beats':
                out.append(f'fx.{slot}.p.{key} {fmt(sync_beats(v))}')
            elif how == 'pattern':
                out.append(f'fx.{slot}.p.{key} {pattern_bits(v)}')
            elif isinstance(how, dict):
                out.append(f'fx.{slot}.p.{key} {how.get(str(v).strip().upper(), 0)}')
            elif isinstance(v, (int, float)):
                if how == 'pct':
                    mx = shown_max(tag, src)
                    v = v / 100 if mx is None or mx > 1.5 else v
                elif how in ('pan', 'lin'):
                    v = v / 100
                elif how == 'vol':
                    v = v / 5  # 0.2 dB steps
                elif how == 'flag':
                    v = 1 if v else 0
                out.append(f'fx.{slot}.p.{key} {fmt(v)}')
        spec = fields.get(tag, {}).get('fields', [])
        wets = [(k, n.strip(), v) for k, (n, v) in enumerate(named)
                if n.strip() in WET_FIELDS and isinstance(v, (int, float))]
        if kind != 'chorus' and wets:
            k, n, v = wets[-1]
            mx = spec[k].get('max') if k < len(spec) else None
            used.add((tag, inst, n))
            out.append(f'fx.{slot}.p.wet {fmt(v / (200 if mx == 200 else 100))}')

    for inst in range(5):  # LFOs: fields read in order (two are both named "Frequency")
        rowsl = [(n.strip(), v) for t, i, n, v, _u in rows if t == 'ML05' and i == inst]
        if not rowsl:
            continue
        for n, _v in rowsl:
            used.add(('ML05', inst, n))
        k = f'lfo.{inst + 1}'
        freqs = [v for n, v in rowsl if n == 'Frequency']
        first = {}
        for n, v in rowsl:
            first.setdefault(n, v)
        wave = str(first.get('Waveform', 'Sine')).strip()
        out += [f'{k}.wave {LFO_WAVES.index(wave) if wave in LFO_WAVES else 0}',
                f'{k}.rate_hz {fmt(freqs[0] if freqs and isinstance(freqs[0], (int, float)) else 1)}',
                f'{k}.sync_beats {fmt(sync_beats(freqs[1]) if len(freqs) > 1 else 0)}',
                f'{k}.delay {fmt(first.get("Delay", 0) or 0)}', f'{k}.attack {fmt(first.get("Attack", 0) or 0)}',
                f'{k}.symmetry {fmt((first.get("Symmetry", 50) or 50) / 100)}',
                f'{k}.phase {fmt((first.get("Phase", 0) or 0) / 100)}']
        pol = str(first.get('Polar', 'NORMAL')).strip()
        free = str(first.get('FreeRun', 'Phase')).strip()
        out += [f'{k}.polarity {LFO_POLAR.index(pol) if pol in LFO_POLAR else 0}',
                f'{k}.smooth {fmt((first.get("Smooth", 1) or 1) / 100)}',
                f'{k}.free_run {LFO_FREE.index(free) if free in LFO_FREE else 0}']
    for slot in range(1, 25):
        src = matrix_source(text('PMX0', 0, f'Source {slot}', '- - -'))
        dst = matrix_dest(text('PMX0', 0, f'Destination {slot}', '- - -'))
        amt = num('PMX0', 0, f'Amount {slot}', 0) / 100
        # Zero-base rows can be activated by another row targeting Matrix Depth.
        if src != 'none' and dst != 'none':
            out += [f'matrix.{slot}.source {src}', f'matrix.{slot}.dest {dst}', f'matrix.{slot}.amount {fmt(amt)}']

    modes = ['OFF', 'MOD-ONLY', 'UP', 'DOWN', 'UP/DOWN', 'UP/DOWN+', 'DOWN/UP', 'DOWN/UP+', 'RANDOM', 'CHORD', 'ASPLAYED']
    mode = text('SEQ2', 0, 'Op.Mode', 'OFF').strip().upper()
    out += [f'arp.mode {modes.index(mode) if mode in modes else 0}',
            f"arp.resolution_beats {fmt(sync_beats(text('SEQ2', 0, 'Resolution', '1/16')) or 0.25)}",
            f"arp.length {int(num('SEQ2', 0, 'Length', 16))}", f"arp.octaves {int(num('SEQ2', 0, 'Octaves', 1))}",
            f"arp.swing {fmt(num('SEQ2', 0, 'Swing', 0) / 100)}",
            f"arp.velo_blend {fmt(num('SEQ2', 0, 'Velo Blend', 0) / 100)}",
            f"arp.note_length {fmt(num('SEQ2', 0, 'Note Length', 50) / 100)}"]
    for st in range(1, 33):
        tr = get('SEQ2', 0, f'Step {st} transpose')
        if tr is None:
            continue
        for k in ('velocity meaning',):
            get('SEQ2', 0, f'Step {st} {k}')
        out += [f'arp.step.{st}.transpose {int(tr)}',
                f"arp.step.{st}.velocity {int(num('SEQ2', 0, f'Step {st} velocity', 100))}",
                f"arp.step.{st}.length {fmt(num('SEQ2', 0, f'Step {st} length', 50) / 100)}",
                f"arp.step.{st}.advance {int(num('SEQ2', 0, f'Step {st} note advance', 0))}",
                f"arp.step.{st}.legato {int(bool(get('SEQ2', 0, f'Step {st} legato')))}"]

    unmapped = [(t, i, n) for t, i, n, _v, _u in rows if (t, i, n) not in used and n != 'Version']
    out.append(f'# {len(unmapped)} Spectral fields are not used by this build yet, for example:')
    for t, i, n in unmapped[:12]:
        out.append(f'#   {t}#{i} {n}')
    return '\n'.join(out) + '\n', len(unmapped)


def main(argv):
    tools = DEFAULT_TOOLS
    if '--linplug-tools' in argv:
        k = argv.index('--linplug-tools')
        tools = argv[k + 1]
        argv = argv[:k] + argv[k + 2:]
    L = load_reader(tools)
    if len(argv) == 4 and argv[1] == '--batch':
        src, dst = argv[2], argv[3]
        os.makedirs(dst, exist_ok=True)
        ok = bad = 0
        for f in L.fxp_files(src):
            try:
                text, _ = convert(L, f)
            except Exception as e:  # report and continue: one bad preset must not stop a batch
                print(f'skip {f}: {e}')
                bad += 1
                continue
            name = os.path.splitext(os.path.basename(f))[0] + '.ptch'
            with open(os.path.join(dst, name), 'w') as fh:
                fh.write(text)
            ok += 1
        print(f'converted {ok}, skipped {bad}')
        return 0 if ok else 1
    if len(argv) != 3:
        print(__doc__)
        return 2
    text, unmapped = convert(L, argv[1])
    with open(argv[2], 'w') as fh:
        fh.write(text)
    print(f'{argv[2]}: written; {unmapped} Spectral fields not used yet')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
