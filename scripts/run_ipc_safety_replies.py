# SPDX-License-Identifier: MIT
# Copyright (c) 2026 yeyunyyds.
"""Production serializer default-retry positive/negative controls across processes."""
import json
import os
from pathlib import Path
import subprocess
import uuid
ROOT=Path(__file__).resolve().parents[1]
results=[]
def pair(source,host,mode,expected):
    guid=str(uuid.uuid4());env=dict(os.environ,LDB_TEST_IPC_BATCH='8')
    server=subprocess.Popen([str(source/('transport-host'+host+'.exe')),guid,mode],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env)
    client=None
    try:
        client=subprocess.Popen([str(source/'transport-client32.exe'),guid,mode],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env)
        cout=client.communicate(timeout=20)[0];hout=server.communicate(timeout=20)[0]
        row=dict(source=source.name,host=host,mode=mode,client_rc=client.returncode,host_rc=server.returncode,client=cout,server=hout)
        results.append(row)
        (ROOT/'work/ipc-safety-replies.json').write_text(json.dumps(results,indent=2)+'\n')
        if expected:
            if client.returncode or server.returncode:raise RuntimeError(str(row))
        elif not client.returncode or 'C32_BRIDGE_FAIL' not in cout:
            raise RuntimeError('Original default-retry boundary was not detected: '+str(row))
        print('REPLY_BUDGET_PAIR_PASS',source.name,host,mode,'positive' if expected else 'negative',flush=True)
    finally:
        for child in [client,server]:
            if child is not None and child.poll() is None:child.kill();child.wait()
for host in ['32','64']:
    for mode in ['reply-short-budget','reply-default-budget','reply-abandoned','reply-cancel']:
        pair(ROOT/'work/ipc-full-native',host,mode,True)
pair(ROOT/'work/ipc-safety-baseline','64','reply-default-budget',False)
