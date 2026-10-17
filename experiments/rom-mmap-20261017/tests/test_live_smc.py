"""Run real translated code and compare it with ARM926 executing guest opcodes.
Includes runtime changes without recompilation and whole-STM retention checks.
"""
from pathlib import Path
import sys, struct, random
base=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(base/'local/test-deps'))
# Reuse ELF loader and libc stubs from the guard regression, before its tests.
setup=(base/'tests/test_guard_flush.py').read_text().split("ir=syms['iwram']")[0]
sys.argv=[str(base/'tests/test_guard_flush.py'),str(base/'build/gpsp')]
exec(compile(setup,str(base/'tests/test_guard_flush.py'),'exec'))
from unicorn import UcError
ir=syms['iwram']; cache=syms['ram_translation_cache']
allocation={0:3,1:4,6:5,9:6,12:7,14:8}
regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,
      UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,
      UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR]
def get32(at):return int.from_bytes(uc.mem_read(at,4),'little')
def call(name,args=()):
    uc.reg_write(UC_ARM_REG_CPSR,0x10)
    uc.reg_write(UC_ARM_REG_SP,0x30008000);uc.reg_write(UC_ARM_REG_LR,0x31000000)
    for r,v in zip(regs,args):uc.reg_write(r,v)
    uc.emu_start(syms[name],0x31000004,count=2000000)
    assert uc.reg_read(UC_ARM_REG_PC)==0x31000000,(name,hex(uc.reg_read(UC_ARM_REG_PC)))
    return uc.reg_read(UC_ARM_REG_R0)
def skip_cache(uc,pc,size,unused):
    uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
uc.hook_add(UC_HOOK_CODE,skip_cache,begin=syms['warm_cache_op_range'],end=syms['warm_cache_op_range'])
uc.mem_write(syms['memory_map_read']+(0x03000000>>15)*4,pack(ir+0x8000))
uc.mem_write(syms['memory_map_write']+(0x03000000>>15)*4,pack(ir+0x8000))
uc.mem_write(syms['waitstate_cycles_sequential'],bytes([1])*0x100)
columns=[[0xe1a09b49,0xe0969bc9],[0xe0999086,0xe08c6006,0xe0877004],
 [0xe0977004,0xe0833ba7],[0x508c6006,0x51f360d1,0xe1d360d0],
 [0x51f3c0d1,0xe1f3c0d1],[0x505cc006,0xe05cc006]]
positions=[(0x030020e0+half+sample*0x24+off,col)
 for half in (0,0x98) for sample in range(4)
 for col,off in enumerate((0,4,0x0c,0x10,0x14,0x18))]
native_stop=[]
def stop_native(uc,pc,size,unused):native_stop.append(pc);uc.emu_stop()
for name in ('arm_indirect_branch_dual_arm','arm_indirect_branch_arm'):
    uc.hook_add(UC_HOOK_CODE,stop_native,begin=syms[name],end=syms[name])
