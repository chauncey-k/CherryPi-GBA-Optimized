from pathlib import Path
import hashlib,tarfile,shutil,difflib,json,os
b=Path(__file__).resolve().parent;p=b/'cherrypi-fastboot'
old=Path(os.environ.get('CHERRYPI_REPO_ROOT',str(b.parents[1])))
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
for name,paths in {
 'BOOT_BASELINES':[b/'boot-baseline.scr',p/'boot.scr'],
 'DTB_BASELINES':[b/'baseline.dtb',p/'suniv-f1c100s-generic.dtb'],
 'MENU_BASELINES':[p/'cherrypi-launcher'],
 'GPSP_BASELINES':[old/'components/gpsp/cherrypi-profile/gpsp']}.items():
 prefix='a39c074a08f57b567755d905d4148c3e9656341bccff28babaeaad35618945ad\n' if name=='MENU_BASELINES' else ''
 (p/name).write_text(prefix+''.join(sha(path)+'\n' for path in paths),newline='\n')
for f in p.glob('*.sh'):f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
(p/'SHA256SUMS').write_text(''.join(sha(f)+'  '+f.name+'\n' for f in sorted(p.iterdir()) if f.is_file() and f.name!='SHA256SUMS'),newline='\n')
(b/'launcher.patch').write_text(''.join(difflib.unified_diff((old/'components/main-menu/source/launcher.c').read_text().splitlines(True),(b/'source/launcher.c').read_text().splitlines(True),fromfile='a/launcher.c',tofile='b/launcher.c')),newline='\n')
archive=b/'cherrypi-fastboot-stage1-20261017.tar.gz'
def attrs(m):
 m.uid=m.gid=0;m.uname=m.gname='root';m.mode=0o755 if m.isdir() or m.name.endswith(('.sh','/cherrypi-launcher')) else 0o644
 return m
with tarfile.open(archive,'w:gz') as t:
 t.add(p,arcname='cherrypi-fastboot',filter=attrs)
 for name in ('source','tests','boot.cmd','build.sh','launcher.patch','test_payload.py','test_install.py','baseline.dtb','boot-baseline.scr'):
  if (b/name).exists():t.add(b/name,arcname='cherrypi-fastboot/'+name,filter=attrs)
(b/'SHA256SUMS').write_text(sha(archive)+'  '+archive.name+'\n',newline='\n')
print(archive,archive.stat().st_size,'bytes')
