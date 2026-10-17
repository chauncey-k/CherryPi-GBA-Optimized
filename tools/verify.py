"""Check the standalone tree, kit and every embedded SHA256SUMS manifest."""
from pathlib import Path,PurePosixPath
import hashlib,json,sys,tarfile,gzip
root=Path(__file__).resolve().parents[1]
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()
def rows(blob):
 for line in blob.decode('utf-8').splitlines():
  if not line:continue
  digest,name=line.split('  ',1);yield digest,name
manifest=json.loads((root/'RELEASE_MANIFEST.json').read_text(encoding='utf-8'))
for item in manifest['files']:
 p=root/item['path']
 if not p.is_file() or sha(p)!=item['sha256']:raise SystemExit('Mismatch/missing: '+item['path'])
kit=root/'sdcard-files/cherrypi-kit'
for path in kit.rglob('SHA256SUMS'):
 for expected,name in rows(path.read_bytes()):
  if sha(path.parent/name)!=expected:raise SystemExit('Kit checksum mismatch: '+name)
for archive in (kit/'packages').glob('*.tar.gz'):
 with tarfile.open(archive) as t:
  members={m.name:m for m in t.getmembers()}
  for name,m in members.items():
   n=PurePosixPath(name)
   if n.is_absolute() or '..' in n.parts or m.issym() or m.islnk():raise SystemExit('Unsafe archive member: '+name)
   if n.name.lower()=='gba_bios.bin' or n.suffix.lower() in ('.gba','.sav','.svs'):raise SystemExit('Private/game content in archive: '+name)
   if m.isfile() and n.name=='SHA256SUMS':
    for expected,rel in rows(t.extractfile(m).read()):
     target=str(n.parent/rel)
     if target not in members:raise SystemExit('Missing archived checksum member: '+target)
     if hashlib.sha256(t.extractfile(members[target]).read()).hexdigest()!=expected:raise SystemExit('Archived checksum mismatch: '+target)
 print('PASS archive:',archive.name)
if '--include-local' in sys.argv:
 meta=json.loads((root/'firmware/image.json').read_text(encoding='utf-8'));p=root/'firmware'/meta['compressed_name']
 if not p.is_file() or sha(p)!=meta['compressed_sha256']:raise SystemExit('Local compressed image missing/mismatch')
 h=hashlib.sha256();size=0
 with gzip.open(p,'rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk);size+=len(chunk)
 if size!=meta['size'] or h.hexdigest()!=meta['sha256']:raise SystemExit('Local expanded image checksum mismatch')
 print('PASS local base image:',size,'bytes')
print('PASS standalone manifest:',len(manifest['files']),'files; packages contain no BIOS/ROM/savestates')
