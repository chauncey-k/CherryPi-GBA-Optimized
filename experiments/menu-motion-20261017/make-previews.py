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
sequence=['page-before','page-000','page-040','page-080','page-120','focus-000','focus-040','focus-080','focus-100']
frames=[]
for name in sequence:
 with Image.open(output/(name+'.png')) as image:
  frames.append(image.resize((640,480),Image.Resampling.NEAREST).convert('P',palette=Image.Palette.ADAPTIVE))
frames[0].save(output/'menu-motion.gif',save_all=True,append_images=frames[1:],duration=[600,40,40,40,600,40,40,20,700],loop=0,disposal=2,optimize=False)
print(output/'overview.png')
