import struct
import sys
from pathlib import Path

from version_resource import version

VERSION = version(Path(__file__).resolve().parent.parent / 'include/ntpwr_version.h')

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
    expect = 1 if path.lower().endswith('.sys') else 3
    assert sub == expect, f'{path}: incorrect PE subsystem {sub}'
    image_base = struct.unpack_from('<I', data, opt + 28)[0]
    assert len(data) > 512
    resource_rva, resource_size = struct.unpack_from('<II', data, opt + 112)
    assert resource_rva and resource_size, f'{path}: missing version resource'
    section_table = opt + opt_size
    resource_data = None
    for n in range(sections):
        section = section_table + n * 40
        virtual_size, virtual_addr, raw_size, raw_offset = struct.unpack_from('<IIII', data, section + 8)
        if virtual_addr <= resource_rva < virtual_addr + max(virtual_size, raw_size):
            resource_data = data[raw_offset:raw_offset + raw_size]
            break
    assert resource_data, f'{path}: resource section missing'
    marker = struct.pack('<II', 0xFEEF04BD, 0x00010000)
    at = resource_data.find(marker)
    assert at >= 0, f'{path}: invalid VERSIONINFO'
    file_ms, file_ls, product_ms, product_ls = struct.unpack_from('<IIII', resource_data, at + 8)
    major, minor, patch = VERSION
    expected = ((major << 16) | minor, patch << 16)
    assert (file_ms, file_ls) == expected, f'{path}: FILEVERSION mismatch'
    assert (product_ms, product_ls) == expected, f'{path}: PRODUCTVERSION mismatch'
    assert f'{major}.{minor}.{patch}'.encode('utf-16le') in resource_data, f'{path}: missing version string'
    assert Path(path).name.upper().encode('utf-16le') in resource_data, f'{path}: wrong original filename'
    assert 'Copyright (c) 2026 CoccodrillooXDS'.encode('utf-16le') in resource_data, f'{path}: missing copyright'
    print(f'PASS {Path(path).name}: PE32 i386, OS 3.51, subsystem={sub}, NTPWR {major}.{minor}.{patch}, size={len(data)}')

for filename in sys.argv[1:]: check(filename)
