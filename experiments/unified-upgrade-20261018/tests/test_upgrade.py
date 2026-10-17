"""Execute the real installer under BusyBox against an isolated filesystem."""
from pathlib import Path,PurePosixPath
import tempfile,tarfile,subprocess,hashlib,shutil,os,re
b=Path(__file__).resolve().parents[1]
archive=b/'cherrypi-upgrade-20261018.tar.gz'
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
paths={'gpsp':'root/gpsp/gpsp','menu':'usr/local/bin/cherrypi-launcher','supervisor':'usr/local/sbin/cherrypi-autostart',
 'display':'usr/local/sbin/cherrypi-display-session','control':'usr/local/sbin/cherrypi-device-control',
 'boot':'mnt/boot/boot.scr','dtb':'mnt/boot/suniv-f1c100s-generic.dtb','release':'etc/cherrypi-release'}
class Rig:
 def __init__(self,helper=False):
  self.tmp=tempfile.TemporaryDirectory(prefix='cherrypi-unified-test-');self.r=Path(self.tmp.name)
  with tarfile.open(archive) as t:
   for m in t.getmembers():
    p=PurePosixPath(m.name)
    assert not p.is_absolute() and '..' not in p.parts and not m.issym() and not m.islnk()
    assert p.suffix.lower() not in ('.gba','.sav','.svs') and p.name.lower()!='gba_bios.bin'
   t.extractall(self.r)
  self.pkg=self.r/'cherrypi-upgrade-20261018'
  self.manifest=[line.split('  ',1)[1] for line in (self.pkg/'SHA256SUMS').read_text().splitlines()]
  assert all(sha(self.pkg/n)==line.split('  ',1)[0] for n,line in zip(self.manifest,(self.pkg/'SHA256SUMS').read_text().splitlines()))
  for n in ('root/gpsp/lib','root/audio-tools','mnt/games/roms','mnt/boot','usr/local/bin','usr/local/sbin','etc','run','proc','mock'):(self.r/n).mkdir(parents=True,exist_ok=True)
  self.old={}
  for key,path in paths.items():
   f=self.r/path
   if key=='release' or (key=='display' and not helper):
    self.old[key]=None
   else:
    self.old[key]=('original '+key).encode();f.write_bytes(self.old[key]);f.chmod(0o755)
   with (self.pkg/'baselines'/key).open('a') as out:out.write((sha(f) if f.exists() else 'absent')+'\n')
  self.protected={n:('private preserved '+n).encode() for n in ('etc/cherrypi-autostart.conf','root/gpsp/Game0.svs','root/gpsp/Game.sav','root/.cherrypi-recent','mnt/games/roms/Game.gba','mnt/boot/zImage')}
  for n,data in self.protected.items():(self.r/n).write_bytes(data)
  for n in ('root/gpsp/run-gpsp.sh','root/audio-tools/amixer','root/audio-tools/audio-mode'):
   f=self.r/n;f.write_text('#!/bin/sh\nexit 0\n');f.chmod(0o755)
  for n in ('libSDL-1.2.so.0','libpng16.so.16','libz.so.1'):(self.r/'root/gpsp/lib'/n).write_text('library')
  (self.r/'root/gpsp/gba_bios.bin').write_bytes(bytes(16384))
  (self.r/'etc/inittab').write_text(f'ttyS0::respawn:{self.r}/usr/local/sbin/cherrypi-autostart\n')
  (self.r/'proc/mounts').write_text(f'/dev/mmcblk0p1 {self.r}/mnt/boot vfat rw,relatime 0 0\n')
  (self.r/'mnt/games/DISABLE_GBA_AUTOSTART').touch()
  self.state=self.r/'root/cherrypi-unified-20261018';self.first=self.state/'first-backup';self.pending=self.state/'pending'
  for name,body in {'pidof':'[ "${RUNNING:-0}" = 1 ]','sync':'exit 0','df':"printf 'Filesystem 1K-blocks Used Available Use%% Mounted\\nmock 100000 1000 %s 1%% /\\n' \"${FREE_KB:-90000}\"",'mv':
'''if [ "$2" = "${CRASH_ON:-}" ] && [ "$1" = "$2.unified-new" ]; then kill -KILL "$PPID";exit 99;fi
if { [ "$2" = "${FAIL_ON:-}" ] && [ "$1" = "$2.unified-new" ]; } || { [ "$2" = "${FAIL_RESTORE_ON:-}" ] && [ "$1" = "$2.unified-restore" ]; }; then exit 71;fi
exec /bin/mv "$@"'''} .items():
   f=self.r/'mock'/name;f.write_text('#!/bin/sh\n'+body+'\n');f.chmod(0o755)
  s=(self.pkg/'upgrade.sh').read_text().replace('[ "$(id -u)" -eq 0 ]','true')
  s=re.sub(r'/(?:usr/local/(?:bin|sbin)|root|mnt|run|etc|proc)(?=/)',lambda m:str(self.r)+m[0],s)
  # This host BusyBox resolves standalone applets before PATH. Shell functions
  # explicitly route only the fault-injected host commands into our mock tree.
  adapters='\n'.join(name+'() { "'+str(self.r/'mock'/name)+'" "$@"; }' for name in ('df','mv','pidof','sync'))
  s=s.replace('set -eu','set -eu\n'+adapters,1)
  self.runner=self.pkg/'.test-runner.sh';self.runner.write_text(s)
  self.env=dict(os.environ,PATH=str(self.r/'mock')+':'+os.environ['PATH'])
  self.rehash()
 def rehash(self):
  (self.pkg/'SHA256SUMS').write_text(''.join(sha(self.pkg/n)+'  '+n+'\n' for n in self.manifest))
 def run(self,action,ok=True,**extra):
  out=subprocess.run(['busybox','sh',str(self.runner),action],env=dict(self.env,**extra),capture_output=True,text=True)
  assert (out.returncode==0)==ok,(action,out.returncode,out.stdout,out.stderr)
  self.preserve();return out
 def preserve(self):
  for n,data in self.protected.items():assert (self.r/n).read_bytes()==data,n
 def original(self):
  for key,path in paths.items():
   f=self.r/path
   assert (f.read_bytes() if f.exists() else None)==self.old[key],key
 def installed(self):
  for key,path in paths.items():assert (self.r/path).read_bytes()==(self.pkg/'payload'/key).read_bytes(),key
 def close(self):self.tmp.cleanup()

