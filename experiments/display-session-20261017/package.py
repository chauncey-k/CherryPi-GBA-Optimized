from pathlib import Path
import hashlib,tarfile,difflib,os
b=Path(__file__).resolve().parent;p=b/'display-session' if not (b/'install.sh').is_file() else b
r=Path(os.environ.get('CHERRYPI_REPO_ROOT',str(b.parents[1])))
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
(p/'SUPERVISOR_BASELINES').write_text(sha(r/'components/system-scripts/supervisor.sh')+'\n'+sha(p/'supervisor.sh')+'\n',newline='\n')
runtime=('cherrypi-display-session','supervisor.sh','SUPERVISOR_BASELINES','COPYING','README.md','install.sh','uninstall.sh','SHA256SUMS')
if p!=b:(p/'README.md').write_bytes((b/'README.md').read_bytes())
(p/'SHA256SUMS').write_text(''.join(sha(p/name)+'  '+name+'\n' for name in sorted(runtime) if name!='SHA256SUMS'),newline='\n')
(b/'supervisor.patch').write_text(''.join(difflib.unified_diff((r/'components/system-scripts/supervisor.sh').read_text().splitlines(True),(b/'source/supervisor.sh').read_text().splitlines(True),fromfile='a/supervisor.sh',tofile='b/supervisor.sh')),newline='\n')
archive=b/'cherrypi-display-session-20261017.tar.gz'
def attrs(m):
 m.uid=m.gid=0;m.uname=m.gname='root';m.mode=0o755 if m.isdir() or m.name.endswith(('.sh','/cherrypi-display-session')) else 0o644
 return m
with tarfile.open(archive,'w:gz') as t:
 for name in runtime:t.add(p/name,arcname='cherrypi-display-session/'+name,filter=attrs)
 for name in ('source','tests','build.sh','supervisor.patch','package.py'):
  t.add(b/name,arcname='cherrypi-display-session/'+name,filter=attrs)
(b/'SHA256SUMS').write_text(sha(archive)+'  '+archive.name+'\n',newline='\n')
print(archive,archive.stat().st_size,'bytes; helper SHA256',sha(p/'cherrypi-display-session'))
