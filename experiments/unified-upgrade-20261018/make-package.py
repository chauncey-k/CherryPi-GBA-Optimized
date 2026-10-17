"""Assemble verified existing components. No cross-compilation or private content."""
from pathlib import Path
import argparse,hashlib,json,tarfile,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);a=p.parse_args()
b=Path(__file__).resolve().parent;r=a.repo.resolve();sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
sources={
 'gpsp':'experiments/rom-mmap-20261017/cherrypi-profile/gpsp',
 'menu':'experiments/menu-motion-20261017/motion/cherrypi-launcher',
 'supervisor':'experiments/display-session-20261017/display-session/supervisor.sh',
 'display':'experiments/display-session-20261017/display-session/cherrypi-display-session',
 'control':'components/system-scripts/device-control.sh',
 'boot':'experiments/fastboot-stage1-r2-20261017/cherrypi-fastboot/boot.scr',
 'dtb':'experiments/fastboot-stage1-r2-20261017/cherrypi-fastboot/suniv-f1c100s-generic.dtb'}
expected={'gpsp':'b07165b2a3b31c036fb9ac57047e52a041aa2bcd98027798a5e69fe7efb6a1a7',
 'menu':'9696b5acde87dfe68db59a466c2ca3243ee52eddf72f8465032e6aa911c8bd73',
 'supervisor':'e58b0fd6dbaf6b6c5a0937b52847ee58829ad05bceb898ac9d1f153191c3bedd',
 'display':'175dc54d268dc611f149d905ff52194940d7618249998496c9ff8978467af173',
 'control':'95971fcab8743531547d4500944fdbc74579a05e623afec44a2792183aff3dfa',
 'boot':'e37524dc29139c6968cc690e796d5fa8840198ffecd748c2fb493b259cdfda1c',
 'dtb':'7c2a1a06fd6ad060a2c8cb721ca5f682b1bb40110a1c331d34ad5c3d05a8ae17'}
(b/'payload').mkdir(exist_ok=True);(b/'baselines').mkdir(exist_ok=True)
for key,path in sources.items():
 assert key not in expected or sha(r/path)==expected[key],key
 shutil.copyfile(r/path,b/'payload'/key)
release='VERSION=2026.10.18-unified\nMENU=2026.10.17-menu-motion\nGPSP=2026.10.17-rom-mmap\n'
(b/'payload/release').write_text(release,encoding='utf-8',newline='\n')
old={
 'gpsp':['923fb7fff27a3f0eb2bfc4b3599a62dd0149a478444ee8a521e49a563589f250','9b6df48b3e2aa5992de818f6b5012f2d333d598634a14d7ed2e1deef3c9ca26d'],
 'menu':['a39c074a08f57b567755d905d4148c3e9656341bccff28babaeaad35618945ad','3a3eaf7378a6a4ef69ffcd193e4e2ff0de3ff39c65347d231a963dd9d4de50b7','778493c890be55a283569e05619e65bc9c45ace36b4afef3ced2d3d527bba8c2','ed5e89028201e22238d6bb1fd5f318bd4025009872ce1274ab425f88f278ebe6'],
 'supervisor':[sha(r/'components/system-scripts/supervisor.sh')],
 'display':['absent'],'release':['absent'],
 'boot':(r/'experiments/fastboot-stage1-r2-20261017/cherrypi-fastboot/BOOT_BASELINES').read_text().split(),
 'dtb':(r/'experiments/fastboot-stage1-r2-20261017/cherrypi-fastboot/DTB_BASELINES').read_text().split()}
for key in [*sources,'release']:
 (b/'baselines'/key).write_text('\n'.join(sorted(set(old.get(key,[])+[sha(b/'payload'/key)])))+'\n',newline='\n')
shutil.copyfile(r/'COPYING',b/'COPYING')
source_paths={'gpsp':'experiments/rom-mmap-20261017/source/gpsp','menu':'experiments/menu-motion-20261017/source',
 'display':'experiments/display-session-20261017/source','supervisor':'experiments/display-session-20261017/source/supervisor.sh',
 'control':'components/system-scripts/device-control.sh','boot':'experiments/fastboot-stage1-r2-20261017/boot.cmd',
 'dtb':'experiments/fastboot-stage1-r2-20261017/build.sh'}
commit='b74c2e7fd2cf7b51a8fc5fe4ddf926b17ef997df'
mapping={'version':'2026.10.18-unified','source_commit':commit,'components':[
 {'key':k,'file':v,'sha256':sha(b/'payload'/k),'source':source_paths[k],
  'source_url':'https://github.com/chauncey-k/CherryPi-GBA-Optimized/tree/'+commit+'/'+source_paths[k]} for k,v in sources.items()]}
(b/'SOURCE_MAP.json').write_text(json.dumps(mapping,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
for f in b.rglob('*.sh'):f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
members=[f for name in ('payload','baselines') for f in sorted((b/name).rglob('*')) if f.is_file()]
members += [b/n for n in ('upgrade.sh','README.md','COPYING','SOURCE_MAP.json','make-package.py')]
(b/'SHA256SUMS').write_text(''.join(sha(f)+'  '+f.relative_to(b).as_posix()+'\n' for f in sorted(members)),newline='\n')
archive=b/'cherrypi-upgrade-20261018.tar.gz'
def attrs(m):
 m.uid=m.gid=0;m.uname=m.gname='root';m.mtime=1792252800;m.mode=0o755 if m.name.endswith('.sh') else 0o644
 return m
with tarfile.open(archive,'w:gz') as t:
 for f in members+[b/'SHA256SUMS']:
  t.add(f,arcname='cherrypi-upgrade-20261018/'+f.relative_to(b).as_posix(),filter=attrs)
(b/'ARCHIVE_SHA256SUMS').write_text(sha(archive)+'  '+archive.name+'\n',newline='\n')
print(archive,archive.stat().st_size,'bytes')
