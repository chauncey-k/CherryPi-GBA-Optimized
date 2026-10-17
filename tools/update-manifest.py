from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
files=[]
for p in sorted(root.rglob('*')):
 if not p.is_file():continue
 rel=p.relative_to(root)
 if any(x in rel.parts for x in ('.git','build','logs','local','__pycache__')) or p.name=='RELEASE_MANIFEST.json':continue
 if p.name.endswith(('.img','.img.gz','.partial','.pyc')):continue
 if p.suffix.lower() in ('.gba','.sav','.svs') or p.name.lower()=='gba_bios.bin':continue
 if rel.parts[:2]==('sdcard-files','roms') and p.name!='README.txt':continue
 files.append({'path':rel.as_posix(),'size':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(root/'RELEASE_MANIFEST.json').write_text(json.dumps({'version':'2026.10.17-smc-live','files':files},ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print('Manifest:',len(files),'files')
