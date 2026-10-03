"""Independent, source-backed interpretation of shipped follower PACK records.

The schema oracle comes from xEdit definitions and real direct-player Follow
fixtures, not generate_runtime.py. This is NOT Skyrim's loader or pathfinder.
"""
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from esp_read import Plugin


class FollowSchema(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = Plugin(ROOT / 'package/FreedomControlRuntime.esp')
        cls.packages = {r['id'] & 0xfff: r for r in cls.plugin.records if r['type'] == 'PACK'}
        cls.quest = next(r for r in cls.plugin.records if r['type'] == 'QUST' and r['id'] & 0xfff == 0x800)

    def field(self, record, name):
        return next(v for k, v in record['subs'] if k == name)

    def inputs(self, record):
        out = []
        for i, (name, value) in enumerate(record['subs']):
            if name == 'ANAM':
                kind = value.rstrip(b'\0').decode()
                tag, payload = record['subs'][i + 1]
                expected = {'SingleRef': 'PTDA', 'Location': 'PLDT', 'Float': 'CNAM', 'Bool': 'CNAM'}[kind]
                self.assertEqual(tag, expected)
                self.assertEqual(len(payload), {'SingleRef': 12, 'Location': 12, 'Float': 4, 'Bool': 1}[kind])
                out.append((kind, payload))
        return out

    def test_direct_follow_matches_independent_record_schema(self):
        # HappyCat.esp AppleCatPackage 0x082A and original-research direct-player
        # Follow records independently corroborate this same template contract.
        for local, record in self.packages.items():
            if local == 0x809:
                continue
            with self.subTest(local=hex(local)):
                self.assertEqual(struct.unpack('<III', self.field(record, 'PKCU')), (6, 0x19b2c, 4))
                inputs = self.inputs(record)
                self.assertEqual([kind for kind, _ in inputs], ['SingleRef', 'Float', 'Float', 'Bool', 'Bool', 'Bool'])
                self.assertEqual(struct.unpack('<III', inputs[0][1]), (0, 0x14, 0))
                self.assertEqual([payload[0] for _, payload in inputs[3:]], [1, 0, 0])
                self.assertEqual([v[0] for k, v in record['subs'] if k == 'UNAM'], [0, 1, 2, 4, 6, 8])
                self.assertEqual(self.field(record, 'XNAM'), b'\x09')
                self.assertFalse(any(k == 'QNAM' for k, _ in record['subs']))
                names = [k for k, _ in record['subs']]
                self.assertLess(max(i for i, k in enumerate(names) if k == 'ANAM'), names.index('UNAM'))
                self.assertEqual(self.field(record, 'PSDT'), bytes.fromhex('ff ff 00 ff ff 00 00 00 00 00 00 00'))

    def test_authored_float_values_and_unconditional_fallback(self):
        distances = (96, 160, 240, 400, 700, 1100, 1800, 3000)
        for band, distance in enumerate(distances):
            for lane in range(3):
                inputs = self.inputs(self.packages[0x820 + band * 3 + lane])
                values = [struct.unpack('<f', payload)[0] for kind, payload in inputs if kind == 'Float']
                self.assertEqual(values, [distance + lane * 48, distance + lane * 48 + 128])
        fallback = self.packages[0x80a]
        self.assertFalse(any(k == 'CTDA' for k, _ in fallback['subs']))
        self.assertEqual([struct.unpack('<f', v)[0] for k, v in self.inputs(fallback) if k == 'Float'], [240, 368])

    def test_orbit_matches_independent_dragon_record_schema(self):
        record = self.packages[0x809]
        self.assertEqual(struct.unpack('<III', self.field(record, 'PKCU')), (4, 0x15b84, 1))
        inputs = self.inputs(record)
        self.assertEqual([kind for kind, _ in inputs], ['Location', 'Float', 'Float', 'Float'])
        self.assertEqual(struct.unpack('<III', inputs[0][1]), (0, 0x14, 50))
        self.assertEqual([v[0] for k, v in record['subs'] if k == 'UNAM'], [1, 2, 3, 4])
        self.assertEqual(self.field(record, 'XNAM'), b'\x05')

    def test_real_alias_order_selects_expected_band_or_fallback(self):
        aliases = []
        for name, value in self.quest['subs']:
            if name == 'ALST':
                aliases.append([])
            elif name == 'ALPC':
                aliases[-1].append(struct.unpack('<I', value)[0] & 0xfff)
        self.assertEqual(len(aliases), 512)
        self.assertTrue(all(order == aliases[0] for order in aliases))
        self.assertEqual(aliases[0], [0x809] + list(range(0x820, 0x838)) + [0x80a])

        def accepts(record, band, rank, dragon):
            for key, value in record['subs']:
                if key != 'CTDA':
                    continue
                op, comparison, function, parameter, parameter2, run_on, ref, unknown = struct.unpack('<B3xfH2xIIIIi', value)
                self.assertEqual((op, parameter2, run_on, ref, unknown), (0, 0, 0, 0, -1))
                local = parameter & 0xfff
                if function == 71:
                    self.assertEqual(local, 0x803); observed = float(dragon)
                elif function == 73:
                    self.assertEqual(local, 0x801); observed = float(rank)
                elif function == 74:
                    self.assertEqual(local, 0x804); observed = float(band)
                else:
                    self.fail('Unexpected condition function ' + str(function))
                if observed != comparison:
                    return False
            return True

        for band in range(8):
            for rank in range(3):
                for dragon in (False, True):
                    selected = next(local for local in aliases[0] if accepts(self.packages[local], band, rank, dragon))
                    self.assertEqual(selected, 0x809 if dragon else 0x820 + band * 3 + rank)
        for band, rank in [(-1, 0), (9, 0), (2, -1), (2, 127)]:
            selected = next(local for local in aliases[0] if accepts(self.packages[local], band, rank, False))
            self.assertEqual(selected, 0x80a)


if __name__ == '__main__':
    unittest.main(verbosity=2)
