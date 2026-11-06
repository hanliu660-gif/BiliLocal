# dumpscan.py - WER minidump forensics without cdb (pitfall 55's tool).
# Parses a LocalDumps .dmp: exception thread's IP resolved to the nearest
# export of its module (AppHang dumps put the STUCK thread's IP there),
# then scans each thread's stack top for module-resolved return addresses.
# Usage: python dumpscan.py <path-to-.dmp>   (needs pip minidump + pefile)
import sys, struct
import pefile
from minidump.minidumpfile import MinidumpFile

path = sys.argv[1]
mdf = MinidumpFile.parse(path)

mods = []
for m in mdf.modules.modules:
    nm = m.name.replace('\\', '/').split('/')[-1]
    mods.append((m.baseaddress, m.baseaddress + m.size, nm))

def resolve(addr):
    for lo, hi, name in mods:
        if lo <= addr < hi:
            return "%s+0x%x" % (name, addr - lo)
    return None

segs = []
for seg in mdf.memory_segments_64.memory_segments:
    segs.append((seg.start_virtual_address, seg.size, seg.start_file_address))

def read_virt(addr, size):
    for va, sz, fo in segs:
        if va <= addr < va + sz:
            off = addr - va
            n = min(size, sz - off)
            with open(path, 'rb') as f:
                f.seek(fo + off)
                return f.read(n)
    return None

# nearest-export map per module of interest (built lazily)
exp_cache = {}
def nearest_export(mod_name, rva):
    if mod_name not in exp_cache:
        full = None
        for lo, hi, name in mods:
            if name == mod_name:
                for m in mdf.modules.modules:
                    n2 = m.name.replace('\\', '/').split('/')[-1]
                    if n2 == mod_name:
                        full = m.name
                        base = m.baseaddress
                        break
                break
        if not full:
            exp_cache[mod_name] = []
            return None
        try:
            pe = pefile.PE(full, fast_load=True)
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
            lst = []
            if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
                for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
                    if s.address and s.name:
                        lst.append((s.address, s.name.decode('ascii', 'replace')))
            lst.sort()
            exp_cache[mod_name] = lst
        except Exception as e:
            print("export parse fail %s: %s" % (mod_name, e))
            exp_cache[mod_name] = []
    lst = exp_cache[mod_name]
    lo_, hi_ = 0, len(lst) - 1
    best = None
    while lo_ <= hi_:
        mid = (lo_ + hi_) // 2
        if lst[mid][0] <= rva:
            best = lst[mid]
            lo_ = mid + 1
        else:
            hi_ = mid - 1
    return best[1] + "+0x%x" % (rva - best[0]) if best else None

exc_tid = None
try:
    if mdf.exception and mdf.exception.exception_records:
        er = mdf.exception.exception_records[0]
        exc_tid = er.ThreadId
        ea = er.ExceptionRecord.ExceptionAddress
        r = resolve(ea)
        detail = ""
        if r:
            mn, off = r.split('+0x')
            ne = nearest_export(mn, int(off, 16))
            detail = " (%s)" % ne if ne else ""
        print("EXCEPTION-STREAM tid=%s addr=%s%s" % (exc_tid, r, detail))
except Exception as e:
    print("exception-stream read failed:", e)

print("threads=%d" % len(mdf.threads.threads))
for t in mdf.threads.threads:
    stk = t.Stack
    if not stk:
        continue
    lo = getattr(stk, 'StartOfMemoryRange', 0)
    dsz = getattr(stk, 'DataSize', 0)
    if not dsz or dsz < 512:
        continue
    tag = " <== EXCEPTION-THREAD" if exc_tid == t.ThreadId else ""
    data = read_virt(lo, min(8192, dsz))
    if not data:
        continue
    line = []
    for i in range(0, len(data) - 7, 8):
        v = struct.unpack_from('<Q', data, i)[0]
        r = resolve(v)
        if r:
            mn, off = r.split('+0x')
            ne = nearest_export(mn, int(off, 16))
            line.append("+%d %s" % (i, ne if ne else r))
    if tag or line:
        print("tid=%s%s" % (t.ThreadId, tag))
        for h in line[:24]:
            print("    " + h)
