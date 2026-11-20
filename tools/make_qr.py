# QR code banano (shudhu Python er nijer jinish, kono library lage na). Byte mode, version 1-4.
# Robot er OLED er jonno PetBot\qr_github.h banay.
# Chalano:  python -I tools/make_qr.py "https://github.com/999mamun999" 3 M
#           python -I tools/make_qr.py --check scratch\user_qr.txt "https://github.com/999mamun999"
import sys, os

ECL_BITS = {'L': 1, 'M': 0, 'Q': 3, 'H': 2}
# version -> ecl -> (block count, data codewords per block, ecc codewords per block)
TABLE = {
    1: {'L': (1, 19, 7),  'M': (1, 16, 10), 'Q': (1, 13, 13), 'H': (1, 9, 17)},
    2: {'L': (1, 34, 10), 'M': (1, 28, 16), 'Q': (1, 22, 22), 'H': (1, 16, 28)},
    3: {'L': (1, 55, 15), 'M': (1, 44, 26), 'Q': (2, 17, 18), 'H': (2, 13, 22)},
    4: {'L': (1, 80, 20), 'M': (2, 32, 18), 'Q': (2, 24, 26), 'H': (4, 9, 16)},
}
ALIGN = {1: [], 2: [6, 18], 3: [6, 22], 4: [6, 26]}
REMAINDER_BITS = {1: 0, 2: 7, 3: 7, 4: 7}

def gf_mul(x, y):
    z = 0
    for i in reversed(range(8)):
        z = (z << 1) ^ ((z >> 7) * 0x11D)
        z ^= ((y >> i) & 1) * x
    return z

def rs_divisor(degree):
    result = [0] * (degree - 1) + [1]
    root = 1
    for _ in range(degree):
        for j in range(degree):
            result[j] = gf_mul(result[j], root)
            if j + 1 < len(result):
                result[j] ^= result[j + 1]
        root = gf_mul(root, 0x02)
    return result

def rs_remainder(data, divisor):
    result = [0] * len(divisor)
    for b in data:
        factor = b ^ result.pop(0)
        result.append(0)
        for i, coef in enumerate(divisor):
            result[i] ^= gf_mul(coef, factor)
    return result

def make_codewords(text, ver, ecl):
    nb, dlen, elen = TABLE[ver][ecl]
    cap = nb * dlen
    data = text.encode('utf-8')
    bits = [0, 1, 0, 0]                                   # byte mode
    bits += [(len(data) >> i) & 1 for i in reversed(range(8))]
    for byte in data:
        bits += [(byte >> i) & 1 for i in reversed(range(8))]
    if len(bits) > cap * 8:
        raise ValueError('lekha onek boro')
    bits += [0] * min(4, cap * 8 - len(bits))             # terminator
    bits += [0] * (-len(bits) % 8)
    cw = [int(''.join(map(str, bits[i:i + 8])), 2) for i in range(0, len(bits), 8)]
    pad = [0xEC, 0x11]
    k = 0
    while len(cw) < cap:
        cw.append(pad[k % 2]); k += 1
    div = rs_divisor(elen)
    blocks = [cw[i * dlen:(i + 1) * dlen] for i in range(nb)]
    eccs = [rs_remainder(b, div) for b in blocks]
    out = []
    for i in range(dlen):
        for b in blocks: out.append(b[i])
    for i in range(elen):
        for e in eccs: out.append(e[i])
    return out

