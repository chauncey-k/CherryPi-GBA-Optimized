"""Mock filesystem install/reinstall/rollback; no actual ARM helper is run."""
from pathlib import Path
import hashlib,subprocess,tempfile,re,shutil,os
b=Path(__file__).resolve().parents[1];pkg=b/'display-session' if (b/'display-session').is_dir() else b
for previous_helper in (False,True):
 with tempfile.TemporaryDirectory() as d:
  r=Path(d)
  for n in ('root/gpsp','root/cherrypi-display-session','usr/local/sbin','usr/local/bin','mnt/games','mnt/boot','run','etc','mock'):(r/n).mkdir(parents=True,exist_ok=True)
  p=r/'root/cherrypi-display-session'
  for f in pkg.iterdir():
   if f.is_file():shutil.copy2(f,p/f.name)
  sup=r/'usr/local/sbin/cherrypi-autostart'
  # Use a synthetic baseline and update only the package allowlist/checksum.
  old=b'#!/bin/sh\n# original supervisor\n';sup.write_bytes(old);sup.chmod(0o755)
  protected={n:('keep '+n).encode() for n in ('root/gpsp/gpsp','root/gpsp/Original0.svs','usr/local/bin/cherrypi-launcher','mnt/boot/boot.scr','mnt/boot/suniv-f1c100s-generic.dtb','etc/cherrypi-autostart.conf')}
  for n,data in protected.items():(r/n).write_bytes(data);(r/n).chmod(0o755)
  (r/'etc/inittab').write_text(f'ttyS0::respawn:{r}/usr/local/sbin/cherrypi-autostart\n')
  helper=r/'usr/local/sbin/cherrypi-display-session'
  if previous_helper:shutil.copy2(p/'cherrypi-display-session',helper);helper.chmod(0o755)
  (p/'SUPERVISOR_BASELINES').write_text(hashlib.sha256(old).hexdigest()+'\n'+hashlib.sha256((p/'supervisor.sh').read_bytes()).hexdigest()+'\n')
  (p/'SHA256SUMS').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in sorted(p.iterdir()) if f.is_file() and f.name!='SHA256SUMS'))
  for name,body in (('pidof','exit 1'),('sync','exit 0'),('lcd-release','exit 0')):
   f=r/'mock'/name;f.write_text('#!/bin/sh\n'+body+'\n');f.chmod(0o755)
  env=dict(os.environ,PATH=str(r/'mock')+':'+os.environ['PATH'])
  def run(name,ok=True):
   s=(pkg/name).read_text().replace('[ "$(id -u)" -eq 0 ]','true')
   s=re.sub(r'/(?:usr/local/(?:sbin|bin)|root|mnt|run|etc)(?=/)',lambda m:str(r)+m[0],s)
   s=s.replace(str(helper)+' release',str(r/'mock/lcd-release')+' release')
   f=p/('test-'+name);f.write_text(s)
   out=subprocess.run(['busybox','sh',str(f)],env=env,capture_output=True,text=True)
   assert (out.returncode==0)==ok,out.stdout+out.stderr
  marker=r/'mnt/games/DISABLE_GBA_AUTOSTART';marker.mkdir();run('install.sh',False);marker.rmdir();marker.touch()
  sup.write_bytes(b'unsupported supervisor');run('install.sh',False);sup.write_bytes(old)
  (r/'run/cherrypi-autostart.ready').touch();run('install.sh',False);(r/'run/cherrypi-autostart.ready').unlink()
  payload=(p/'cherrypi-display-session').read_bytes();(p/'cherrypi-display-session').write_bytes(b'bad');run('install.sh',False);(p/'cherrypi-display-session').write_bytes(payload)
  assert not (r/'root/cherrypi-display-session-20261017/state/backup-complete').exists()
  run('install.sh');run('install.sh')
  assert sup.read_bytes()==(p/'supervisor.sh').read_bytes() and helper.read_bytes()==payload
  state=r/'root/cherrypi-display-session-20261017/state';assert (state/'supervisor.sh').read_bytes()==old
  for n,data in protected.items():assert (r/n).read_bytes()==data
  helper.write_bytes(b'unknown helper');run('install.sh',False);assert helper.read_bytes()==b'unknown helper';helper.write_bytes(payload)
  run('uninstall.sh');assert sup.read_bytes()==old
  assert helper.exists()==previous_helper
  if previous_helper:assert helper.read_bytes()==payload
  for n,data in protected.items():assert (r/n).read_bytes()==data
  print('PASS installer baseline/marker/busy/checksum rejection, backup/reinstall/rollback; previous helper:',previous_helper)
