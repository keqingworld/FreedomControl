"""Read the *shipped ESP* into the adversarial native-control fixture.

This is a narrow schema/condition model, not Skyrim's package loader or pathfinder.
It cannot establish that a template executes in-game. Unlike the original test
setup, it does not invent a single always-accepted package for every actor.
"""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from esp_read import Plugin


def write_fixture(destination: Path):
    plugin = Plugin(ROOT / 'package/FreedomControlRuntime.esp')
    packages = [r for r in plugin.records if r['type'] == 'PACK']
    quest = next(r for r in plugin.records if r['type'] == 'QUST' and r['id'] & 0xfff == 0x800)
    # The runtime stack must really contain each record, in its authored order.
    aliases = []
    for name, value in quest['subs']:
        if name == 'ALST':
            aliases.append([])
        elif name == 'ALPC':
            aliases[-1].append(struct.unpack('<I', value)[0] & 0xfff)
    assert len(aliases) == 512 and all(a == aliases[0] for a in aliases)
    lines = ['#pragma once', 'namespace fixture15 {',
             'struct Condition { unsigned function, parameter, runOn, op; float comparison; };',
             'struct Definition { unsigned local, type, program, target; bool schemaReady; std::vector<Condition> conditions; };',
             'inline const std::vector<Definition> definitions{']
    for record in packages:
        fields = dict(record['subs'])
        count, program, version = struct.unpack('<III', fields['PKCU'])
        flags, kind, interrupt, speed, _, _, _ = struct.unpack('<IBBBBHH', fields['PKDT'])
        types = [value.rstrip(b'\0') for name, value in record['subs'] if name == 'ANAM']
        indexes = [value[0] for name, value in record['subs'] if name == 'UNAM']
        # Structural validity only, backed by xEdit PACK field definitions.
        # Template behavior still requires the game and its Skyrim.esm.
        schema = kind == 18 and count == len(types) == len(indexes) and len(fields['PSDT']) == 12
        target = struct.unpack('<III', fields.get('PTDA', fields.get('PLDT')))[1]
        conditions = []
        for name, value in record['subs']:
            if name == 'CTDA':
                op, comparison, function, parameter, parameter2, run_on, ref, unknown = struct.unpack('<B3xfH2xIIIIi', value)
                assert parameter2 == 0 and ref == 0 and unknown == -1
                conditions.append('{%d,%d,%d,%d,%sf}' % (function, parameter & 0xfff, run_on, op, float(comparison)))
        lines.append('{%d,%d,%d,%d,%s,{%s}},' % (record['id'] & 0xfff, kind, program, target,
                      str(schema).lower(), ','.join(conditions)))
    lines += ['};', 'inline const std::vector<unsigned> aliasOrder{' + ','.join(map(str, aliases[0])) + '};', '}']
    destination.write_text('\n'.join(lines) + '\n')
