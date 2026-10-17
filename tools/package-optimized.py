from pathlib import Path
import hashlib,tarfile
root=Path(__file__).resolve().parents[1];base=root/'components/gpsp';runtime=base/'cherrypi-profile'
files=sorted(p for p in runtime.iterdir() if p.is_file() and p.name!='SHA256SUMS')
(runtime/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in files),newline='\n')
archive=root/'sdcard-files/cherrypi-kit/packages/optimized-20261017.tar.gz'
def attrs(m):
 m.uid=m.gid=0;m.uname=m.gname='root'
 m.mode=0o755 if m.isdir() or m.name.endswith('.sh') or m.name.endswith('/gpsp') else 0o644
 return m
with tarfile.open(archive,'w:gz') as t:
 t.add(runtime,arcname='cherrypi-profile',filter=attrs)
 for name in ('source','tests','build.sh','fix.patch','analyze.py'):
  t.add(base/name,arcname='cherrypi-profile/'+name,filter=attrs)
kit=archive.parents[1]
(kit/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(kit).as_posix()+'\n' for p in sorted(kit.rglob('*')) if p.is_file() and p.name!='SHA256SUMS'),newline='\n')
print(archive,archive.stat().st_size)
