"""Names the guest addresses in a crash log.

Reads logs\\simsbustinout.log (or the file given) and prints each line that holds a guest address
with what that address is: the XBE section, the recompiled function it lies in, the nearest name
from config\\game.toml [symbols]/[data], and whether the host replaces that function (the exe
exports a strong sub_XXXXXXXX for it, so the guest body never runs).

    python nimea-kaatumisosoitteet.py [log] [--exe out\\build\\release\\SimsBustinOut.exe]

Example: the Start-button crash of session 1 came out as
    [esp+030] 0028E083   -> DSOUND  sub_0028E062+0x21 = IDirectSoundBuffer_PlayEx+0x21  guest code
which is the whole diagnosis: the console's sound library ran on a host object (the host has
replaced it since, so today the same line ends in "host replaces it").
Standard library only.
"""
import argparse
import bisect
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))


def xbe_sections(path):
    data = open(path, 'rb').read()
    base, = struct.unpack_from('<I', data, 0x104)
    count, headers = struct.unpack_from('<II', data, 0x11C)
    out = []
    for i in range(count):
        flags, va, size, raw, raw_size, name_va = struct.unpack_from('<6I', data, headers - base + i * 0x38)
        end = data.index(b'\0', name_va - base)
        out.append((va, va + size, data[name_va - base:end].decode('ascii', 'replace')))
    return sorted(out)


def toml_names(path):
    """name -> address from [symbols] and [data]; zero addresses (absent in the image) are left out."""
    names, section = [], None
    for line in open(path, encoding='utf-8'):
        line = line.split('#', 1)[0].strip()
        if line.startswith('['):
            section = line
        elif section in ('[symbols]', '[data]') and '=' in line:
            name, value = (s.strip() for s in line.split('=', 1))
            try:
                va = int(value, 0)
            except ValueError:
                continue
            if va:
                names.append((va, name))
    return sorted(names)


def function_starts(path):
    starts = [int(m.group(1), 16) for m in re.finditer(r'\{0x([0-9A-Fa-f]+)u, sub_', open(path, encoding='utf-8').read())]
    return sorted(set(starts))


def pe_exports(path):
    """export names of a PE32+ file"""
    data = open(path, 'rb').read()
    pe, = struct.unpack_from('<I', data, 0x3C)
    sections_at = pe + 24 + struct.unpack_from('<H', data, pe + 20)[0]
    count, = struct.unpack_from('<H', data, pe + 6)
    sections = [struct.unpack_from('<IIII', data, sections_at + i * 40 + 8) for i in range(count)]

    def offset(rva):
        for vsize, va, raw_size, raw in sections:
            if va <= rva < va + max(vsize, raw_size):
                return rva - va + raw
        raise ValueError(hex(rva))

    export_rva, = struct.unpack_from('<I', data, pe + 24 + 112)  # PE32+ data directory 0
    if not export_rva:
        return set()
    e = offset(export_rva)
    n_names, = struct.unpack_from('<I', data, e + 24)
    names_rva, = struct.unpack_from('<I', data, e + 32)
    out = set()
    for i in range(n_names):
        rva, = struct.unpack_from('<I', data, offset(names_rva) + i * 4)
        o = offset(rva)
        out.add(data[o:data.index(b'\0', o)].decode('ascii', 'replace'))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    p.add_argument('log', nargs='?', default=os.path.join(ROOT, 'out', 'build', 'release', 'logs', 'simsbustinout.log'))
    p.add_argument('--exe', default=os.path.join(ROOT, 'out', 'build', 'release', 'SimsBustinOut.exe'))
    p.add_argument('--all', action='store_true', help='print every log line, not only the ones with addresses')
    args = p.parse_args()

    sections = xbe_sections(os.path.join(ROOT, 'assets', 'default.xbe'))
    names = toml_names(os.path.join(ROOT, 'config', 'game.toml'))
    starts = function_starts(os.path.join(ROOT, 'generated', 'game', 'x86_func_mapping.cpp'))
    replaced = {n for n in pe_exports(args.exe) if n.startswith('sub_')} if os.path.exists(args.exe) else None
    name_vas = [va for va, _ in names]

    def describe(va):
        sec = next((s for s in sections if s[0] <= va < s[1]), None)
        if not sec:
            return 'below the image: a null pointer plus an offset' if va < 0x10000 else 'outside the image'
        parts = [sec[2]]
        i = bisect.bisect_right(starts, va) - 1
        if i >= 0 and sec[0] <= starts[i]:
            fn = 'sub_%08X' % starts[i]
            parts.append(fn + ('+0x%X' % (va - starts[i]) if va != starts[i] else ''))
        # a name only when it starts inside the same function (or, for data, close by)
        floor = starts[i] if i >= 0 and sec[0] <= starts[i] else max(sec[0], va - 0x1000)
        j = bisect.bisect_right(name_vas, va) - 1
        if j >= 0 and names[j][0] >= floor:
            parts[-1] += ' = %s%s' % (names[j][1], '+0x%X' % (va - names[j][0]) if va != names[j][0] else '')
        if replaced is not None and i >= 0 and sec[0] <= starts[i]:
            parts.append('host replaces it' if 'sub_%08X' % starts[i] in replaced else 'guest code')
        return '  '.join(parts)

    patterns = [r'\[esp\+[0-9A-F]+\] ([0-9A-F]{8})', r'guest ([0-9A-F]{8})', r'trap at ([0-9A-F]{8})',
                r'call to ([0-9A-F]{8})']
    for line in open(args.log, encoding='utf-8', errors='replace'):
        line = line.rstrip('\n')
        found = [m for pat in patterns for m in re.finditer(pat, line)]
        if not found:
            if args.all:
                print(line)
            continue
        print(line + '   -> ' + '; '.join(describe(int(m.group(1), 16)) for m in found))


if __name__ == '__main__':
    sys.exit(main())