MASKS = [
    lambda x, y: (x + y) % 2 == 0,
    lambda x, y: y % 2 == 0,
    lambda x, y: x % 3 == 0,
    lambda x, y: (x + y) % 3 == 0,
    lambda x, y: (x // 3 + y // 2) % 2 == 0,
    lambda x, y: x * y % 2 + x * y % 3 == 0,
    lambda x, y: (x * y % 2 + x * y % 3) % 2 == 0,
    lambda x, y: ((x + y) % 2 + x * y % 3) % 2 == 0,
]

def build(text, ver, ecl, mask=None):
    size = ver * 4 + 17
    m = [[False] * size for _ in range(size)]
    fn = [[False] * size for _ in range(size)]
    def setf(x, y, v):
        m[y][x] = v; fn[y][x] = True
    for i in range(size):
        setf(6, i, i % 2 == 0); setf(i, 6, i % 2 == 0)
    for (cx, cy) in [(3, 3), (size - 4, 3), (3, size - 4)]:
        for dy in range(-4, 5):
            for dx in range(-4, 5):
                x, y = cx + dx, cy + dy
                if 0 <= x < size and 0 <= y < size:
                    setf(x, y, max(abs(dx), abs(dy)) not in (2, 4))
    al = ALIGN[ver]
    n = len(al)
    for i in range(n):
        for j in range(n):
            if (i == 0 and j == 0) or (i == 0 and j == n - 1) or (i == n - 1 and j == 0):
                continue
            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    setf(al[i] + dx, al[j] + dy, max(abs(dx), abs(dy)) != 1)
    def draw_format(msk):
        data = ECL_BITS[ecl] << 3 | msk
        rem = data
        for _ in range(10):
            rem = (rem << 1) ^ ((rem >> 9) * 0x537)
        bits = (data << 10 | rem) ^ 0x5412
        gb = lambda i: (bits >> i) & 1 == 1
        for i in range(6): setf(8, i, gb(i))
        setf(8, 7, gb(6)); setf(8, 8, gb(7)); setf(7, 8, gb(8))
        for i in range(9, 15): setf(14 - i, 8, gb(i))
        for i in range(8): setf(size - 1 - i, 8, gb(i))
        for i in range(8, 15): setf(8, size - 15 + i, gb(i))
        setf(8, size - 8, True)
    draw_format(0)                                        # function jayga gulo chinhito korte
    cw = make_codewords(text, ver, ecl)
    i = 0
    right = size - 1
    while right >= 1:
        if right == 6: right = 5
        for vert in range(size):
            for j in range(2):
                x = right - j
                upward = ((right + 1) & 2) == 0
                y = (size - 1 - vert) if upward else vert
                if not fn[y][x] and i < len(cw) * 8:
                    m[y][x] = (cw[i >> 3] >> (7 - (i & 7))) & 1 == 1
                    i += 1
        right -= 2
    def apply(msk):
        out = [row[:] for row in m]
        for y in range(size):
            for x in range(size):
                if not fn[y][x] and MASKS[msk](x, y):
                    out[y][x] = not out[y][x]
        return out
    def penalty(g):
        p = 0
        for rows in (g, [list(c) for c in zip(*g)]):
            for row in rows:
                run = 1
                for k in range(1, size + 1):
                    if k < size and row[k] == row[k - 1]: run += 1
                    else:
                        if run >= 5: p += 3 + (run - 5)
                        run = 1
        for y in range(size - 1):
            for x in range(size - 1):
                c = g[y][x]
                if c == g[y][x + 1] == g[y + 1][x] == g[y + 1][x + 1]: p += 3
        dark = sum(sum(r) for r in g)
        p += abs(dark * 20 - size * size * 10) // (size * size) * 10
        return p
    if mask is None:
        best = None
        for k in range(8):
            draw_format(k)
            g = apply(k)
            pen = penalty(g)
            if best is None or pen < best[0]: best = (pen, k, g)
        mask = best[1]
    draw_format(mask)
    return apply(mask), mask

def to_text(g):
    return [''.join('1' if v else '0' for v in row) for row in g]

def read_format(rows):                                   # chhobi theke ECC ar mask khuje ber kora
    size = len(rows)
    for ecl in 'LMQH':
        for msk in range(8):
            data = ECL_BITS[ecl] << 3 | msk
            rem = data
            for _ in range(10): rem = (rem << 1) ^ ((rem >> 9) * 0x537)
            bits = (data << 10 | rem) ^ 0x5412
            ok = all(rows[8][size - 1 - i] == str((bits >> i) & 1) for i in range(8))   # 2nd copy (row 8, dane)
            if ok: return ecl, msk
    return None, None

if __name__ == '__main__':
    if sys.argv[1] == '--check':
        rows = [l.strip() for l in open(sys.argv[2]) if l.strip()]
        ver = (len(rows) - 17) // 4
        ecl, msk = read_format(rows)
        print('user QR: version', ver, 'ECC', ecl, 'mask', msk)
        for text in sys.argv[3:]:
            try:
                g, _ = build(text, ver, ecl, msk)
            except ValueError as e:
                print(repr(text), e); continue
            mine = to_text(g)
            diff = sum(a != b for r1, r2 in zip(mine, rows) for a, b in zip(r1, r2))
            print(repr(text), '-> mile na' if diff else '-> HUBHU MILECHE', '(alada ghor:', diff, ')')
        sys.exit(0)
    text, ver, ecl = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    g, msk = build(text, ver, ecl)
    rows = to_text(g)
    size = len(rows)
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'PetBot', 'qr_github.h')
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('// Auto-generated by tools/make_qr.py - hate edit koro na.\n')
        f.write('// QR: "%s" (version %d, ECC %s, mask %d)\n' % (text, ver, ecl, msk))
        f.write('#pragma once\n#define QR_SIZE %d\n' % size)
        f.write('// protiti line ekta row, bit 1 = kalo ghor; MSB = bam\n')
        f.write('static const uint32_t QR_ROWS[QR_SIZE] PROGMEM = {\n')
        for r in rows:
            f.write('  0x%08X,  // %s\n' % (int(r, 2), r))
        f.write('};\n')
    print('\n'.join(rows))
    print('mask', msk, '->', out)
