from pathlib import Path
import tempfile,subprocess,shutil,hashlib,os,re
b=Path(__file__).resolve().parent
package=b/'cherrypi-fastboot' if (b/'cherrypi-fastboot').is_dir() else b
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory() as d:
 root=Path(d)
 for n in ('root/gpsp','root/cherrypi-fastboot','mnt/boot','mnt/games','usr/local/bin','proc','mock'): (root/n).mkdir(parents=True,exist_ok=True)
 pkg=root/'root/cherrypi-fastboot'
 for f in package.iterdir():
  if f.is_file():shutil.copy2(f,pkg/f.name)
 originals={
  'mnt/boot/boot.scr':b'original boot script',
  'mnt/boot/suniv-f1c100s-generic.dtb':b'original device tree',
  'usr/local/bin/cherrypi-launcher':b'original launcher',
  'root/gpsp/gpsp':b'verified optimized gpsp unchanged'
 }
 for n,data in originals.items():(root/n).write_bytes(data)
 for name,orig,new in [('BOOT_BASELINES','mnt/boot/boot.scr','boot.scr'),('DTB_BASELINES','mnt/boot/suniv-f1c100s-generic.dtb','suniv-f1c100s-generic.dtb'),('MENU_BASELINES','usr/local/bin/cherrypi-launcher','cherrypi-launcher'),('GPSP_BASELINES','root/gpsp/gpsp',None)]:
  (pkg/name).write_text(sha(root/orig)+'\n'+(sha(pkg/new)+'\n' if new else ''))
 (pkg/'SHA256SUMS').write_text(''.join(sha(f)+'  '+f.name+'\n' for f in sorted(pkg.iterdir()) if f.is_file() and f.name!='SHA256SUMS'))
 (root/'proc/mounts').write_text(f'/dev/mmcblk0p1 {root}/mnt/boot vfat rw,relatime 0 0\n')
 (root/'mnt/games/DISABLE_GBA_AUTOSTART').touch()
 for name in ('pidof','sync'):
  f=root/'mock'/name;f.write_text('#!/bin/sh\nexit '+('1' if name=='pidof' else '0')+'\n');f.chmod(0o755)
 env=dict(os.environ,PATH=str(root/'mock')+':'+os.environ['PATH'])
 def run(name,ok=True):
  s=(package/name).read_text().replace('[ "$(id -u)" -eq 0 ]','true')
  s=re.sub(r'/(?:root|mnt|usr/local/bin|proc)(?=/)',lambda m:str(root)+m[0],s)
  path=pkg/('test-'+name);path.write_text(s)
  r=subprocess.run(['busybox','sh',str(path)],env=env,capture_output=True,text=True)
  assert (r.returncode==0)==ok,r.stdout+r.stderr
 # Preflight must reject a different boot configuration before creating backup.
 (root/'mnt/boot/boot.scr').write_bytes(b'unknown boot')
 run('install.sh',False);assert not (root/'root/cherrypi-fastboot-stage1/state').exists()
 (root/'mnt/boot/boot.scr').write_bytes(originals['mnt/boot/boot.scr'])
 run('install.sh')
 for dest,name in [('mnt/boot/boot.scr','boot.scr'),('mnt/boot/suniv-f1c100s-generic.dtb','suniv-f1c100s-generic.dtb'),('usr/local/bin/cherrypi-launcher','cherrypi-launcher')]:assert (root/dest).read_bytes()==(pkg/name).read_bytes()
 state=root/'root/cherrypi-fastboot-stage1/state'
 assert (state/'boot.scr').read_bytes()==originals['mnt/boot/boot.scr']
 run('install.sh');assert (state/'boot.scr').read_bytes()==originals['mnt/boot/boot.scr']
 assert (root/'root/gpsp/gpsp').read_bytes()==originals['root/gpsp/gpsp']
 run('uninstall.sh')
 for n,data in originals.items():assert (root/n).read_bytes()==data
 (pkg/'boot.scr').write_bytes(b'corrupt payload');run('install.sh',False)
 assert (root/'mnt/boot/boot.scr').read_bytes()==originals['mnt/boot/boot.scr']
 print('PASS installer: unknown baseline and corrupt payload rejected; atomic install, reinstall backup, rollback, optimized gpSP preserved')
