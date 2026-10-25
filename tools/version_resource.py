import re
import struct
import sys
from pathlib import Path


def version(path):
    source = Path(path).read_text()
    parts = []
    for name in ('MAJOR', 'MINOR', 'PATCH'):
        match = re.search(r'^#define NTPWR_VERSION_' + name + r'\s+(\d+)\s*$', source, re.M)
        if not match:
            raise SystemExit(f'Missing NTPWR_VERSION_{name}')
        parts.append(int(match.group(1)))
    if any(n > 65535 for n in parts):
        raise SystemExit('Version component exceeds 65535')
    return parts


def align(data):
    return data + b'\x00' * (-len(data) % 4)


def info(key, value=b'', children=(), value_type=1, value_length=0):
    data = struct.pack('<HHH', 0, value_length, value_type)
    data = align(data + key.encode('utf-16le') + b'\x00\x00')
    data = align(data + value) + b''.join(children)
    return struct.pack('<H', len(data)) + data[2:]


def string(key, text):
    value = (text + '\x00').encode('utf-16le')
    return info(key, value, value_length=len(value) // 2)


def resource_header(data_size, type_id):
    return struct.pack('<IIHHHHIHHII', data_size, 32, 0xFFFF, type_id,
                       0xFFFF, 0 if type_id == 0 else 1, 0, 0x0030, 0x0409, 0, 0)


def make_resource(parts, kind):
    major, minor, patch = parts
    file_version = f'{major}.{minor}.{patch}.0'
    product_version = f'{major}.{minor}.{patch}'
    exe = kind == 'exe'
    fixed = struct.pack('<13I', 0xFEEF04BD, 0x00010000,
                        major << 16 | minor, patch << 16,
                        major << 16 | minor, patch << 16,
                        0x3F, 0, 0x00040004, 1 if exe else 3,
                        0 if exe else 7, 0, 0)
    texts = {
        'FileDescription': 'NTPWR Management Utility' if exe else 'NTPWR Kernel Driver',
        'FileVersion': file_version,
        'LegalCopyright': 'Copyright (c) 2026 CoccodrillooXDS',
        'OriginalFilename': 'NTPWR.EXE' if exe else 'NTPWR.SYS',
        'ProductName': 'NTPWR',
        'ProductVersion': product_version,
    }
    table = info('040904B0', children=[string(k, v) for k, v in texts.items()])
    strings = info('StringFileInfo', children=[table])
    trans = info('Translation', struct.pack('<HH', 0x0409, 0x04B0),
                 value_type=0, value_length=4)
    vars_ = info('VarFileInfo', children=[trans])
    payload = info('VS_VERSION_INFO', fixed, children=[strings, vars_],
                   value_type=0, value_length=len(fixed))
    return resource_header(0, 0) + resource_header(len(payload), 16) + align(payload)


if __name__ == '__main__':
    if len(sys.argv) != 4 or sys.argv[3] not in ('exe', 'sys'):
        raise SystemExit('Usage: version_resource.py <version_header> <output.res> <exe|sys>')
    Path(sys.argv[2]).write_bytes(make_resource(version(sys.argv[1]), sys.argv[3]))
