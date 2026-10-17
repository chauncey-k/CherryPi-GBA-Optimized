"""Check and extract the final deliverable; rerun tests from the extracted tree."""
from pathlib import Path,PurePosixPath
import tarfile,hashlib,tempfile,subprocess,os
b=Path(__file__).resolve().parents[1]
archive=b/'cherrypi-menu-motion-20261017.tar.gz'
with tempfile.TemporaryDirectory(prefix='cherrypi-menu-motion-check-') as d:
 with tarfile.open(archive) as t:
  members=t.getmembers()
  for m in members:
   p=PurePosixPath(m.name)
   assert not p.is_absolute() and '..' not in p.parts and not m.issym() and not m.islnk()
   assert p.suffix.lower() not in ('.gba','.sav','.svs') and p.name.lower()!='gba_bios.bin'
  t.extractall(d)
 p=Path(d)/'cherrypi-menu-motion'
 for line in (p/'SHA256SUMS').read_text().splitlines():
  expected,name=line.split('  ',1)
  assert hashlib.sha256((p/name).read_bytes()).hexdigest()==expected,name
 subprocess.run(['bash',str(p/'tests/run.sh')],check=True)
 print('PASS extracted final archive: safe paths, checksums, theme/menu/install tests')