for helper in (False,True):
 q=Rig(helper)
 q.run('status');q.original()
 q.run('install',False,RUNNING='1');q.original()
 marker=q.r/'mnt/games/DISABLE_GBA_AUTOSTART';marker.unlink();marker.mkdir();q.run('install',False);marker.rmdir();marker.touch()
 (q.r/'run/cherrypi-autostart.ready').touch();q.run('install',False);(q.r/'run/cherrypi-autostart.ready').unlink()
 q.run('install',False,FREE_KB='1')
 mounts=q.r/'proc/mounts';mountdata=mounts.read_text();mounts.write_text(mountdata.replace('vfat rw,','vfat ro,'));q.run('install',False);mounts.write_text(mountdata)
 scratch=q.r/(paths['menu']+'.unified-new');scratch.symlink_to(q.r/'etc/cherrypi-autostart.conf');q.run('install',False);scratch.unlink()
 menu=q.r/paths['menu'];old=menu.read_bytes();menu.write_bytes(b'unknown');q.run('install',False);menu.write_bytes(old)
 bios=q.r/'root/gpsp/gba_bios.bin';bios.write_bytes(b'bad');q.run('install',False);bios.write_bytes(bytes(16384))
 payload=q.pkg/'payload/menu';data=payload.read_bytes();payload.write_bytes(b'bad');q.run('install',False);payload.write_bytes(data)
 assert not q.first.exists()
 q.run('install');q.installed();first_hash={f.name:sha(f) for f in q.first.iterdir() if f.is_file()}
 q.run('install');q.installed();assert first_hash=={f.name:sha(f) for f in q.first.iterdir() if f.is_file()}
 q.run('rollback');q.original();q.run('install');q.installed()
 menu.write_bytes(b'other later update');q.run('rollback',False);assert menu.read_bytes()==b'other later update';menu.write_bytes(data)
 backup=q.first/'gpsp';old=backup.read_bytes();backup.write_bytes(b'corrupt');q.run('rollback',False);q.installed();backup.write_bytes(old)
 q.run('rollback');q.original();q.close()
print('PASS preflight/baselines/package/space, first backup/reinstall/rollback, helper absence/presence; user data unchanged')

for key in paths:
 q=Rig();q.run('install',False,FAIL_ON=str(q.r/paths[key]));q.original()
 assert not (q.pending/'APPLYING').exists();q.run('install');q.installed();q.run('rollback');q.original();q.close()
print('PASS injected failure at every target write automatically restores transaction state')

q=Rig();q.run('install',False,CRASH_ON=str(q.r/paths['dtb']))
assert (q.pending/'APPLYING').exists();assert 'RECOVERY REQUIRED' in q.run('status').stdout
# SIGKILL intentionally leaves the lock; reboot would clear /run.
(q.r/'run/cherrypi-upgrade.lock').rmdir()
q.run('install',False);q.run('recover');q.original();q.run('install');q.installed();q.close()
print('PASS SIGKILL recovery refuses normal install until pending backup restored')

q=Rig();q.run('install',False,FAIL_ON=str(q.r/paths['menu']),FAIL_RESTORE_ON=str(q.r/paths['gpsp']))
assert (q.pending/'APPLYING').exists();q.run('recover');q.original();q.close()
q=Rig();q.run('install');q.run('rollback',False,FAIL_RESTORE_ON=str(q.r/paths['menu']))
assert (q.pending/'APPLYING').exists();q.run('recover');q.installed();q.close()
print('PASS rollback/recovery write failure retains transaction backup for retry')
