from pathlib import Path
import hashlib,tarfile,difflib,os
b=Path(__file__).resolve().parent;p=b/'cherrypi-profile'
root=Path(os.environ.get('CHERRYPI_REPO_ROOT',str(b.parents[1])))
old=root/'components/gpsp/source/gpsp'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(p/'SHA256SUMS').write_text(''.join(sha(f)+'  '+f.name+'\n' for f in sorted(p.iterdir()) if f.is_file() and f.name!='SHA256SUMS'),newline='\n')
patch=[]
for f in sorted((b/'source/gpsp').rglob('*')):
 if not f.is_file():continue
 rel=f.relative_to(b/'source/gpsp');prev=old/rel
 if prev.exists() and prev.read_bytes()!=f.read_bytes():
  patch.extend(difflib.unified_diff(prev.read_text().splitlines(True),f.read_text().splitlines(True),fromfile='a/'+rel.as_posix(),tofile='b/'+rel.as_posix()))
(b/'fix.patch').write_text(''.join(patch),newline='\n')
archive=b/'cherrypi-memory-safe-20261017.tar.gz'
def attrs(m):
 m.uid=m.gid=0;m.uname=m.gname='root'
 m.mode=0o755 if m.isdir() or m.name.endswith('.sh') or m.name.endswith('/gpsp') else 0o644
 return m
with tarfile.open(archive,'w:gz') as t:
 t.add(p,arcname='cherrypi-profile',filter=attrs)
 for name in ('source','tests','build.sh','fix.patch','analyze.py'):
  t.add(b/name,arcname='cherrypi-profile/'+name,filter=attrs)
(b/'SHA256SUMS').write_text(sha(archive)+'  '+archive.name+'\n',newline='\n')
print(archive,archive.stat().st_size,'bytes; binary SHA256',sha(p/'gpsp'))
