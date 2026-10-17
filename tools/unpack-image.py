"""Verify and decompress the local base image without touching a physical disk."""
from pathlib import Path
import hashlib,gzip,json,os,shutil
root=Path(__file__).resolve().parents[1];folder=root/'firmware'
meta=json.loads((folder/'image.json').read_text(encoding='utf-8'))
source=folder/meta['compressed_name'];target=folder/meta['name']
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()
if not source.is_file():raise SystemExit('Local image missing: read firmware/README.md; Git does not include this file.')
if sha(source)!=meta['compressed_sha256']:raise SystemExit('Compressed image checksum mismatch')
if target.exists():
 if target.stat().st_size==meta['size'] and sha(target)==meta['sha256']:print('Existing image verified:',target);raise SystemExit(0)
 raise SystemExit('Refusing to overwrite an existing, different image')
if shutil.disk_usage(folder).free<meta['size']+16*1024*1024:raise SystemExit('Not enough free space for 321MiB output')
temp=target.with_suffix('.img.partial')
if temp.exists():raise SystemExit('Existing partial file found; inspect it before retrying')
h=hashlib.sha256();size=0
try:
 with gzip.open(source,'rb') as src,temp.open('xb') as dst:
  for chunk in iter(lambda:src.read(1024*1024),b''):
   size+=len(chunk)
   if size>meta['size']:raise ValueError('Expanded image larger than expected')
   h.update(chunk);dst.write(chunk)
  dst.flush();os.fsync(dst.fileno())
 if size!=meta['size'] or h.hexdigest()!=meta['sha256']:raise ValueError('Expanded image checksum/size mismatch')
 temp.rename(target)
except Exception:
 if temp.exists():temp.unlink()
 raise
print('Verified image:',target,'bytes:',size,'SHA256:',h.hexdigest())
