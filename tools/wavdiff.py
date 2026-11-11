#!/usr/bin/env python3
"""tools/wavdiff.py - are two renders the same?

    python3 tools/wavdiff.py a.wav b.wav

Prints whether they are bit-identical and, if not, the largest sample difference and the difference's
level against a.wav's (in dB). Reads 16/24-bit PCM and 32-bit float WAVs (what check.py --render writes).
Exit 0 if bit-identical, 1 if not.
"""
import math, struct, sys

def read(path):
    d = open(path, 'rb').read()
    if d[:4] != b'RIFF' or d[8:12] != b'WAVE': sys.exit(f'{path}: not a WAV file')
    o, fmt, ch, bits, data = 12, None, 1, 16, None
    while o + 8 <= len(d):
        tag, n = d[o:o + 4], struct.unpack('<I', d[o + 4:o + 8])[0]
        if tag == b'fmt ':
            fmt, ch, _, _, _, bits = struct.unpack('<HHIIHH', d[o + 8:o + 24])
            if fmt == 0xFFFE: fmt = struct.unpack('<H', d[o + 32:o + 34])[0]
        if tag == b'data': data = d[o + 8:o + 8 + n]; break
        o += 8 + n + (n & 1)
    if data is None: sys.exit(f'{path}: no audio')
    if fmt == 3 and bits == 32: v = struct.unpack(f'<{len(data) // 4}f', data[:len(data) // 4 * 4])
    elif bits == 16: v = [x / 32768 for x in struct.unpack(f'<{len(data) // 2}h', data[:len(data) // 2 * 2])]
    elif bits == 24: v = [int.from_bytes(data[i:i + 3], 'little', signed=True) / 8388608 for i in range(0, len(data) - 2, 3)]
    else: sys.exit(f'{path}: {bits}-bit format {fmt} not read')
    return v, ch

def main():
    if len(sys.argv) != 3: sys.exit(__doc__)
    (a, ca), (b, cb) = read(sys.argv[1]), read(sys.argv[2])
    if ca != cb or len(a) != len(b): print(f'different shapes: {len(a) // ca} vs {len(b) // cb} frames, {ca} vs {cb} channels'); sys.exit(1)
    if a == b: print(f'bit-identical: {len(a) // ca} frames, {ca} channel(s)'); sys.exit(0)
    big = max(abs(x - y) for x, y in zip(a, b))
    da = math.sqrt(sum(x * x for x in a) / len(a)) + 1e-30
    dd = math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / len(a)) + 1e-30
    first = next(i for i, (x, y) in enumerate(zip(a, b)) if x != y) // ca
    print(f'different: largest sample difference {big:.3g}; the difference is {20 * math.log10(dd / da):.1f} dB against {sys.argv[1]}; first at frame {first}')
    sys.exit(1)

if __name__ == '__main__':
    main()
