from pathlib import Path
import subprocess, tempfile
base = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp/'symbols').write_text('00010000 00000100 T update_scanline\n00010100 T arm_update\n00010200 T next\n')
    (tmp/'report').write_text('''CP_PROFILE_V1
TOTAL 10
LOST 1
JIT_ROM 3
JIT_RAM 1
JIT_BIOS 0
PC 10004 2
PC 10104 1
PC 20004 2
TRANSLATIONS 0 2
TRANSLATIONS 1 1
FLUSHES 0 1
GUEST 03000100 0 2
TRACE 1 10 2 1 0 0 0 0 1 0 0
ORIGIN 00010004 030024fc 03005ce0 16 00000001 00000002 5000 7
MAPS
00010000-00020000 r-xp 00000000 00:00 0 /root/gpsp/gpsp
00020000-00030000 r-xp 00000000 00:00 0 /root/gpsp/lib/libSDL.so
''')
    p = subprocess.run(['python3', str(base/'analyze.py'), str(tmp/'report'), str(tmp/'symbols')], capture_output=True, text=True)
    assert p.returncode == 0, p.stderr
    for text in ('30.00%', 'update_scanline', 'arm_update', 'libSDL.so', 'LOST',
                 'Translation calls RAM ARM: 2', 'Cache flushes RAM: 1', '0x03000100',
                 'Per 100 delivered samples', 'CPU store16 to 0x03005ce0', 'guest=0x030024fc'):
        assert text in p.stdout, p.stdout
    (tmp/'report').write_text('TOTAL 0\n')
    assert subprocess.run(['python3', str(base/'analyze.py'), str(tmp/'report'), str(tmp/'symbols')], capture_output=True).returncode != 0
print('PASS profiler analysis: C/assembly symbols, JIT, shared libraries, lost samples and empty report')
