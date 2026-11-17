"""Regression coverage for actual padded matrix destination strings in factory presets."""
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from import_linplug import matrix_dest, convert


class MatrixDestinations(unittest.TestCase):
    def test_factory_envelope_depth(self):
        for osc in range(1, 5):
            self.assertEqual(matrix_dest(f'Filter {osc} $Env-Depth '), f'filter{osc}.envdepth')

    def test_older_spelling(self):
        self.assertEqual(matrix_dest('Filter 2 $Env Depth '), 'filter2.envdepth')

    def test_unimplemented_destination_stays_visible(self):
        self.assertEqual(matrix_dest('Filter 1 $LP+ Cutoff'), 'raw_filter_1_lp+_cutoff')

    def test_zero_depth_factory_route_survives_import(self):
        root = Path(__file__).resolve().parents[2] / 'reverse-engineer'
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'reference/tools'))
        import linplug_fxp
        source = root / 'corpus/backup-LinPlug/Spectral/Sound Banks/leads/hr Tortured Lead 2.fxp'
        if not source.exists():
            self.skipTest('local reference corpus unavailable')
        text, _ = convert(linplug_fxp, str(source))
        rows = [line for line in text.splitlines() if line.startswith('matrix.')]
        self.assertIn('matrix.2.source lfo1', rows)
        self.assertIn('matrix.2.dest main.pitch', rows)
        self.assertIn('matrix.2.amount 0', rows)
        self.assertIn('matrix.8.dest matrix.depth2', rows)


if __name__ == '__main__':
    unittest.main()
