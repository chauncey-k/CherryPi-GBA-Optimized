"""Run the real BusyBox lifecycle with mocked hardware tools and LCD helper."""
from pathlib import Path
import subprocess,tempfile,re
b=Path(__file__).resolve().parents[1]
def case(mode):
 with tempfile.TemporaryDirectory() as d:
  r=Path(d)
  for n in ('etc','run','tmp','mnt/games/roms','root/gpsp','root/audio-tools','usr/local/bin','usr/local/sbin','sbin','proc/bus/input'):(r/n).mkdir(parents=True,exist_ok=True)
  def put(n,data,execute=False):
   p=r/n;p.write_text(data)
   if execute:p.chmod(0o755)
  put('etc/cherrypi-autostart.conf','VOLUME=45\n')
  put('proc/mounts',f'/dev/mmcblk0p3 {r}/mnt/games vfat rw 0 0\n')
  put('proc/bus/input/devices','Name="cherrypi-gamepad"\n')
  (r/'root/gpsp/gba_bios.bin').write_bytes(bytes(16384))
  for n in ('root/gpsp/gpsp','root/audio-tools/amixer','root/audio-tools/audio-mode','usr/local/sbin/cherrypi-device-control'):put(n,'#!/bin/sh\nexit 0\n',True)
  put('root/gpsp/run-gpsp.sh','#!/bin/sh\nexport SDL_AUDIODRIVER=dsp\n',True)
  put('usr/local/sbin/cherrypi-display-session',f'''#!/bin/sh
echo "lcd:$1" >> '{r}/events'
if [ "$1" = acquire ]; then
 { 'exit 1' if mode=='helper-error' else ':' }
 touch '{r}/run/lcd-reserved'
else
 rm -f '{r}/run/lcd-reserved'
fi
''',True)
  put('sbin/getty',f"#!/bin/sh\ntest ! -f '{r}/run/lcd-reserved' || exit 99\necho getty >> '{r}/events'\n",True)
  put('sbin/poweroff',f'#!/bin/sh\necho poweroff >> \'{r}/events\'\nkill -TERM "$PPID"\n',True)
  if mode=='crash':body='exit 1'
  elif mode=='poweroff':body=f"touch '{r}/run/cherrypi-poweroff-request'\nexit 0"
  elif mode=='interrupt':body=f"touch '{r}/run/cherrypi-launcher-game'\nkill -INT \"$PPID\"\nexit 130"
  else:body=f'''if [ "$n" -eq 1 ]; then
touch '{r}/run/cherrypi-launcher-game'
exit {7 if mode=='game-error' else 0}
fi
{f"test -f '{r}/run/cherrypi-launcher-error' || exit 99" if mode=='game-error' else ':'}
touch '{r}/run/cherrypi-autostart.paused'
exit 0'''
  put('usr/local/bin/cherrypi-launcher',f'''#!/bin/sh
test -f '{r}/run/lcd-reserved' || exit 99
n=0
[ ! -f '{r}/count' ] || n=$(cat '{r}/count')
n=$((n+1));echo "$n" > '{r}/count'
echo launch >> '{r}/events'
{body}
''',True)
  if mode=='recovery':put('mnt/games/DISABLE_GBA_AUTOSTART','')
  if mode=='missing-helper':(r/'usr/local/sbin/cherrypi-display-session').unlink()
  s=(b/'source/supervisor.sh').read_text()
  s=re.sub(r'''/(?:usr/local/(?:sbin|bin)|usr/local|root|etc|run|tmp|mnt|proc|sbin)(?=/|[\s:"'])''',lambda m:str(r)+m[0],s)
  s=s.replace('[ -t 0 ]','true').replace('sleep 1',f"echo retry-delay >> '{r}/events'").replace('sleep 60','sleep 0.01')
  s=s.replace('    sync\n','    :\n')
  script=r/'supervisor.sh';script.write_text(s)
  out=subprocess.run(['busybox','sh',str(script)],capture_output=True,text=True,timeout=10)
  assert out.returncode==0,(mode,out.stdout,out.stderr)
  events=(r/'events').read_text().splitlines()
  expected={
   'game':['lcd:acquire','launch','launch','retry-delay','lcd:release','getty'],
   'game-error':['lcd:acquire','launch','launch','retry-delay','lcd:release','getty'],
   'interrupt':['lcd:acquire','launch','lcd:release','getty'],
   'crash':['lcd:acquire','launch','retry-delay','launch','retry-delay','launch','lcd:release','getty'],
   'poweroff':['lcd:acquire','launch','poweroff','lcd:release'],
   'recovery':['lcd:release','getty'],
   'helper-error':['lcd:acquire','lcd:release','getty'],
   'missing-helper':['getty']
  }[mode]
  assert events==expected,(mode,events,out.stdout,out.stderr)
  assert not (r/'run/cherrypi-autostart.ready').exists()
  assert not (r/'run/lcd-reserved').exists()
  print('PASS supervisor',mode)
for m in ('game','game-error','interrupt','crash','poweroff','recovery','helper-error','missing-helper'):case(m)
print('PASS game return has no retry delay or LCD release; serial interrupt, repeated failure and shutdown restore modes')
