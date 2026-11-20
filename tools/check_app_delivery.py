"""Exercise stable bundle identity and failure rollback with disposable signed app copies."""
import json
import plistlib
import select
import subprocess
from pathlib import Path
import shutil
import tempfile
from unittest.mock import patch
import deliver_app as delivery


def check():
    source = delivery.APP
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        release = root / 'Release'
        app = release / source.name
        receipt = release / 'Optisynth-build.json'
        candidate = root / 'Candidate' / source.name
        shutil.copytree(source, candidate)
        with patch.multiple(delivery, RELEASE=release, APP=app, RECEIPT=receipt, ARCHIVES=root/'archive'):
            delivery.copy_delivery(source, 'Initial')
            initial_id = app.stat().st_ino
            initial_content_id = (app/'Contents').stat().st_ino
            delivery.copy_delivery(candidate, 'Second')
            assert app.stat().st_ino == initial_id
            assert (app/'Contents').stat().st_ino != initial_content_id
            assert delivery.inspect(Path(json.loads(receipt.read_text())['previous_app'])) == delivery.inspect(source)
            old_contents = (app/'Contents').stat().st_ino
            old_receipt = receipt.read_bytes()
            with patch.object(Path, 'replace', side_effect=OSError('simulated receipt failure')):
                try:
                    delivery.copy_delivery(candidate, 'Broken')
                except OSError as exc:
                    assert str(exc) == 'simulated receipt failure'
                else:
                    raise AssertionError('Expected receipt failure')
            assert (app.stat().st_ino, (app/'Contents').stat().st_ino) == (initial_id, old_contents)
            assert receipt.read_bytes() == old_receipt
            assert delivery.inspect(app) == delivery.inspect(source)
    print('Passed stable app identity, signed contents, archived previous app and rollback.')


def signed_probe(directory, version):
    """Build two distinguishable macOS executables without taking an audio device."""
    bundle = directory / 'Optisynth.app'
    contents = bundle / 'Contents'
    (contents / 'MacOS').mkdir(parents=True)
    info = {'CFBundleExecutable': 'Optisynth', 'CFBundleIdentifier': 'local.test.optisynth.update',
            'CFBundleName': 'Optisynth', 'CFBundlePackageType': 'APPL',
            'CFBundleVersion': version, 'CFBundleShortVersionString': version}
    (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
    source = '#include <stdio.h>\nint main(void) { puts(VERSION); fflush(stdout); getchar(); puts(VERSION); }'
    subprocess.run(['cc', '-x', 'c', f'-DVERSION="{version}"', '-',
                    '-o', str(contents / 'MacOS/Optisynth')], input=source, text=True, check=True)
    subprocess.run(['codesign', '--force', '--sign', '-', str(bundle)], check=True)
    return bundle


def check_running_update():
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        old = signed_probe(root/'old', '1')
        new = signed_probe(root/'new', '2')
        release = root/'Release'
        app = release/'Optisynth.app'
        executable = app/'Contents/MacOS/Optisynth'
        receipt = release/'Optisynth-build.json'
        with patch.multiple(delivery, RELEASE=release, APP=app, RECEIPT=receipt, ARCHIVES=root/'archive'):
            delivery.copy_delivery(old, 'Initial')
            folder_id = app.stat().st_ino
            with subprocess.Popen([executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as session:
                try:
                    assert select.select([session.stdout], [], [], 10)[0], 'Test app did not start'
                    assert session.stdout.readline().strip() == '1'
                    delivery.copy_delivery(new, 'Ready for next launch')
                    expected = delivery.inspect(new)
                    installed = json.loads(receipt.read_text())
                    assert delivery.inspect(app) == expected
                    assert all(installed[key] == value for key, value in expected.items())
                    assert app.stat().st_ino == folder_id
                    assert session.poll() is None
                    remaining, _ = session.communicate(input='\n', timeout=10)
                    assert session.returncode == 0 and remaining.strip() == '1'
                finally:
                    if session.poll() is None:
                        session.kill()
            reopened = subprocess.run([executable], input='\n', text=True, capture_output=True,
                                      check=True, timeout=10)
            assert reopened.stdout.splitlines() == ['2', '2']
    print('Passed update during use: original process stayed on v1, immediate reopen from the same path ran v2.')


if __name__ == '__main__':
    check()
    check_running_update()
