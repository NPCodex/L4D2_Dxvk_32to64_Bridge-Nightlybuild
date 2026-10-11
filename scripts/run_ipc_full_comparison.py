"""Outer-batch CPU/wall timing; raw failures are retained, never discarded."""
from pathlib import Path
import hashlib,json,os,re,statistics,subprocess,uuid
ROOT=Path(__file__).resolve().parents[1]
dirs={'C32':ROOT/'work/ipc-full-comparison','Full':ROOT/'work/ipc-full-native'}
out=ROOT/'work/ipc-full-comparison'
for folder in dirs.values():
    assert (folder/'full_bridge_cases.cpp').read_bytes()==(ROOT/'tests/ipc-v2/full_bridge_cases.cpp').read_bytes()
rows=[]
pattern=re.compile(r'C32_TIMING cycles=(\d+) commands=(\d+) wall_ns=(\d+) threaded=(\d+)')
batches=('1','2','4','8','16','32')
for iteration in range(6):
    for batch in (batches if iteration%2==0 else tuple(reversed(batches))):
        for arch in ('32','64'):
            for mode in (('stream','mixed') if iteration%2==0 else ('mixed','stream')):
                for variant in (('C32','Full') if iteration%2==0 else ('Full','C32')):
                    folder=dirs[variant];guid=str(uuid.uuid4());env=dict(os.environ,LDB_TEST_IPC_BATCH=batch)
                    host=subprocess.Popen([folder/f'transport-host{arch}.exe',guid,mode],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
                    client=None
                    try:
                        client=subprocess.Popen([folder/'transport-client32.exe',guid,mode],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
                        ct=client.communicate(timeout=30)[0];ht=host.communicate(timeout=30)[0]
                        row=dict(iteration=iteration,warmup=iteration==0,batch=batch,host_arch=arch,mode=mode,variant=variant,
                             client_rc=client.returncode,host_rc=host.returncode,client_raw=ct,host_raw=ht)
                        for role,raw in (('client',ct),('host',ht)):
                            values=pattern.findall(raw)
                            row[role+'_timing']=values
                            row[role+'_upload_capacities']=[int(v) for v in re.findall(r'TRANSPORT_CAPACITY upload_bytes=(\d+)',raw)]
                            if len(values)==2:
                                commands=sum(int(v[1]) for v in values)
                                row[role+'_cycles_per_command']=sum(int(v[0]) for v in values)/commands
                                row[role+'_wall_ns_per_command']=sum(int(v[2]) for v in values)/commands
                        rows.append(row);(out/'comparison-raw.json').write_text(json.dumps(rows,indent=2)+'\n')
                        if client.returncode or host.returncode or len(row['client_timing'])!=2 or len(row['host_timing'])!=2:
                            print(ct,ht,flush=True);raise RuntimeError(f'Comparison failed: {variant} {batch} {arch} {mode}')
                        print(f'COMPARISON_PASS iteration={iteration} variant={variant} batch={batch} host={arch} mode={mode}',flush=True)
                    finally:
                        for child in (client,host):
                            if child is not None and child.poll() is None:child.kill();child.wait()
summary=[]
for batch in batches:
    for arch in ('32','64'):
        for mode in ('stream','mixed'):
            entry=dict(batch=batch,host_arch=arch,mode=mode)
            for metric in ('client_cycles_per_command','host_cycles_per_command','client_wall_ns_per_command','host_wall_ns_per_command'):
                medians={variant:statistics.median(r[metric] for r in rows if not r['warmup'] and r['variant']==variant and r['batch']==batch and r['host_arch']==arch and r['mode']==mode) for variant in dirs}
                paired=[]
                for iteration in range(1,6):
                    values={r['variant']:r[metric] for r in rows if r['iteration']==iteration and r['batch']==batch and r['host_arch']==arch and r['mode']==mode}
                    paired.append((values['Full']/values['C32']-1)*100)
                entry[metric]=dict(medians,full_change_percent=(medians['Full']/medians['C32']-1)*100,
                                   paired_median_percent=statistics.median(paired),paired_change_percent=paired)
            summary.append(entry)
checksums={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in dirs.values() for p in folder.glob('transport-*.exe')}
receipt=dict(baseline='published all-gear C32 00007e8 (production d59493c)',rounds=5,warmup_rounds=1,mapping_budget_bytes=2097152,
             usable_upload_bytes={variant:{role:next(r[role+'_upload_capacities'] for r in rows if r['variant']==variant) for role in ('client','host')} for variant in dirs},
             compiler='MSVC 14.29 /O2 /DNDEBUG /LARGEADDRESSAWARE',caller='identical full_bridge_cases.cpp',
             limitations=['Host Sleep(1) every 1024 calls; RPC every 101 calls; outer wall includes these waits','not FPS or GPU timing','synthetic mixed mode uses 512-byte upload on one third of calls','same mapping budget; Full lease metadata reduces usable payload capacity relative to C32'],
             binary_sha256=checksums,results=summary)
(out/'comparison-summary.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(summary,indent=2))
