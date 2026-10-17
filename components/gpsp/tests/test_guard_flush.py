"""Exercise a real incomplete ARM translation and inspect its guard cleanup.
No user ROM is required: input consists of synthetic NOPs and BX LR.
"""
from pathlib import Path
import struct, sys
base=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(base/'local/test-deps'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
path=Path(sys.argv[1]) if len(sys.argv)>1 else base/'build/gpsp'
uc=Uc(UC_ARCH_ARM,UC_MODE_ARM);uc.ctl_set_cpu_model(UC_CPU_ARM_926)
pack=lambda n:struct.pack('<I',n&0xffffffff)
with path.open('rb') as f:
    elf=ELFFile(f)
    loads=[s for s in elf.iter_segments() if s['p_type']=='PT_LOAD']
    low=min(s['p_vaddr'] for s in loads)&~4095
    high=(max(s['p_vaddr']+s['p_memsz'] for s in loads)+4095)&~4095
    uc.mem_map(low,high-low)
    for s in loads:uc.mem_write(s['p_vaddr'],s.data())
    syms={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    hooks={}
    uc.mem_map(0x50000000,0x10000)
    for section in elf.iter_sections():
        if section.name!='.rel.plt':continue
        dyn=elf.get_section(section['sh_link'])
        for rel in section.iter_relocations():
            name=dyn.get_symbol(rel['r_info_sym']).name
            address=0x50000000+len(hooks)*16
            hooks[address]=name;uc.mem_write(rel['r_offset'],pack(address))
uc.mem_map(0x30000000,0x10000)
uc.mem_map(0x31000000,0x1000)
def hook(uc,pc,size,unused):
    if pc==0x31000000:uc.emu_stop();return
    if pc not in hooks:return
    name=hooks[pc];a=uc.reg_read(UC_ARM_REG_R0);b=uc.reg_read(UC_ARM_REG_R1);n=uc.reg_read(UC_ARM_REG_R2)
    if name=='memset':uc.mem_write(a,bytes([b&255])*n)
    elif name in ('memcpy','memmove'):uc.mem_write(a,bytes(uc.mem_read(b,n)))
    else:raise RuntimeError(name)
    uc.reg_write(UC_ARM_REG_R0,a);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,hook,begin=0x50000000,end=0x5000ffff)
uc.hook_add(UC_HOOK_CODE,hook,begin=0x31000000,end=0x31000000)
def set32(name,value):uc.mem_write(syms[name],pack(value))
ir=syms['iwram']
uc.mem_write(syms['memory_map_read']+(0x03000000>>15)*4,pack(ir+0x8000))
uc.mem_write(ir+0x1000,bytes([255])*0x100) # a previously translated small block
uc.mem_write(ir+0x8000+0x2000,pack(0xe1a00000)*200+pack(0xe12fff1e))
set32('iwram_code_min',0x03001000);set32('iwram_code_max',0x03001100)
set32('ram_translation_ptr',syms['ram_translation_cache']+786432-32768-4)
if 'ram_guard_dirty_pages' in syms:set32('ram_guard_dirty_pages',1<<8)
uc.reg_write(UC_ARM_REG_SP,0x30008000);uc.reg_write(UC_ARM_REG_LR,0x31000000)
uc.reg_write(UC_ARM_REG_R0,0x03002000);uc.reg_write(UC_ARM_REG_R1,0);uc.reg_write(UC_ARM_REG_R2,1)
ir_before=bytes(uc.mem_read(ir+0x8000,0x8000))
uc.emu_start(syms['translate_block_arm'],0x31000004,count=2000000)
assert ir_before==bytes(uc.mem_read(ir+0x8000,0x8000))
assert uc.reg_read(UC_ARM_REG_PC)==0x31000000,'Replay did not finish'
assert uc.reg_read(UC_ARM_REG_R0)==0xffffffff,'Expected capacity-triggered aborted translation'
tags=bytes(uc.mem_read(ir,0x8000))
left=sum(byte!=0 for byte in tags)
print(path,'aborted translation; remaining nonzero IWRAM guard bytes:',left)
expect_clean='--expect-clean' in sys.argv
if expect_clean:assert left==0, 'stale guards survived a full cache reset'
else:assert left>0, 'baseline failed to reproduce stale guard bug'

def call(name,args=()):
    uc.reg_write(UC_ARM_REG_SP,0x30008000);uc.reg_write(UC_ARM_REG_LR,0x31000000)
    for register,value in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2),args):uc.reg_write(register,value)
    uc.emu_start(syms[name],0x31000004,count=2000000)
    assert uc.reg_read(UC_ARM_REG_PC)==0x31000000
