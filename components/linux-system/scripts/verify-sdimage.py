#!/usr/bin/env python3
"""Check generated image structure and compare embedded artifacts; no device I/O."""
from pathlib import Path
import struct
import hashlib
import zlib
root = Path(__file__).resolve().parent.parent
images = root / 'build/images'
image = images / 'sdcard.img'

def check(condition, message):
    if not condition:
        raise SystemExit('FAIL: ' + message)
    print('PASS: ' + message)

def compare_range(handle, offset, artifact):
    handle.seek(offset)
    with artifact.open('rb') as src:
        while chunk := src.read(1024 * 1024):
            if handle.read(len(chunk)) != chunk:
                return False
    return True

with image.open('rb') as handle:
    mbr = handle.read(512)
    check(mbr[510:512] == b'\x55\xaa', 'MBR signature')
    parts = []
    for i in range(4):
        entry = mbr[446 + i * 16:462 + i * 16]
        start, count = struct.unpack_from('<II', entry, 8)
        if entry[4]:
            parts.append((entry[0], entry[4], start, count))
    check(len(parts) == 2, 'two MBR partitions')
    check(parts[0] == (0x80, 0x0c, 2048, 131072), '64 MiB FAT32 boot partition at 1 MiB')
    check(parts[1][1:] == (0x83, 133120, 524288), '256 MiB Linux root partition at 65 MiB')
    check(image.stat().st_size == 321 * 1024 * 1024, '321 MiB image size')
    bootloader = images / 'u-boot-sunxi-with-spl.bin'
    check(8192 + bootloader.stat().st_size <= 1024 * 1024, 'U-Boot fits before partition 1')
    handle.seek(8196)
    check(handle.read(8) == b'eGON.BT0', 'Allwinner SPL signature at 8 KiB')
    check(compare_range(handle, 8192, bootloader), 'embedded U-Boot matches build artifact')
    check(compare_range(handle, 1024 * 1024, images / 'boot.vfat'), 'embedded boot filesystem matches boot.vfat')
    check(compare_range(handle, 65 * 1024 * 1024, images / 'rootfs.ext4'), 'embedded root filesystem matches rootfs.ext4')
    handle.seek(65 * 1024 * 1024 + 1024 + 56)
    check(handle.read(2) == b'\x53\xef', 'ext4 superblock signature')

script = (images / 'boot.scr').read_bytes()
check(struct.unpack_from('>I', script)[0] == 0x27051956, 'U-Boot script image magic')
header = bytearray(script[:64])
header_crc = struct.unpack_from('>I', header, 4)[0]
header[4:8] = b'\0' * 4
check(zlib.crc32(header) & 0xffffffff == header_crc, 'boot.scr header CRC')
size = struct.unpack_from('>I', script, 12)[0]
data_crc = struct.unpack_from('>I', script, 24)[0]
check(len(script) == 64 + size, 'boot.scr payload size')
check(zlib.crc32(script[64:]) & 0xffffffff == data_crc, 'boot.scr payload CRC')
with image.open('rb') as handle:
    digest = hashlib.sha256()
    for chunk in iter(lambda: handle.read(1024 * 1024), b''):
        digest.update(chunk)
expected = (images / 'sdcard.img.sha256').read_text().split()[0]
check(digest.hexdigest() == expected, 'sdcard.img SHA-256')
print('Image structure verified; hardware boot remains unverified.')