reference=Uc(UC_ARCH_ARM,UC_MODE_ARM);reference.ctl_set_cpu_model(UC_CPU_ARM_926)
reference.mem_map(0x03000000,0x8000)
rng=random.Random(92617); count=0
for address,col in positions:
    call('flush_translation_cache_ram')
    offset=address&0x7fff
    uc.mem_write(ir+0x8000+offset,pack(columns[col][0])+pack(0xe12fff1e))
    result=call('translate_block_arm',(address,0,1))
    uc.ctl_remove_cache(cache,cache+786432)
    assert result!=0xffffffff
    assert uc.mem_read(syms['cp_live_ready']+offset//4,1)==b'\1'
    # The SAME emitted block must execute every later opcode variant.
    for word in columns[col]:
      for flags in range(16):
        values=[rng.getrandbits(32) for _ in range(15)]
        values[3]=0x03001000 # signed byte sample address
        sample=bytes([0x81,0x7f,0x00,0xff])
        reference.mem_write(0x03001000,sample);uc.mem_write(ir+0x9000,sample)
        reference.mem_write(address,pack(word))
        reference.ctl_remove_cache(address,address+4)
        reference.reg_write(UC_ARM_REG_CPSR,(flags<<28)|0x10)
        for r,v in zip(regs,values):reference.reg_write(r,v)
        reference.emu_start(address,address+4,count=1)
        expected=[reference.reg_read(r) for r in regs]
        expected_flags=reference.reg_read(UC_ARM_REG_CPSR)&0xf0000000
        uc.mem_write(ir+0x8000+offset,pack(word))
        uc.reg_write(UC_ARM_REG_CPSR,(flags<<28)|0x10)
        regbase=0x30001000
        for i,v in enumerate(values):uc.mem_write(regbase+1024+i*4,pack(v))
        for guest,host in allocation.items():uc.reg_write(regs[host],values[guest])
        uc.reg_write(UC_ARM_REG_SP,regbase);uc.reg_write(UC_ARM_REG_R12,0xffff0000)
        native_stop.clear()
        uc.emu_start(cache,0x31000004,count=1000)
        assert native_stop,(hex(address),hex(word),'did not return')
        actual=[uc.reg_read(regs[allocation[i]]) if i in allocation else get32(regbase+1024+i*4) for i in range(15)]
        assert actual==expected,(hex(address),hex(word),flags,actual,expected)
        assert uc.reg_read(UC_ARM_REG_CPSR)&0xf0000000==expected_flags,(hex(address),hex(word),flags,'NZCV')
        assert uc.reg_read(UC_ARM_REG_SP)==regbase
        count+=1
print('PASS live emitted ARM vs ARM926:',count,'cases; 48 sites, every variant, all NZCV inputs, signed loads and writeback')

# Unknown live words take the ordinary flush/retranslate path, even when a
# preceding safe STM store changed them without a final guarded-word change.
uc.mem_write(ir+0x8000+offset,pack(0xe1a00000))
def unknown_stop(uc,pc,size,unused):uc.emu_stop()
handle=uc.hook_add(UC_HOOK_CODE,unknown_stop,begin=syms['smc_write'],end=syms['smc_write'])
uc.reg_write(UC_ARM_REG_CPSR,0xb0000010);uc.reg_write(UC_ARM_REG_SP,0x30001000)
uc.emu_start(cache,0x31000004,count=1000)
assert uc.reg_read(UC_ARM_REG_PC)==syms['smc_write']
assert get32(0x30001000+1084)==address
assert uc.reg_read(UC_ARM_REG_R11)&0xf0000000==0xb0000000
uc.hook_del(handle)
print('PASS unknown live opcode retries through normal SMC invalidation with original PC/NZCV')

# Replay the actual eight-sample mixer body from the user's checkpoint.
# Stop before the outer loop branch; compare sample buffers as well as regs.
import os
fixture=Path(os.environ.get('CP_IWRAM_FIXTURE',str(base/'local/iwram.bin')))
if fixture.is_file():
    image=fixture.read_bytes()
    call('flush_translation_cache_ram')
    uc.mem_write(ir+0x8000,image)
    uc.mem_write(ir+0x8000+0x2208,pack(0xe12fff1e))
    call('translate_block_arm',(0x030020d8,0,1))
    uc.ctl_remove_cache(cache,cache+786432)
    templates=[struct.unpack('<6I',image[at:at+24]) for at in (0x2220,0x2238,0x2250)]
    boundaries={get32(syms['block_data']+i*8):0x030020d8+i*4 for i in range((0x2208-0x20d8)//4+1)}
    for words in templates:
     for flags in range(16):
        data=bytearray(image)
        data[0x800:0x1000]=bytes(rng.randrange(256) for _ in range(0x800))
        data[0x1000:0x1800]=bytes(rng.randrange(256) for _ in range(0x800))
        for address,col in positions:data[address&0x7fff:(address&0x7fff)+4]=pack(words[col])
        data[0x2208:0x220c]=pack(0xe12fff1e)
        values=[rng.getrandbits(32) for _ in range(15)]
        values[3]=0x03000800;values[4]=0x00800000;values[5]=0x03001000;values[7]=0;values[6]=0xffffff81
        uc.mem_write(ir+0x8000,bytes(data));reference.mem_write(0x03000000,bytes(data))
        reference.ctl_remove_cache(0x030020d8,0x0300220c)
        reference.reg_write(UC_ARM_REG_CPSR,(flags<<28)|0x10)
        for r,v in zip(regs,values):reference.reg_write(r,v)
        guest_trace={};native_trace={}
        def trace_guest(u,p,s,d):guest_trace[p]=([u.reg_read(r) for r in regs],u.reg_read(UC_ARM_REG_CPSR)&0xf0000000)
        gh=reference.hook_add(UC_HOOK_CODE,trace_guest)
        reference.emu_start(0x030020d8,0x03002208,count=1000)
        reference.hook_del(gh)
        expected=[reference.reg_read(r) for r in regs]
        expected_flags=reference.reg_read(UC_ARM_REG_CPSR)&0xf0000000
        uc.reg_write(UC_ARM_REG_CPSR,(flags<<28)|0x10)
        regbase=0x30001000
        for i,v in enumerate(values):uc.mem_write(regbase+1024+i*4,pack(v))
        for guest,host in allocation.items():uc.reg_write(regs[host],values[guest])
        uc.reg_write(UC_ARM_REG_SP,regbase);uc.reg_write(UC_ARM_REG_R12,0xffff0000)
        def trace_native(u,p,s,d):
          if p in boundaries:native_trace[boundaries[p]]=([u.reg_read(regs[allocation[i]]) if i in allocation else get32(regbase+1024+i*4) for i in range(15)],u.reg_read(UC_ARM_REG_CPSR)&0xf0000000)
        nh=uc.hook_add(UC_HOOK_CODE,trace_native)
        native_stop.clear();uc.emu_start(cache,0x31000004,count=100000)
        uc.hook_del(nh)
        assert native_stop,'full mixer did not return'
        actual=[uc.reg_read(regs[allocation[i]]) if i in allocation else get32(regbase+1024+i*4) for i in range(15)]
        if actual!=expected:
          for at in sorted(guest_trace):
            if at in native_trace and guest_trace[at]!=native_trace[at]:
              print('FIRST DIFFERENT BOUNDARY',hex(at),guest_trace[at],native_trace[at]);break
        assert actual==expected,(words,flags,actual,expected)
        assert uc.reg_read(UC_ARM_REG_CPSR)&0xf0000000==expected_flags
        assert bytes(uc.mem_read(ir+0x9000,0x800))==bytes(reference.mem_read(0x03001000,0x800))
    print('PASS actual eight-sample mixer body: all 3 templates x 16 NZCV states, registers and output samples match ARM926')
    
else:
    print('SKIP full user mixer fixture: set CP_IWRAM_FIXTURE to a locally extracted 32KiB IWRAM file')

# Retention must validate every safe store preceding the guarded last STM word.
call('flush_translation_cache_ram')
for address,col in positions:
    offset=address&0x7fff
    uc.mem_write(ir+0x8000+offset,pack(columns[col][0]))
    uc.mem_write(syms['cp_live_ready']+offset//4,b'\1')
for half in (0,0x98):
 for sample in range(4):
  for off,n,resume in ((0,2,0x030020a0 if half==0 else 0x030020a8),(0x0c,4,0x030020b0 if half==0 else 0x030020b8)):
    first=0x20e0+half+sample*0x24+off;host=ir+0x8000+first+(n-1)*4
    assert call('cp_live_store',(host,resume))==1
    for i in range(n):
      at=first+i*4;old=get32(ir+0x8000+at)
      uc.mem_write(ir+0x8000+at,pack(0xe1a00000))
      assert call('cp_live_store',(host,resume))==0,'unknown earlier safe store accepted'
      uc.mem_write(ir+0x8000+at,pack(old))
      uc.mem_write(syms['cp_live_ready']+at//4,b'\0')
      assert call('cp_live_store',(host,resume))==0,'static translation accepted'
      uc.mem_write(syms['cp_live_ready']+at//4,b'\1')
      uc.mem_write(syms['cp_live_blocked']+at//4,b'\1')
      assert call('cp_live_store',(host,resume))==0,'Thumb overlap accepted'
      uc.mem_write(syms['cp_live_blocked']+at//4,b'\0')
    assert call('cp_live_store',(host,0x08000004))==0
print('PASS all 16 STM groups; unknown opcode, uncompiled/static word, Thumb overlap and other writer rejected')

# Exercise the real assembly store ABI and its C retention call.
first=0x20e0;target=first+4;host=ir+0x8000+target
uc.mem_write(ir+0x8000+first,pack(columns[0][0]))
uc.mem_write(ir+0x8000+target,pack(columns[1][0]))
uc.mem_write(ir+target,pack(0xffffffff))
uc.mem_write(0x31000080,pack(0x030020a0))
stops=[]
def store_stop(uc,pc,size,unused):stops.append(pc);uc.emu_stop()
for at in (0x31000084,syms['smc_write']):uc.hook_add(UC_HOOK_CODE,store_stop,begin=at,end=at)
for flags in range(16):
 uc.mem_write(host,pack(columns[1][0]))
 uc.reg_write(UC_ARM_REG_CPSR,(flags<<28)|0x10)
 for i,r in enumerate(regs[:13]):uc.reg_write(r,0x12340000+i)
 uc.reg_write(UC_ARM_REG_R0,0x03000000+target);uc.reg_write(UC_ARM_REG_R1,columns[1][2])
 uc.reg_write(UC_ARM_REG_SP,0x30001000);uc.reg_write(UC_ARM_REG_LR,0x31000080)
 stops.clear();uc.emu_start(syms['execute_store_u32'],0x31000088,count=1000)
 assert stops==[0x31000084],stops
 assert get32(host)==columns[1][2]
 assert uc.reg_read(UC_ARM_REG_SP)==0x30001000
 assert uc.reg_read(UC_ARM_REG_CPSR)&0xf0000000==flags<<28
 for i in list(range(3,11))+[12]:assert uc.reg_read(regs[i])==0x12340000+i
call('flush_translation_cache_ram')
assert bytes(uc.mem_read(syms['cp_live_ready'],0x2000))==bytes(0x2000)
assert bytes(uc.mem_read(syms['cp_live_blocked'],0x2000))==bytes(0x2000)
print('PASS retained store assembly ABI, registers, stack and NZCV; cache flush resets live eligibility')
