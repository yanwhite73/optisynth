"""Exercise stable bundle identity and failure rollback with disposable signed app copies."""
import json
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


if __name__ == '__main__':
    check()
