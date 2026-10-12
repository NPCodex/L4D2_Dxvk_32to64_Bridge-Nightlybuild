"""Apply full transport overlay after the frozen C32 production preparation."""
from pathlib import Path
import hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
def prepare(source):
    snapshot=ROOT/'work/c32-source/bridge/src/util'
    snapshot.parent.mkdir(parents=True,exist_ok=True)
    shutil.copytree(source/'bridge/src/util',snapshot,dirs_exist_ok=True)
    patch=ROOT/'patches/ipc-full.patch'
    subprocess.run(['git','apply','--check',str(patch)],cwd=source,check=True)
    subprocess.run(['git','apply',str(patch)],cwd=source,check=True)
    util=source/'bridge/src/util'
    for folder in ('ipc-full','ipc-v2'):
        target=util/folder.replace('-','_');target.mkdir(exist_ok=True)
        for p in sorted((ROOT/'experiments'/folder).glob('*.h')):shutil.copy2(p,target/p.name)
    digest=hashlib.sha256()
    paths=[ROOT/'patches/l4d2-bridge.patch',ROOT/'patches/ipc-c32.patch',patch]
    paths+=sorted((ROOT/'experiments/ipc-c32').glob('*.h'))+sorted((ROOT/'experiments/ipc-full').glob('*.h'))+sorted((ROOT/'experiments/ipc-v2').glob('*.h'))
    for p in paths:digest.update(p.relative_to(ROOT).as_posix().encode()+b'\0'+p.read_bytes().replace(b'\r\n',b'\n')+b'\0')
    sha=digest.hexdigest()
    (util/'ipc_v2/ipc_v2_build.h').write_text(''.join(f'#define LDB_IPC_V2_BUILD{i} 0x{sha[i*8:i*8+8]}u\n' for i in range(4)))
    receipt=dict(build_id='ldb-ipc-full-dev+'+sha[:16],protocol_source_sha256=sha,foundation='C1/C2/C4/C8/C16/C32 production d59493c + VA allocator b90d056',
                 protocol_version='0x00020004',message_blocks=64,message_block_bytes=4096,upload_page_bytes=4096,upload_slots=1024,
                 supported_batches=[1,2,4,8,16,32],legacy_data_ring=False,reply_v2=True,upload_leases=True,reply_slots=False,gameplay_validated=False)
    (ROOT/'work').mkdir(exist_ok=True);(ROOT/'work/ipc-full-build.json').write_text(json.dumps(receipt,indent=2)+'\n');print(receipt['build_id'])
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=ROOT/'.deps/dxvk-remix');a=p.parse_args();prepare(a.source.resolve())
