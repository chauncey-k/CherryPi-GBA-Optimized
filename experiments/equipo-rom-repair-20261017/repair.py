"""Repair two known bag-category string pointers in one exact user ROM.
GPL-2.0-or-later. No ROM, text payload, BIOS or user state is distributed here.
"""
from pathlib import Path
import argparse, hashlib, json, struct, zlib

ROM_SHA1='60b3c1b3d5b94d0a1ec2f2662c55b86d9a259d9a'
FIXED_ROM_SHA1='5ad0baa726e15414738b9597caebdb80d7373ef8'
STATE_SHA256='67a9ce43048e7b3e59f98b12d8a72830dada54367bcc829503b2cfff84796e75'
ROM_NAME='Equipo Rocket - repaired.gba'
STATE_NAME='Equipo Rocket - repaired0.svs'
LIVE_NAME='/mnt/games/roms/'+ROM_NAME
NAME_OFFSET=77563
PATCHES=((0xc4c4fc,bytes.fromhex('4a4b4c4d'),0x08cb9a41,'MEDICINAS'),
         (0xc4c500,bytes.fromhex('4e4f0000'),0x08cb9a4b,'OBJ. BATALLA'))

def repair_rom(data):
    if hashlib.sha1(data).hexdigest()!=ROM_SHA1:
        raise ValueError('Unsupported ROM SHA1; no bytes were repaired')
    out=bytearray(data)
    for off,expected,target,label in PATCHES:
        if data[off:off+4]!=expected: raise ValueError('Unexpected patch bytes')
        # The existing Latin text must be bounded and terminated in this ROM.
        pos=target-0x08000000
        if b'\xff' not in data[pos:pos+32]: raise ValueError('Text terminator missing')
        struct.pack_into('<I',out,off,target)
    return bytes(out)

def checked_state(data):
    if len(data)!=507015 or data[:8]!=b'CPSTATE2': raise ValueError('Unsupported state')
    h,p=data[:64],data[64:]
    if struct.unpack_from('<I',h,8)[0]!=1: raise ValueError('State version mismatch')
    if struct.unpack_from('<II',h,12)!=(506951,0xa89e4532): raise ValueError('State layout mismatch')
    if struct.unpack_from('<I',h,60)[0]!=zlib.crc32(h[:60]): raise ValueError('State header CRC mismatch')
    if struct.unpack_from('<I',h,20)[0]!=zlib.crc32(p): raise ValueError('State payload CRC mismatch')
    return h,p

def convert_state(data,new_rom):
    if hashlib.sha1(new_rom).hexdigest()!=FIXED_ROM_SHA1: raise ValueError('State conversion requires the exact 8-byte repaired ROM')
    h,p=checked_state(data)
    if hashlib.sha256(data).hexdigest()!=STATE_SHA256: raise ValueError('Use the verified before-interaction state')
    if h[24:44].hex()!=ROM_SHA1: raise ValueError('Original state ROM mismatch')
    old_name=p[NAME_OFFSET:NAME_OFFSET+512].split(b'\0',1)[0]
    if old_name.split(b'/')[-1]!=b'Equipo Rocket.gba': raise ValueError('Original ROM name mismatch')
    out=bytearray(data)
    name=LIVE_NAME.encode('ascii')
    out[64+NAME_OFFSET:64+NAME_OFFSET+512]=name+b'\0'*(512-len(name))
    out[24:44]=hashlib.sha1(new_rom).digest()
    struct.pack_into('<I',out,20,zlib.crc32(out[64:]))
    struct.pack_into('<I',out,60,zlib.crc32(out[:60]))
    checked_state(out)
    return bytes(out)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',required=True,type=Path)
    p.add_argument('--state',required=True,type=Path)
    p.add_argument('--out',required=True,type=Path)
    a=p.parse_args()
    try:
        original=a.rom.read_bytes();state=a.state.read_bytes()
        fixed=repair_rom(original);converted=convert_state(state,fixed)
        if a.out.exists(): raise ValueError('Output folder already exists; choose a new folder')
        a.out.mkdir(parents=True)
        files={ROM_NAME:fixed,STATE_NAME:converted}
        for name,data in files.items():
            with (a.out/name).open('xb') as f:f.write(data)
        manifest={'original_rom_sha1':ROM_SHA1,'fixed_rom_sha1':hashlib.sha1(fixed).hexdigest(),
          'original_state_sha256':STATE_SHA256,'rom_name':ROM_NAME,'state_name':STATE_NAME,
          'patches':[{'offset':hex(o),'before':old.hex(),'after':struct.pack('<I',target).hex(),'text':label} for o,old,target,label in PATCHES],
          'files':{name:{'size':len(data),'sha256':hashlib.sha256(data).hexdigest()} for name,data in files.items()}}
        (a.out/'PRIVATE-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
        print('Created private repair copies:',a.out)
        print('Fixed ROM SHA1:',manifest['fixed_rom_sha1'])
    except (OSError,ValueError) as e:p.exit(1,str(e)+'\n')

if __name__=='__main__':main()
