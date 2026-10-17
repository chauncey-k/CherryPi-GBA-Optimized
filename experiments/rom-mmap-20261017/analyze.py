"""Usage: python analyze.py /path/to/cherrypi-profile.txt [gpsp.nm]"""
from pathlib import Path
from collections import Counter
from bisect import bisect_right
import re
import sys

log = Path(sys.argv[1]).read_text(errors='replace')
base = Path(__file__).resolve().parent
symbol_path = Path(sys.argv[2]) if len(sys.argv) > 2 else (base/'gpsp.nm' if (base/'gpsp.nm').exists() else base/'cherrypi-profile/gpsp.nm')
if len(sys.argv) <= 2:
    import hashlib
    binary = symbol_path.with_name('gpsp')
    recorded = re.search(r'^BINARY_SHA256 ([0-9a-f]{64})$', log, re.M)
    if recorded and hashlib.sha256(binary.read_bytes()).hexdigest() != recorded[1]:
        raise SystemExit('Binary mismatch: use the symbols packaged with the sampled executable.')
symbols = []
for line in symbol_path.read_text().splitlines():
    match = re.match(r'^([0-9a-f]+)\s+(?:([0-9a-f]+)\s+)?([tTwW])\s+(.*)$', line)
    if match and not match[4].startswith('$'):
        symbols.append((int(match[1], 16), int(match[2] or '0', 16), match[4]))
symbols.sort()
starts = [s[0] for s in symbols]
maps = []
for line in log.splitlines():
    m = re.match(r'^([0-9a-f]+)-([0-9a-f]+)\s+(\S+)\s+\S+\s+\S+\s+\d+\s*(.*)$', line)
    if m:
        maps.append((int(m[1], 16), int(m[2], 16), m[3], m[4]))
total_match = re.search(r'^TOTAL (\d+)$', log, re.M)
if not total_match or int(total_match[1]) == 0:
    raise SystemExit('No samples: use CP_PROFILE=1 and exit the game normally.')
total = int(total_match[1])
counts = Counter()
for key in ('JIT_ROM', 'JIT_RAM', 'JIT_BIOS', 'LOST'):
    counts[key] = int(re.search(r'^'+key+r' (\d+)$', log, re.M)[1])
for pc, hits in re.findall(r'^PC ([0-9a-f]+) (\d+)$', log, re.M):
    pc = int(pc, 16)
    mapping = next((m for m in maps if m[0] <= pc < m[1]), None)
    label = mapping[3] or '[anonymous]' if mapping else '[unmapped]'
    if mapping and mapping[3].endswith('/gpsp'):
        i = bisect_right(starts, pc) - 1
        if i >= 0:
            start, size, name = symbols[i]
            end = start + size if size else (symbols[i+1][0] if i+1 < len(symbols) else start+4)
            label = name if pc < end else '[gpsp: no symbol]'
    counts[label] += int(hits)
print(f'{total} samples; MAIN-THREAD WALL TIME (includes waits/menus, not CPU utilization).')
print('JIT entries identify generated ARM code, not individual guest functions.')
for name, count in counts.most_common(25):
    if count:
        print(f'{100*count/total:6.2f}% {count:6d}  {name}')
if sum(counts.values()) != total:
    raise SystemExit('Sample total mismatch: incomplete or malformed report.')
regions = ('RAM', 'ROM', 'BIOS')
retained=re.search(r'^LIVE_SMC_RETAINED (\d+)$',log,re.M)
if retained:
    print('Verified live-template STM writes retained without cache flush:',retained[1])
for index, count in re.findall(r'^TRANSLATIONS (\d+) (\d+)$', log, re.M):
    index = int(index)
    print(f'Translation calls {regions[index//2]} {"Thumb" if index%2 else "ARM"}: {count}')
for index, count in re.findall(r'^FLUSHES (\d+) (\d+)$', log, re.M):
    print(f'Cache flushes {regions[int(index)]}: {count}')
guests = [(int(count), pc, int(thumb)) for pc, thumb, count in re.findall(r'^GUEST ([0-9a-f]+) ([01]) (\d+)$', log, re.M)]
if guests:
    print('Most repeatedly translated guest blocks (calls, GBA address, instruction set):')
    for count, pc, thumb in sorted(guests, reverse=True)[:12]:
        print(f'  {count:8d}  0x{pc}  {"Thumb" if thumb else "ARM"}')
traces = [list(map(int, line.split())) for line in re.findall(r'^TRACE (.+)$', log, re.M)]
if traces:
    print('Per 100 delivered samples (~1s, not exact): window frames translation_calls RAM_flush ROM_flush BIOS_flush')
    previous = [0] * 11
    for row in traces:
        delta = [a-b for a,b in zip(row, previous)]
        print(f'  {row[0]:3d}  {delta[1]:4d}  {sum(delta[2:8]):8d}  {delta[8]:6d} {delta[9]:6d} {delta[10]:6d}')
        previous = row
origin_rows=re.findall(r'^ORIGIN ([0-9a-f]+) ([0-9a-f]+) ([0-9a-f]+) (\d+) ([0-9a-f]+) ([0-9a-f]+) (\d+) (\d+)$',log,re.M)
if origin_rows:
    print('RAM flush origins (guest PC is the saved resume PC, not necessarily the store instruction):')
    for caller,pc,address,width,old,new,used,count in sorted(origin_rows,key=lambda r:int(r[-1]),reverse=True):
        caller=int(caller,16)
        i=bisect_right(starts,caller-4)-1
        name=symbols[i][2] if i>=0 else 'unknown'
        extra=f'CPU store{width} to 0x{address}: 0x{old} -> 0x{new}' if int(width) else 'no direct-store record (e.g. DMA/capacity/reset)'
        print(f'  {count:>8}  caller={name}+0x{caller-symbols[i][0]:x} guest=0x{pc} max_cache_used={used}; {extra}')