er=syms['ewram']
uc.mem_write(er,bytes(0x80000))
for page in range(8):uc.mem_write(er+page*0x10000+0x8000,bytes([0xa5])*0x8000)
uc.mem_write(er+0x10000+0x100,bytes([255])*0x100)
new_page=er+0x20000+0x8000
uc.mem_write(new_page+0x2000,pack(0xe1a00000)*200+pack(0xe12fff1e))
uc.mem_write(syms['memory_map_read']+(0x02010000>>15)*4,pack(new_page))
set32('iwram_code_min',0xffffffff);set32('iwram_code_max',0xffffffff)
set32('ewram_code_min',0x02008100);set32('ewram_code_max',0x02008200)
set32('ram_translation_ptr',syms['ram_translation_cache']+786432-32768-4)
if 'ram_guard_dirty_pages' in syms:set32('ram_guard_dirty_pages',1<<1)
data_before=[bytes(uc.mem_read(er+p*0x10000+0x8000,0x8000)) for p in range(8)]
call('translate_block_arm',(0x02012000,0,1))
assert uc.reg_read(UC_ARM_REG_R0)==0xffffffff
left=sum(byte!=0 for p in range(8) for byte in uc.mem_read(er+p*0x10000,0x8000))
print('EWRAM aborted translation; remaining guard bytes:',left)
assert (left==0) if expect_clean else (left>0)
assert data_before==[bytes(uc.mem_read(er+p*0x10000+0x8000,0x8000)) for p in range(8)]

# Existing cross-page cleanup also missed the first page tail and last page.
uc.mem_write(er,bytes(0x80000))
for p in range(8):uc.mem_write(er+p*0x10000+0x8000,bytes([0x5a])*0x8000)
uc.mem_write(er+0x10000+0x100,bytes([255])*0x7f00)
uc.mem_write(er+0x20000,bytes([255])*0x200)
set32('ewram_code_min',0x02008100);set32('ewram_code_max',0x02010200)
if 'ram_guard_dirty_pages' in syms:set32('ram_guard_dirty_pages',(1<<1)|(1<<2))
call('flush_translation_cache_ram')
left=sum(byte!=0 for p in range(8) for byte in uc.mem_read(er+p*0x10000,0x8000))
print('EWRAM multi-page reset; remaining guard bytes:',left)
assert (left==0) if expect_clean else (left>0)
for p in range(8):assert bytes(uc.mem_read(er+p*0x10000+0x8000,0x8000))==bytes([0x5a])*0x8000
if expect_clean:
    assert int.from_bytes(uc.mem_read(syms['ram_guard_dirty_pages'],4),'little')==0
    for name in ('iwram_code_min','iwram_code_max','ewram_code_min','ewram_code_max'):
        assert int.from_bytes(uc.mem_read(syms[name],4),'little')==0xffffffff
    def skip_cache(uc,pc,size,unused):
        uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE,skip_cache,begin=syms['warm_cache_op_range'],end=syms['warm_cache_op_range'])
    # A one-instruction block already has an entry tag when the scanner sees it.
    # Entry-tag creation must mark the page even if scanning writes no new tag.
    for address,page_data,page_tags,bit in ((0x03000700,ir+0x8000,ir,8),
                                          (0x02030700,er+6*0x10000+0x8000,er+6*0x10000,6)):
        uc.mem_write(syms['memory_map_read']+(address>>15)*4,pack(page_data))
        uc.mem_write(page_data+0x700,pack(0xe12fff1e))
        call('block_lookup_address_arm',(address,))
        assert int.from_bytes(uc.mem_read(syms['ram_guard_dirty_pages'],4),'little')&(1<<bit)
        assert bytes(uc.mem_read(page_tags+0x700,4))!=bytes(4)
        call('flush_translation_cache_ram')
        assert bytes(uc.mem_read(page_tags,0x8000))==bytes(0x8000)
        assert bytes(uc.mem_read(page_data+0x700,4))==pack(0xe12fff1e)
print('PASS guard-reset comparison; actual guest RAM bytes preserved')
