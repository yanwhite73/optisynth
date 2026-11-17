#!/usr/bin/env python3
"""Offline CLI timing regressions; run via agent-workload after building renderer."""
import array
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RendererTiming(unittest.TestCase):
    def test_sample_timing_and_retrigger(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            patch = folder / 'sine.ptch'
            patch.write_text('partials-patch 1\nname Timing test\n'
                             'osc.1.on 1\nosc.2.on 0\nosc.3.on 0\nosc.4.on 0\n'
                             'osc.1.filter.on 0\nenv.main.attack 0\nenv.main.release 0\n'
                             'env.amp.1.attack 0\nenv.amp.1.release 0\n')

            def render(name, notes, *extra):
                output = folder / (name + '.wav')
                cmd = [str(ROOT / 'build/partials-render'), str(patch), str(output),
                       '--raw', '--sr', '44100', '--seconds', '0.4', *extra]
                for n in notes:
                    cmd += ['--note', n]
                subprocess.run(cmd, check=True, capture_output=True, timeout=30)
                data = output.read_bytes()
                self.assertEqual(data[36:40], b'data')
                self.assertEqual(struct.unpack_from('<I', data, 40)[0], 17640 * 8)
                samples = array.array('f', data[44:])
                return samples[::2]

            # Neither 441 nor 5733 lies on the old 256-sample boundaries.
            delayed = render('delayed', ['69:0.01:0.12:100'], '--bpm', '110', '--ppq-start', '448')
            self.assertTrue(all(v == 0 for v in delayed[:441]))
            self.assertGreater(max(abs(v) for v in delayed[441:473]), 1e-5)
            # A note shorter than a host block used to be released before any audio.
            short = render('short', ['69:0.01:0.001:100'])
            self.assertGreater(max(abs(v) for v in short[441:485]), 1e-5)
            # End/start the same pitch at the same sample; the new note must survive.
            again = render('retrigger', ['69:0.01:0.12:100', '69:0.13:0.12:100'])
            self.assertGreater(max(abs(v) for v in again[6615:8820]), 1e-5)
            self.assertLess(max(abs(v) for v in delayed[6615:8820]),
                            max(abs(v) for v in again[6615:8820]) * .05)


if __name__ == '__main__':
    unittest.main()
