from pathlib import Path
import struct,zlib,hashlib
b=Path(__file__).resolve().parent
def tree(path):
    data=path.read_bytes();h=struct.unpack_from('>10I',data);assert h[0]==0xd00dfeed
    at=h[2];strings=data[h[3]:h[3]+h[8]];stack=[];properties={}
    while True:
        token=struct.unpack_from('>I',data,at)[0];at+=4
        if token==1:
            end=data.index(0,at);stack.append(data[at:end].decode());at=(end+4)&~3
        elif token==2:stack.pop()
        elif token==3:
            size,name=struct.unpack_from('>2I',data,at);at+=8
            key=strings[name:strings.index(0,name)].decode()
            properties[('/'+'/'.join(stack[1:]),key)]=data[at:at+size];at=(at+size+3)&~3
        elif token==4:pass
        elif token==9:break
        else:raise AssertionError(token)
    return properties
old=tree(b/'baseline.dtb');new=tree(b/'cherrypi-fastboot/suniv-f1c100s-generic.dtb')
changed={k for k in old.keys()|new.keys() if old.get(k)!=new.get(k)}
assert changed=={('/soc/spi@1c05000','status'),('/soc/video-codec@1c0e000','status')},changed
assert all(new[k]==b'disabled\0' for k in changed)
print('PASS DTB: exactly two status properties changed; all other properties and phandles preserved')
def script(path):
    d=path.read_bytes();h=list(struct.unpack_from('>7I4B32s',d));assert h[0]==0x27051956
    header=bytearray(d[:64]);header[4:8]=bytes(4)
    assert zlib.crc32(header)&0xffffffff==h[1]
    payload=d[64:];assert len(payload)==h[3] and zlib.crc32(payload)&0xffffffff==h[6]
    assert struct.unpack_from('>I',payload)[0]==len(payload)-8
    return payload[8:].decode()
before=script(b/'boot-baseline.scr');after=script(b/'cherrypi-fastboot/boot.scr')
assert after==before.replace('earlycon=uart8250,mmio32,0x01c25400 ignore_loglevel','loglevel=4')
assert 'rootwait' in after and 'root=/dev/mmcblk0p2' in after and 'console=ttyS0,115200' in after
print('PASS U-Boot script header/data CRC, preserved addresses/rootwait/console; only debug args replaced')
