from pathlib import Path
import subprocess,tempfile,shutil,re,os,hashlib
base=Path(__file__).resolve().parents[1]
package=base/'cherrypi-profile' if (base/'cherrypi-profile').is_dir() else base
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory);target=root/'root/gpsp/gpsp';target.parent.mkdir(parents=True)
    original=b'verified safe baseline';target.write_bytes(original);target.chmod(0o755)
    slot=root/'root/gpsp/Test0.svs';slot.write_bytes(b'existing state')
    launcher=root/'usr/local/bin/cherrypi-launcher';launcher.parent.mkdir(parents=True);launcher.write_text('#!/bin/sh\n');launcher.chmod(0o755)
    pkg=root/'root/cherrypi-profile';shutil.copytree(package,pkg)
    (pkg/'BASELINE_SHA256').write_text(hashlib.sha256(original).hexdigest()+'\n')
    records=[]
    for line in (pkg/'SHA256SUMS').read_text().splitlines():
        digest,name=line.split('  ',1)
        if name=='BASELINE_SHA256':digest=hashlib.sha256((pkg/name).read_bytes()).hexdigest()
        records.append(digest+'  '+name+'\n')
    (pkg/'SHA256SUMS').write_text(''.join(records))
    mock=root/'mock';mock.mkdir()
    for name,code in [('pidof','exit 1'),('sync','exit 0')]:
        p=mock/name;p.write_text('#!/bin/sh\n'+code+'\n');p.chmod(0o755)
    env=dict(os.environ,PATH=str(mock)+':'+os.environ['PATH'])
    def run(name,ok=True):
        script=(package/name).read_text().replace('[ "$(id -u)" -eq 0 ]','true')
        script=re.sub(r'/(?:root|usr/local)(?=/)',lambda m:str(root)+m[0],script)
        p=pkg/('test-'+name);p.write_text(script)
        r=subprocess.run(['busybox','sh',str(p)],env=env,capture_output=True,text=True)
        assert (r.returncode==0)==ok,r.stdout+r.stderr
    target.write_bytes(b'unsupported previous version');run('install.sh',False);assert not (root/'root/cherrypi-optimized-20261017/state').exists()
    target.write_bytes(original);run('install.sh');assert target.read_bytes()==(package/'gpsp').read_bytes()
    assert slot.read_bytes()==b'existing state'
    run('install.sh');assert (root/'root/cherrypi-optimized-20261017/state/gpsp').read_bytes()==original
    slot.write_bytes(b'new state in unchanged format')
    run('uninstall.sh');assert target.read_bytes()==original and slot.read_bytes()==b'new state in unchanged format'
    with (pkg/'gpsp').open('ab') as f:f.write(b'corrupt')
    run('install.sh',False);assert target.read_bytes()==original
print('PASS installer: baseline rejection, checksums, install/reinstall backup, rollback, slots preserved')
