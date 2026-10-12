"""Exercise production SharedHeap bounds plus x86->x86/x64 physical mappings."""
import json
import argparse
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output', required=True, type=Path)
OUT = parser.parse_args().output
rows = []
for arch in ('x86','x64'):
    result = subprocess.run([OUT / f'clientheap-{arch}.exe'], capture_output=True, text=True, timeout=20)
    rows.append(dict(role='client-'+arch, rc=result.returncode, stdout=result.stdout, stderr=result.stderr))
    (OUT / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    result.check_returncode()
    print(result.stdout, end='', flush=True)
for role in ('client32', 'host32', 'host64'):
    result = subprocess.run([OUT / f'heap-{role}.exe'], capture_output=True, text=True, timeout=20)
    rows.append(dict(role=role, rc=result.returncode, stdout=result.stdout, stderr=result.stderr))
    (OUT / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    result.check_returncode()
    print(result.stdout, end='', flush=True)
for role in ('host32', 'host64'):
    guid = str(uuid.uuid4())
    client = subprocess.Popen([OUT / 'heap-client32.exe', guid], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        fields = []
        while True:
            line = client.stdout.readline()
            if line.startswith('HEAP_ALLOCATION '):
                fields.extend(line.split()[1:])
            if line.startswith('HEAP_READY'):
                break
            if not line:
                raise RuntimeError('Client failed to publish allocation fields')
        result = subprocess.run([OUT / f'heap-{role}.exe', guid, *fields], capture_output=True, text=True, timeout=20)
        ct, ce = client.communicate('q', timeout=20)
        rows.append(dict(role=role, cross_process=True, fields=fields, client_rc=client.returncode,
                         host_rc=result.returncode, host=result.stdout, stderr=result.stderr, client=ct, client_stderr=ce))
        (OUT / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
        result.check_returncode()
        if client.returncode:
            raise RuntimeError(f'Client failure: {ct} {ce}')
        print(result.stdout, end='', flush=True)
    finally:
        if client.poll() is None:
            client.kill()
            client.wait()
print('OPTIMIZATION45_SHAREDHEAP_PASS', flush=True)
