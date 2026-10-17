"""On the PC: convert a user-supplied image to a bounded cover PNG. Requires Pillow."""
import argparse
from pathlib import Path
from PIL import Image,ImageOps
p=argparse.ArgumentParser(description='Convert JPG/PNG to a Cherry Pi cover; does not overwrite existing files.')
p.add_argument('image',type=Path);p.add_argument('output',type=Path,help='Output .png, named after the ROM without .gba')
a=p.parse_args()
if a.output.suffix.lower()!='.png':p.error('Output must end in .png')
with Image.open(a.image) as source:
 image=ImageOps.exif_transpose(source).convert('RGBA')
 image.thumbnail((224,288),Image.Resampling.LANCZOS)
 a.output.parent.mkdir(parents=True,exist_ok=True)
 with a.output.open('xb') as f:image.save(f,format='PNG',optimize=True)
print(a.output, image.size)
