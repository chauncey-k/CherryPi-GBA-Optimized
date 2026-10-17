"""Run the shipped installer under BusyBox with a redirected temporary filesystem."""
from pathlib import Path
import hashlib,subprocess,tempfile,re,shutil,os
b=Path(__file__).resolve().parents[1];pkg=b/'motion' if (b/'motion').is_dir() else b
sha=lambda data:hashlib.sha256(data).hexdigest()
with tempfile.TemporaryDirectory() as d:
 r=Path(d)
 for n in ('root/gpsp/lib','root/cherrypi-menu-motion','usr/local/bin','mnt/games','mnt/boot','run','etc','mock'):(r/n).mkdir(parents=True,exist_ok=True)
 p=r/'root/cherrypi-menu-motion'
 for f in pkg.iterdir():
  if f.is_file():shutil.copy2(f,p/f.name)
 menu=r/'usr/local/bin/cherrypi-launcher';old=b'old fastboot menu\n';menu.write_bytes(old);menu.chmod(0o755)
 protected={n:('preserve '+n).encode() for n in ('root/gpsp/gpsp','root/gpsp/Game0.svs','etc/cherrypi-autostart.conf','mnt/boot/boot.scr','mnt/boot/suniv-f1c100s-generic.dtb','usr/local/bin/another-app','root/gpsp/lib/libpng16.so.16','root/gpsp/lib/libz.so.1')}
 for n,data in protected.items():(r/n).write_bytes(data)
 payload=(p/'cherrypi-launcher').read_bytes()
 (p/'MENU_BASELINES').write_text(sha(old)+'\n'+sha(payload)+'\n')
 (p/'SHA256SUMS').write_text(''.join(sha(f.read_bytes())+'  '+f.name+'\n' for f in sorted(p.iterdir()) if f.is_file() and f.name!='SHA256SUMS'))
 for n,body in (('pidof','exit 1'),('sync','exit 0')):
  f=r/'mock'/n;f.write_text('#!/bin/sh\n'+body+'\n');f.chmod(0o755)
 env=dict(os.environ,PATH=str(r/'mock')+':'+os.environ['PATH'])
 def run(name,ok=True):
  s=(pkg/name).read_text().replace('[ "$(id -u)" -eq 0 ]','true')
  s=re.sub(r'/(?:usr/local/bin|root|mnt|run|etc)(?=/)',lambda m:str(r)+m[0],s)
  f=p/('test-'+name);f.write_text(s)
  out=subprocess.run(['busybox','sh',str(f)],env=env,capture_output=True,text=True)
  assert (out.returncode==0)==ok,out.stdout+out.stderr
 marker=r/'mnt/games/DISABLE_GBA_AUTOSTART';marker.mkdir();run('install.sh',False);marker.rmdir();marker.touch()
 menu.write_bytes(b'unknown');run('install.sh',False);menu.write_bytes(old)
 (r/'run/cherrypi-autostart.ready').touch();run('install.sh',False);(r/'run/cherrypi-autostart.ready').unlink()
 (p/'cherrypi-launcher').write_bytes(b'corrupt');run('install.sh',False);(p/'cherrypi-launcher').write_bytes(payload)
 assert not (r/'root/cherrypi-menu-motion-20261017/state/backup-complete').exists()
 run('install.sh');run('install.sh');assert menu.read_bytes()==payload
 state=r/'root/cherrypi-menu-motion-20261017/state';assert (state/'cherrypi-launcher').read_bytes()==old
 for n,data in protected.items():assert (r/n).read_bytes()==data
 run('uninstall.sh');assert menu.read_bytes()==old
 for n,data in protected.items():assert (r/n).read_bytes()==data
 print('PASS install/reinstall/rollback; baseline, busy, marker and checksum rejection; other files retained')
