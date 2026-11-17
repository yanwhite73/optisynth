#!/usr/bin/env python3
"""Bounded reference-document downloads. Run via agent-workload --kind download.

Reads the reviewed project source index; writes only docs/references and its
receipt. Does not extract archives, execute downloads, open plugins or contact a
DAW. At most 50 MB per document, 30-second socket timeout; no retry loop.
"""
import hashlib
import json
from pathlib import Path
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def main():
    index = json.loads((ROOT/'docs/research_sources.json').read_text())
    out = ROOT/'docs/references'
    out.mkdir(exist_ok=True)
    receipts = []
    for row in index['documents']:
        name = row['file']
        if Path(name).name != name or not row['url'].startswith('https://'):
            raise ValueError('Unexpected source destination/URL')
        target = out/name
        partial = out/(name+'.part')
        receipt = dict(row)
        try:
            if not target.exists():
                request = urllib.request.Request(row['url'], headers={'User-Agent': 'Optisynth-reference-research/1.0'})
                start = time.monotonic()
                with urllib.request.urlopen(request, timeout=30) as response, partial.open('xb') as f:
                    receipt['resolved_url'] = response.url
                    total = 0
                    while True:
                        chunk = response.read(256*1024)
                        if not chunk:
                            break
                        total += len(chunk)
                        if total > 50_000_000 or time.monotonic()-start > 120:
                            raise ValueError('Transfer limit exceeded')
                        f.write(chunk)
                with partial.open('rb') as f:
                    prefix = f.read(8)
                if not (prefix.startswith(b'%PDF-') if name.endswith('.pdf') else prefix.startswith(b'PK\x03\x04')):
                    raise ValueError('Response is not the expected document/archive type')
                partial.rename(target)
            data = target.read_bytes()
            receipt.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                           saved_as=str(target.relative_to(ROOT)), status='downloaded')
            print(name, len(data), receipt['sha256'][:12], flush=True)
        except Exception as error:
            if partial.exists():
                partial.unlink()
            receipt.update(status='failed', error=str(error))
            print(name, 'FAILED', str(error), flush=True)
        receipts.append(receipt)
        (ROOT/'docs/reference_downloads.json').write_text(json.dumps(receipts, indent=2)+'\n')
    if any(r['status'] != 'downloaded' for r in receipts):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
