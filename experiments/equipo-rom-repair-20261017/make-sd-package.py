"""Package a private converted state; never add this archive to public Git."""
from pathlib import Path
import argparse,hashlib,io,json,tarfile
p=argparse.ArgumentParser(description=__doc__);p.add_argument('private_output',type=Path);a=p.parse_args()
b=a.private_output;m=json.loads((b/'PRIVATE-MANIFEST.json').read_text())
rom=b/m['rom_name'];state=b/m['state_name']
for f in (rom,state):
 if hashlib.sha256(f.read_bytes()).hexdigest()!=m['files'][f.name]['sha256']:raise SystemExit('Private output checksum mismatch')
script='''#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[ "$(id -u)" -eq 0 ] || { echo 'Root required';exit 1; }
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Exit game and launcher before installing';exit 1; }
[ -x /root/gpsp/gpsp ] || { echo 'Existing gpSP required';exit 1; }
(cd "$BASE" && sha256sum -c SHA256SUMS)
ROM='/mnt/games/roms/Equipo Rocket - repaired.gba'
[ -f "$ROM" ] || { echo "Copy repaired ROM first: $ROM";exit 1; }
printf '%s  %s\n' '@ROM_SHA@' "$ROM" | sha256sum -c -
TARGET='/root/gpsp/Equipo Rocket - repaired0.svs'
if [ -e "$TARGET" ]; then
    actual=$(sha256sum "$TARGET" | cut -d ' ' -f 1)
    [ "$actual" = '@STATE_SHA@' ] || { echo 'Existing repaired Slot 0 differs; preserved. Back it up before resetting the test.';exit 1; }
else
    cp "$BASE/Equipo Rocket - repaired0.svs" "$TARGET.new"
    chmod 600 "$TARGET.new"
    mv "$TARGET.new" "$TARGET"
fi
OLD_SAVE='/root/gpsp/Equipo Rocket.sav'
NEW_SAVE='/root/gpsp/Equipo Rocket - repaired.sav'
if [ -f "$OLD_SAVE" ] && [ ! -e "$NEW_SAVE" ]; then
    cp -p "$OLD_SAVE" "$NEW_SAVE.new"
    mv "$NEW_SAVE.new" "$NEW_SAVE"
fi
sync
echo 'Installed separate repaired Slot 0; original game and saves preserved.'
echo 'Select Equipo Rocket - repaired, load instant state Slot 0, then repeat the ball interaction.'
'''.replace('@ROM_SHA@',m['files'][rom.name]['sha256']).replace('@STATE_SHA@',m['files'][state.name]['sha256'])
files={state.name:state.read_bytes(),'install.sh':script.encode()}
files['SHA256SUMS']=''.join(hashlib.sha256(v).hexdigest()+'  '+k+'\n' for k,v in files.items()).encode()
path=b/'cherrypi-equipo-romfix-20261017.tar.gz'
with tarfile.open(path,'w:gz') as t:
 for name,data in files.items():
  info=tarfile.TarInfo('equipo-romfix-20261017/'+name);info.size=len(data);info.mode=0o755 if name.endswith('.sh') else 0o600
  t.addfile(info,io.BytesIO(data))
(b/'SHA256SUMS').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in (rom,state,path)),newline='\n')
print(path,path.stat().st_size,'bytes (private state included)')
