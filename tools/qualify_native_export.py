#!/usr/bin/env python3
"""Qualify opt-in state-only export; run under agent-workload --kind test.

Requires a previously qualified full-audio native preset and its original .ptch.
No Live, installation, third-party plugin scanning or audio-device access.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def qualify(source, reference, destination, regression=None):
    base = Path(__file__).resolve().parents[1]
    helper = base / 'plugin/Tests/Builds/PartialsStressMacOSX/build/Release/PartialsStress'
    source, reference, destination = source.resolve(), reference.resolve(), destination.resolve()
    assert source.is_file() and reference.is_file()
    destination.mkdir(parents=True, exist_ok=False)
    original = reference.read_bytes()
    expected_count = json.loads((base / 'docs/host_parameters.json').read_text())['processorParameterCount']
    cases = []

    def run(name, args, expected=0, error=''):
        command = [str(helper), *map(str, args)] if name == 'common-float-policy' else [str(helper), '--export-vstpreset', *map(str, args)]
        result = subprocess.run(command,
                                cwd=base, capture_output=True, text=True, timeout=120)
        (destination / (name + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == expected, (name, result.returncode, result.stdout, result.stderr)
        assert error in result.stderr, (name, result.stderr)
        cases.append({'name': name, 'exit': result.returncode})
        print(f'PASS: {name}', flush=True)

    run('common-float-policy', ['--test-export-state-comparison'])
    state = destination / 'state-only.vstpreset'
    audio = destination / 'default-audio.vstpreset'
    run('state-only-sample', ['--state-only', source, state])
    run('default-audio-sample', [source, audio])
    assert state.read_bytes() == audio.read_bytes() == original, 'Native bytes changed between modes/reference'
    for path, state_only in [(state, True), (audio, False)]:
        report = json.loads(Path(str(path) + '.json').read_text())
        assert report['passed'] and report['embeddedStateExact'] and report['freshVst3PresetRecall']
        assert report['schema'] == 3 and report['recalledStateExact']
        assert report['nonHostStateExact'] and report['hostParameterValuesWithinTolerance']
        assert report['hostParameterNormalizedTolerance'] == 1e-6
        assert report['maxHostParameterNormalizedDelta'] <= 1e-6
        assert report['persistedParametersAndNonHostFieldsExact']
        assert report['parametersChecked'] == expected_count and report['bypassVerified'] and report['sourceAutoLevelMeasured']
        assert report['verificationMode'] == ('state-only' if state_only else 'state-and-audio')
        assert report['audioVerified'] is (not state_only)
        assert not report['liveVerified']
        if state_only:
            assert report['audioSkippedReason']
            assert not any(key in report for key in ['audioPeak', 'audioFrames', 'audioSampleRate',
                                                     'audioMaxAbsoluteError', 'audioRelativeRmsError'])
        else:
            assert report['audioFrames'] == 128000 and report['audioMaxAbsoluteError'] == 0

    silent = destination / 'silent.ptch'
    silent.write_text('partials-patch 1\nname Export silent fixture\n'
                      + ''.join(f'osc.{index}.on 0\n' for index in range(1, 5)))
    silent_state = destination / 'silent-state-only.vstpreset'
    silent_audio = destination / 'silent-default.vstpreset'
    run('silent-state-only', [silent, silent_state, '--state-only'])
    silent_report = json.loads(Path(str(silent_state) + '.json').read_text())
    assert silent_report['verificationMode'] == 'state-only' and not silent_report['audioVerified']
    assert silent_report['parametersChecked'] == expected_count and silent_report['embeddedStateExact']
    assert silent_report['trimDb'] == 0 and silent_report['sourceAutoLevelMeasured']
    run('silent-default-refused', [silent, silent_audio], 1, 'Sample is silent')
    assert not silent_audio.exists() and not Path(str(silent_audio) + '.json').exists()
    run('duplicate-option-refused', ['--state-only', '--state-only', source, state], 2, 'usage:')
    run('unknown-option-refused', ['--state-onli', source, state], 2, 'usage:')
    before = [sha(p) for p in [state, Path(str(state) + '.json')]]
    run('overwrite-refused', ['--state-only', source, state], 1, 'Refusing to overwrite')
    assert before == [sha(p) for p in [state, Path(str(state) + '.json')]]
    assert reference.read_bytes() == original
    for index, regression_path in enumerate(regression or []):
        regression_output = destination / f'recall-regression-{index}.vstpreset'
        run(f'recall-regression-{index}', ['--state-only', regression_path.resolve(), regression_output])
        regression_report = json.loads(Path(str(regression_output) + '.json').read_text())
        assert regression_report['schema'] == 3 and regression_report['passed']
        assert not regression_report['embeddedStateExact']
        assert regression_report['nonHostStateExact'] and regression_report['hostParameterValuesWithinTolerance']
        assert regression_report['hostParameterNormalizedTolerance'] == 1e-6
        assert 0 < regression_report['maxHostParameterNormalizedDelta'] <= 1e-6
        assert regression_report['stateComparison'] == 'bounded-host-floats-exact-non-host-state'
        assert regression_report['parametersChecked'] == expected_count and regression_report['bypassVerified']
        assert not regression_report['audioVerified']
    summary = {'passed': True, 'cases': cases, 'helperSha256': sha(helper),
               'sampleSha256': sha(state), 'reference': str(reference),
               'entireNativePresetMatchesReferenceAndBothModes': True,
               'silentStateExported': True, 'defaultAudioStillRejectsSilence': True,
               'liveVerified': False}
    (destination / 'qualification.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--recall-regression', type=Path, action='append')
    args = parser.parse_args()
    qualify(args.source, args.reference, args.out, args.recall_regression)
