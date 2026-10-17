"""Generate tiny synthetic PNGs, with no game artwork or ROM required."""
from pathlib import Path
import struct,zlib,binascii
root=Path('fixtures');root.mkdir(exist_ok=True)
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',binascii.crc32(kind+data)&0xffffffff)
def png(w,h,depth,color,pixel,extra=b'',interlace=0):
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,depth,color,0,0,interlace))+extra+chunk(b'IDAT',zlib.compress((b'\0'+pixel*w)*h))+chunk(b'IEND',b'')
images={
 'rgb':png(16,16,8,2,b'\xff\0\0'), 'rgba':png(16,16,8,6,b'\xff\0\0\0'),
 'palette':png(16,16,8,3,b'\0',chunk(b'PLTE',b'\0\xff\0')),
 'gray':png(16,16,8,0,b'\x80'), 'gray16':png(16,16,16,0,b'\x80\0'),
 'wide':png(512,1,8,2,b'\xff\0\0'), 'tall':png(1,512,8,2,b'\xff\0\0'),
 'oversized':png(513,1,8,2,b'\xff\0\0'),
 # A 1x1 Adam7 image has only the first nonempty pass.
 'interlaced':png(1,1,8,2,b'\0\0\xff',interlace=1),
}
images['truncated']=images['rgb'][:35]
bad=bytearray(images['rgb']);bad[-5]^=0x80;images['bad-crc']=bytes(bad)
images['huge-file']=images['rgb']+b'\0'*(512*1024)
for name,data in images.items():(root/(name+'.png')).write_bytes(data)
for name,data in {'roms/covers/Test':images['rgb'],'covers/Test':images['palette'],'covers/Global':images['palette'],'covers/Beta':images['palette'],'covers/Bad':images['truncated']}.items():
 p=Path(name+'.png');p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
