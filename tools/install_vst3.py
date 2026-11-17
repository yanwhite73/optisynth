#!/usr/bin/env python3
"""Install the qualified local VST3 atomically, with backup and hash verification.

No AU installation, host launch/restart, or preset-library modification.
"""
from pathlib import Path
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import shutil
import subprocess

base = Path(__file__).resolve().parents[1]
source = base / "plugin/Builds/PartialsMacOSX/build/Release/Optispectral.vst3"
destination = Path.home() / "Library/Audio/Plug-Ins/VST3/Optispectral.vst3"


def manifest(path):
    return {str(p.relative_to(path)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(path.rglob("*")) if p.is_file()}


def verify(path):
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(path)], check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-sha", required=True)
    args = parser.parse_args()
    expected = manifest(source)
    assert expected["Contents/MacOS/Optispectral"] == args.expected_sha, "Build changed since qualification"
    verify(source)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    staging_root = base / "build/host_extension" / ("install-" + stamp)
    staging = staging_root / "Optispectral.vst3"
    backup = base.parent / "backups/partials" / ("vst3-before-" + stamp) / "Optispectral.vst3"
    assert destination.parent.is_dir(), "Expected existing VST3 installation folder"
    assert os.stat(base).st_dev == os.stat(destination.parent).st_dev, "Atomic installation requires same filesystem"
    assert not staging_root.exists() and not backup.parent.exists()
    shutil.copytree(source, staging, symlinks=True)
    assert manifest(staging) == expected
    verify(staging)
    previous = manifest(destination) if destination.exists() else None
    backup.parent.mkdir(parents=True)
    if previous is not None:
        destination.rename(backup)
    try:
        staging.rename(destination)
        verify(destination)
        assert manifest(destination) == expected
    except BaseException:
        if destination.exists():
            destination.rename(staging)
        if previous is not None:
            backup.rename(destination)
        raise
    staging_root.rmdir()
    receipt = {"installed": str(destination), "backup": str(backup) if previous else None,
               "sha256": args.expected_sha, "files": expected, "previousFiles": previous,
               "signatureVerified": True, "liveVerified": False}
    report = base / "build/host_extension/install_receipt.json"
    report.write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"installed": str(destination), "backup": receipt["backup"],
                      "verifiedFiles": len(expected), "receipt": str(report)}, indent=2))
