# Robot er screen theke QR pore asol lekha ber kore (encoder theke alada, nijossho decoder; library chara).
# Chalano:  python -I tools/qr_decode.py <shot.hex>      (/api/shot er 1024 byte hex)
# Version 1-4, byte mode. Reed-Solomon check o kore (kono bhul ghor thakle bolbe).
import sys

def gf_tables():
    exp, log = [0] * 512, [0] * 256
    x = 1
    for i in range(255):
        exp[i] = x; log[x] = i
        x <<= 1
        if x & 0x100: x ^= 0x11D
    for i in range(255, 512): exp[i] = exp[i - 255]
    return exp, log
EXP, LOG = gf_tables()
def gmul(a, b): return 0 if a == 0 or b == 0 else EXP[LOG[a] + LOG[b]]

BLOCKS = {  # version -> ecl -> (blocks, data per block, ecc per block)
    1: {'L': (1, 19, 7), 'M': (1, 16, 10), 'Q': (1, 13, 13), 'H': (1, 9, 17)},
    2: {'L': (1, 34, 10), 'M': (1, 28, 16), 'Q': (1, 22, 22), 'H': (1, 16, 28)},
    3: {'L': (1, 55, 15), 'M': (1, 44, 26), 'Q': (2, 17, 18), 'H': (2, 13, 22)},
    4: {'L': (1, 80, 20), 'M': (2, 32, 18), 'Q': (2, 24, 26), 'H': (4, 9, 16)},
}
ALIGN = {1: [], 2: [6, 18], 3: [6, 22], 4: [6, 26]}
ECL_OF = {1: 'L', 0: 'M', 3: 'Q', 2: 'H'}
MASKS = [lambda r, c: (r + c) % 2 == 0, lambda r, c: r % 2 == 0, lambda r, c: c % 3 == 0, lambda r, c: (r + c) % 3 == 0,
         lambda r, c: (r // 2 + c // 3) % 2 == 0, lambda r, c: (r * c) % 2 + (r * c) % 3 == 0,
         lambda r, c: ((r * c) % 2 + (r * c) % 3) % 2 == 0, lambda r, c: ((r + c) % 2 + (r * c) % 3) % 2 == 0]

def bch_format(data5):
    v = data5 << 10
    for i in range(14, 9, -1):
        if v & (1 << i): v ^= 0x537 << (i - 10)
    return ((data5 << 10) | v) ^ 0x5412

def decode(g):
    n = len(g); ver = (n - 17) // 4
    bits = [g[8][c] for c in (0, 1, 2, 3, 4, 5, 7, 8)] + [g[r][8] for r in (7, 5, 4, 3, 2, 1, 0)]
    word = 0
    for b in bits: word = (word << 1) | b
    best = min(range(32), key=lambda d: bin(bch_format(d) ^ word).count('1'))
    dist = bin(bch_format(best) ^ word).count('1')
    ecl, mask = ECL_OF[best >> 3], best & 7
    func = [[False] * n for _ in range(n)]
    def mark(r0, c0, h, w):
        for r in range(max(0, r0), min(n, r0 + h)):
            for c in range(max(0, c0), min(n, c0 + w)): func[r][c] = True
    mark(0, 0, 9, 9); mark(0, n - 8, 9, 8); mark(n - 8, 0, 8, 9)
    mark(6, 0, 1, n); mark(0, 6, n, 1)
    for ar in ALIGN[ver]:
        for ac in ALIGN[ver]:
            if not ((ar == 6 and ac == 6) or (ar == 6 and ac == n - 7) or (ar == n - 7 and ac == 6)):
                mark(ar - 2, ac - 2, 5, 5)
    func[n - 8][8] = True                                   # dark module
    stream = []
    c = n - 1; up = True
    while c > 0:
        if c == 6: c -= 1
        rows = range(n - 1, -1, -1) if up else range(n)
        for r in rows:
            for cc in (c, c - 1):
                if not func[r][cc]:
                    stream.append(g[r][cc] ^ (1 if MASKS[mask](r, cc) else 0))
        up = not up; c -= 2
    nb, dpb, epb = BLOCKS[ver][ecl]
    total = nb * (dpb + epb)
    cw = [int("".join(map(str, stream[i * 8:i * 8 + 8])), 2) for i in range(total)]
    blocks = [[] for _ in range(nb)]
    k = 0
    for i in range(dpb):
        for b in range(nb): blocks[b].append(cw[k]); k += 1
    for i in range(epb):
        for b in range(nb): blocks[b].append(cw[k]); k += 1
    ok = True
    for b in blocks:                                        # Reed-Solomon syndrome: sob 0 hole kono bhul nei
        for i in range(epb):
            s = 0
            for v in b: s = gmul(s, EXP[i]) ^ v
            if s: ok = False
    data = [v for b in blocks for v in b[:dpb]]
    bs = "".join(f"{v:08b}" for v in data)
    mode = int(bs[0:4], 2); cnt = int(bs[4:12], 2)
    text = bytes(int(bs[12 + i * 8:20 + i * 8], 2) for i in range(cnt)).decode("utf-8")
    return dict(version=ver, ecl=ecl, mask=mask, format_errors=dist, mode=mode, rs_ok=ok, text=text)

if __name__ == "__main__":
    hexs = open(sys.argv[1]).read().strip()
    buf = bytes.fromhex(hexs)
    px = lambda x, y: (buf[(y // 8) * 128 + x] >> (y % 8)) & 1
    n = 29                                                 # robot: (3 + 2c, 3 + 2r) te 2x2 ghor, kalo = dark
    g = [[1 - px(3 + 2 * c, 3 + 2 * r) for c in range(n)] for r in range(n)]
    for k, v in decode(g).items(): print(f"{k:14s}: {v}")
