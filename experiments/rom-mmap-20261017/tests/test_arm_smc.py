"""Execute real linked ARM instructions, not a rewritten store implementation.
Usage: python test_arm_smc.py [optimized ELF] [baseline ELF]
Requires unicorn and pyelftools on the test host only.
"""
from pathlib import Path
import random, struct, sys
base = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(base/'build/test-deps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile

def machine(path):
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.ctl_set_cpu_model(UC_CPU_ARM_926)
    with Path(path).open('rb') as f:
        elf = ELFFile(f)
        loads = [s for s in elf.iter_segments() if s['p_type']=='PT_LOAD']
        low = min(s['p_vaddr'] for s in loads) & ~4095
        high = (max(s['p_vaddr']+s['p_memsz'] for s in loads)+4095)&~4095
        uc.mem_map(low, high-low)
        for s in loads: uc.mem_write(s['p_vaddr'], s.data())
        symbols = {s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    uc.mem_map(0x20000000, 0x10000)  # tag area followed by actual mapped RAM
    uc.mem_map(0x30000000, 0x10000)  # dynarec register base and scratch stack
    uc.mem_map(0x31000000, 0x1000)   # fake emitted caller + return PC
    return uc, symbols

optimized = Path(sys.argv[1]) if len(sys.argv)>1 else base/'build/gpsp'
baseline = Path(sys.argv[2]) if len(sys.argv)>2 else None
regs = [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,
        UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,
        UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12]
rng = random.Random(14926)
cases = []
for width in (8,16,32):
    mask = (1<<width)-1
    for region in (0x02000000,0x03000000,0x03ff8000):
        for offset in (0, 0x104, 0x8000-width//8):
            for tag in (0,0x0101,0xffff):
                old = rng.getrandbits(width)
                for value in (old, old^1, old | (0xa5a50000 if width==16 else 0xa5ffff00 if width==8 else 0)):
                    cases.append((width,region+offset,tag,old,value,rng.getrandbits(4)<<28))

for path, is_opt in ([(baseline,False)] if baseline is not None else []) + [(optimized,True)]:
    uc, syms = machine(path)
    stopped = []
    stops = {syms['smc_write']:'smc', 0x31000104:'return', 0x31000100:'safe-return'}
    for w in (8,16,32): stops[syms[f'write_memory{w}']] = 'external'
    def stop(uc, address, size, unused):
        if address in stops:
            stopped.append(stops[address]); uc.emu_stop()
    uc.hook_add(UC_HOOK_CODE, stop)
    for width, address, tag, old, value, flags in cases:
        offset = address & 0x7fff
        mask = (1<<width)-1
        data = 0x20008000+offset
        uc.mem_write(0x20000000, bytes(0x10000))
        uc.mem_write(0x20000000+offset, struct.pack('<I',tag))
        uc.mem_write(data, struct.pack('<I',old)[:width//8])
        uc.mem_write(syms['memory_map_write']+(address>>15)*4, struct.pack('<I',0x20008000))
        uc.mem_write(0x31000100, struct.pack('<I',0x08001234))
        uc.reg_write(UC_ARM_REG_CPSR, flags|0x10)
        for i,r in enumerate(regs): uc.reg_write(r,0x12340000+i)
        uc.reg_write(UC_ARM_REG_R0,address); uc.reg_write(UC_ARM_REG_R1,value)
        uc.reg_write(UC_ARM_REG_SP,0x30001000); uc.reg_write(UC_ARM_REG_LR,0x31000100)
        stopped.clear()
        uc.emu_start(syms[f'execute_store_u{width}'],0x31000108,count=200)
        expected = 'smc' if tag and (not is_opt or old!=(value&mask)) else 'return'
        assert stopped==[expected], (path.name,width,hex(address),tag,old,value,stopped,expected)
        assert int.from_bytes(uc.mem_read(data,width//8),'little')==(value&mask)
        assert int.from_bytes(uc.mem_read(0x20000000+offset,4),'little')==tag
        assert uc.reg_read(UC_ARM_REG_SP)==0x30001000
        for i in list(range(3,11))+[12]: assert uc.reg_read(regs[i])==0x12340000+i
        assert uc.reg_read(UC_ARM_REG_R11)&0xf0000000==flags
        if expected=='return': assert uc.reg_read(UC_ARM_REG_CPSR)&0xf0000000==flags
        else: assert int.from_bytes(uc.mem_read(0x30001000+1084,4),'little')==0x08001234
        if is_opt and expected=='smc' and 'cp_smc_write_trace' in syms:
            trace=struct.unpack('<4I',uc.mem_read(syms['cp_smc_write_trace'],16))
            assert trace==(data,value,old,width), trace
    for width in (8,16,32):
        uc.reg_write(UC_ARM_REG_CPSR,0x10)
        uc.reg_write(UC_ARM_REG_R0,0x04000003)
        uc.reg_write(UC_ARM_REG_R1,0x12345678)
        uc.reg_write(UC_ARM_REG_SP,0x30001000); uc.reg_write(UC_ARM_REG_LR,0x31000100)
        stopped.clear()
        uc.emu_start(syms[f'execute_store_u{width}'],0x31000108,count=200)
        assert stopped==['external'], (width,stopped)
        assert uc.reg_read(UC_ARM_REG_R0)==(0x04000003 & ~(width//8-1))
        if width<32: assert uc.reg_read(UC_ARM_REG_R1)==(0x12345678&((1<<width)-1))
    for value in (0,0x12345678):
        uc.mem_write(0x20000000,struct.pack('<I',0xffff))
        uc.mem_write(syms['memory_map_write']+(0x03000000>>15)*4,struct.pack('<I',0x20008000))
        uc.reg_write(UC_ARM_REG_CPSR,0xa0000010)
        uc.reg_write(UC_ARM_REG_R0,0x03000000); uc.reg_write(UC_ARM_REG_R1,value)
        uc.reg_write(UC_ARM_REG_SP,0x30001000); uc.reg_write(UC_ARM_REG_LR,0x31000100)
        stopped.clear()
        uc.emu_start(syms['execute_store_u32_safe'],0x31000108,count=200)
        assert stopped==['safe-return']
        assert int.from_bytes(uc.mem_read(0x20008000,4),'little')==value
        assert uc.reg_read(UC_ARM_REG_CPSR)&0xf0000000==0xa0000000
    print(f'PASS actual ARM ELF {"optimized" if is_opt else "baseline"}: {len(cases)} RAM stores; widths, mirrors, tags, truncation, flags, preserved registers, stack, SMC PC and external routing')
