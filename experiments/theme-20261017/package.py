from pathlib import Path
import hashlib,tarfile,os,difflib
b=Path(__file__).resolve().parent;p=b if (b/'install.sh').is_file() else b/'theme'
r=Path(os.environ['CHERRYPI_REPO_ROOT'])
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
baselines=['3a3eaf7378a6a4ef69ffcd193e4e2ff0de3ff39c65347d231a963dd9d4de50b7',
 'ed5e89028201e22238d6bb1fd5f318bd4025009872ce1274ab425f88f278ebe6',sha(p/'cherrypi-launcher')]
(p/'MENU_BASELINES').write_text('\n'.join(baselines)+'\n',newline='\n')
(p/'COPYING').write_bytes((r/'COPYING').read_bytes())
if p!=b:(p/'README.md').write_bytes((b/'README.md').read_bytes())
for f in p.glob('*.sh'):f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
runtime=('cherrypi-launcher','install.sh','uninstall.sh','MENU_BASELINES','COPYING','README.md','SHA256SUMS')
(p/'SHA256SUMS').write_text(''.join(sha(p/n)+'  '+n+'\n' for n in sorted(runtime) if n!='SHA256SUMS'),newline='\n')
(b/'launcher.patch').write_text(''.join(difflib.unified_diff((r/'experiments/fastboot-stage1-r2-20261017/source/launcher.c').read_text().splitlines(True),(b/'source/launcher.c').read_text().splitlines(True),fromfile='a/launcher.c',tofile='b/launcher.c')),newline='\n')
archive=b/'cherrypi-theme-20261017.tar.gz'
def attrs(m):
 m.uid=m.gid=0;m.uname=m.gname='root';m.mode=0o755 if m.isdir() or m.name.endswith(('.sh','/cherrypi-launcher')) else 0o644
 return m
with tarfile.open(archive,'w:gz') as t:
 for n in runtime:t.add(p/n,arcname='cherrypi-theme/'+n,filter=attrs)
 for n in ('source','tests','build.sh','package.py','launcher.patch','make-previews.py','previews'):
  t.add(b/n,arcname='cherrypi-theme/'+n,filter=attrs)
(b/'SHA256SUMS').write_text(sha(archive)+'  '+archive.name+'\n',newline='\n')
print(archive,archive.stat().st_size,'bytes; menu SHA256',sha(p/'cherrypi-launcher'))
