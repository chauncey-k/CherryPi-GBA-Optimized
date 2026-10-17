"""Convert actual host-rendered menu frames into shareable PNGs. PC: Pillow required."""
from pathlib import Path
from PIL import Image
b=Path(__file__).resolve().parent
source=b/'build/previews';output=b/'previews';output.mkdir(exist_ok=True)
for f in source.glob('*.ppm'):
 with Image.open(f) as image:image.save(output/(f.stem+'.png'))
names=['home','games-page','settings','poweroff']
canvas=Image.new('RGB',(1280,960))
for i,name in enumerate(names):
 with Image.open(output/(name+'.png')) as image:
  canvas.paste(image.resize((640,480),Image.Resampling.NEAREST),((i%2)*640,(i//2)*480))
canvas.save(output/'overview.png')
print(output/'overview.png')
