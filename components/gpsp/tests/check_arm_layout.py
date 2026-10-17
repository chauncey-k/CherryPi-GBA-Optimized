"""Ask the target compiler for actual serialized field sizes, without emulation."""
from pathlib import Path
import re,subprocess,tempfile,os
base=Path(__file__).resolve().parents[1];source=base/'source/gpsp'
host=Path(os.environ['CHERRYPI_SDK_HOST'])
cc=host/'bin/arm-linux-gcc';readelf=host/'bin/arm-linux-readelf'
sysroot=subprocess.check_output([str(cc),'--print-sysroot'],text=True).strip()
total=0
with tempfile.TemporaryDirectory() as directory:
    d=Path(directory)
    for name in ('cpu','input','main','memory','sound','video'):
        text=(source/(name+'.c')).read_text();a=text.index('#define '+name+'_savestate_builder(type)');b=text.index(name+'_savestate_builder(read)',a)
        block=text[a:b];terms=[]
        for kind,var in re.findall(r'file_##type##_(variable|array)\(savestate_file,\s*(\w+)\)',block):terms.append('sizeof('+var+')')
        for expr,size in re.findall(r'file_##type\(savestate_file,\s*(.*?),\s*(0x[0-9A-Fa-f]+)\)',block):terms.append('('+size+')'+('*8' if expr.startswith('ewram') else ''))
        expression='+'.join(terms);assert expression,(name,block)
        extra='\nconst unsigned char cp_layout_size['+expression+']={0};\n'
        if name=='main':extra+='const unsigned char cp_prefix_size[CP_STATE_PREVIEW+sizeof(time_t)]={0};\n'
        path=d/(name+'.c');path.write_text(text+extra)
        subprocess.run([str(cc),'-O0','-fcommon','-march=armv5te','-mfloat-abi=soft','-DARM_ARCH','-DMIYOO','-DCHERRYPI','-I'+str(source),'-I'+sysroot+'/usr/include/SDL','-I'+sysroot+'/usr/include/libpng16','-c',str(path),'-o',str(d/(name+'.o'))],check=True,capture_output=True)
        symbols=subprocess.check_output([str(readelf),'-sW',str(d/(name+'.o'))],text=True)
        def size_of(symbol):
            match=re.search(r'\s\d+:\s+\S+\s+(\S+)\s+OBJECT\s+GLOBAL\s+DEFAULT\s+\S+\s+'+symbol+r'\b',symbols);assert match,symbol
            return int(match[1],0) if match[1].startswith('0x') else int(match[1])
        n=size_of('cp_layout_size');total+=n;print(name,n)
        if name=='main':prefix=size_of('cp_prefix_size')
total+=prefix
print('Target prefix',prefix,'used',total,'capacity',506951,'legacy omitted',total-506947)
assert total==506951
print('PASS actual ARM legacy payload layout fits bounded staging buffer')
