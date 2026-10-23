import struct
import sys
from pathlib import Path

def check(path):
    data = Path(path).read_bytes()
    assert data[:2] == b'MZ', f'{path}: missing MZ'
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe + 4] == b'PE\0\0', f'{path}: invalid PE'
    machine, sections, _, _, _, opt_size, flags = struct.unpack_from('<HHIIIHH', data, pe + 4)
    assert machine == 0x14c and sections, f'{path}: not x86'
    opt = pe + 24
    assert struct.unpack_from('<H', data, opt)[0] == 0x10b, f'{path}: not PE32'
    os_major, os_minor = struct.unpack_from('<HH', data, opt + 40)
    sub = struct.unpack_from('<H', data, opt + 68)[0]
    assert (os_major, os_minor) == (3, 51), f'{path}: OS is not 3.51'
    expect = 1 if path.lower().endswith('.sys') else 2
    assert sub == expect, f'{path}: incorrect PE subsystem {sub}'
    image_base = struct.unpack_from('<I', data, opt + 28)[0]
    assert len(data) > 512
    print(f'PASS {Path(path).name}: PE32 i386, OS 3.51, subsystem={sub}, base=0x{image_base:X}, size={len(data)}')

for filename in sys.argv[1:]: check(filename)
