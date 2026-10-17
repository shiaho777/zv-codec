#!/usr/bin/env python3
"""Synthetic round-trip tests for the v3/v3s codec paths.

Covers the shapes that historically broke encoder/decoder symmetry:
static, scroll, noise, non-16-aligned dims, small dims, stateful streams.
"""
import ctypes, os, sys
import numpy as np

LIB = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "libzv_core.dylib"))

for fn in ("zv_encode_frame_v3", "zv_encode_frame_v3s",
           "zv_decode_frame_v3", "zv_decode_frame_v3s"):
    pass
LIB.zv_encode_frame_v3.argtypes = [ctypes.c_void_p]*2 + [ctypes.c_int]*2 + \
    [ctypes.c_void_p, ctypes.c_int32]
LIB.zv_encode_frame_v3.restype = ctypes.c_int32
LIB.zv_decode_frame_v3.argtypes = LIB.zv_encode_frame_v3.argtypes[:]
LIB.zv_decode_frame_v3.restype = ctypes.c_int32
LIB.zv_encode_frame_v3s.argtypes = LIB.zv_encode_frame_v3.argtypes[:] + \
    [ctypes.c_void_p]
LIB.zv_encode_frame_v3s.restype = ctypes.c_int32
LIB.zv_decode_frame_v3s.argtypes = LIB.zv_encode_frame_v3s.argtypes[:]
LIB.zv_decode_frame_v3s.restype = ctypes.c_int32
LIB.zv_encode_frame_v3sq.argtypes = [ctypes.c_void_p]*2 + [ctypes.c_int]*2 + \
    [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int, ctypes.c_int] + \
    [ctypes.c_void_p]*2
LIB.zv_encode_frame_v3sq.restype = ctypes.c_int32
LIB.zv3_state_size.argtypes = [ctypes.c_int, ctypes.c_int]
LIB.zv3_state_size.restype = ctypes.c_int32
LIB.zv3_state_init.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
LIB.zv3_state_init.restype = None


def ycocg_of(rgb):  # quick RGB->YCoCg-R-ish int16 triple interleave
    r = rgb[..., 0].astype(np.int32); g = rgb[..., 1]; b = rgb[..., 2]
    y = (r + 2 * g + b) // 4
    co = r - b
    cg = g - (r + b) // 2
    out = np.stack([y, co & 0x7FFF, cg & 0x7FFF], -1)
    return np.clip(out, 0, 4095).astype(np.int16)


def gen_frames(kind, w, h, n, seed=0):
    rng = np.random.default_rng(seed)
    frames = []
    if kind == "static":
        base = rng.integers(0, 256, (h, w, 3), dtype=np.int32)
        frames = [base.copy() for _ in range(n)]
    elif kind == "scroll":
        tex = rng.integers(0, 256, (h, w + 64, 3), dtype=np.int32)
        for i in range(n):
            frames.append(tex[:, i * 2:i * 2 + w].copy())
    elif kind == "noise":
        for _ in range(n):
            frames.append(rng.integers(0, 256, (h, w, 3), dtype=np.int32))
    elif kind == "text":
        # white text-ish blocks on dark bg + scrolling
        for i in range(n):
            f = np.zeros((h, w, 3), dtype=np.int32)
            for y in range(0, h, 8):
                yy = (y + i * 3) % h
                rows = min(4, h - yy)
                f[yy:yy + rows, :] = rng.integers(180, 256, (rows, w, 3))
            frames.append(f)
    elif kind == "cfl":
        # chroma = affine(luma) per leaf - two-tone UI/text content;
        # later frames shift so PSAME/VRPT replay of CFL leaves fires
        base = rng.integers(0, 4000, (h, w), dtype=np.int32)
        for i in range(n):
            y = np.roll(base, i, axis=1) if i else base
            co = (y // 2 + 40).clip(0, 4095)
            cg = (4000 - y // 3).clip(0, 4095)
            frames.append(np.stack([y, co, cg], -1))
    elif kind == "cflout":
        # affine chroma with heavy outliers -> CFL residuals past the
        # hi-escape boundary (u >= 318)
        for i in range(n):
            y = rng.integers(0, 4000, (h, w), dtype=np.int32)
            co = (y // 2 + rng.integers(-600, 600, (h, w))).clip(0, 4095)
            cg = (y // 3 + rng.integers(-2000, 2000, (h, w))).clip(0, 4095)
            frames.append(np.stack([y, co, cg], -1))
    elif kind == "c420":
        # chroma planes exactly 2x2 block-constant (native 420 content)
        for i in range(n):
            y = rng.integers(0, 4000, (h, w), dtype=np.int32)
            u = rng.integers(0, 4095, (h // 2, w // 2), dtype=np.int32)
            v = rng.integers(0, 4095, (h // 2, w // 2), dtype=np.int32)
            co = np.repeat(np.repeat(u, 2, 0), 2, 1)
            cg = np.repeat(np.repeat(v, 2, 0), 2, 1)
            frames.append(np.stack([y, co, cg], -1))
    elif kind == "c420mix":
        # 420-constant chroma that later turns full-res - exercises the
        # PHDR_C420 flag flip and the stateful ptab res-transition
        for i in range(n):
            y = rng.integers(0, 4000, (h, w), dtype=np.int32)
            if i % 2 == 0:
                u = rng.integers(0, 4095, (h // 2, w // 2), dtype=np.int32)
                co = np.repeat(np.repeat(u, 2, 0), 2, 1)
                cg = np.repeat(np.repeat(u[::-1], 2, 0), 2, 1)
            else:
                co = rng.integers(0, 4095, (h, w), dtype=np.int32)
                cg = rng.integers(0, 4095, (h, w), dtype=np.int32)
            frames.append(np.stack([y, co, cg], -1))
    elif kind == "band":
        # tile row 0: checkerboard noise/flat quadrants -> SPLIT leaves.
        # tile row 1: static content -> RUN early-exit at top level.
        # tile row 2: fresh noise every frame -> non-repeat tiles that
        #   read the per-column "above tile split" bit.  If the RUN
        #   early-exit emit forgets to clear that bit, tile-row-2 op
        #   records decode in the wrong context and the stream desyncs.
        for i in range(n):
            f = np.full((h, w, 3), 64, dtype=np.int32)
            n1 = rng.integers(0, 256, (16, w, 3), dtype=np.int32)
            xx, yy = np.indices((16, w))
            mask = ((xx // 8) + (yy // 8)) % 2 == 0      # TL+BR noise
            f[:16] = np.where(mask[..., None], n1, 64)
            if h >= 48:
                f[32:48] = rng.integers(0, 256, (16, w, 3),
                                        dtype=np.int32)
            frames.append(f)
    return frames


def run_stateless(frames, w, h):
    enc = dec = 0
    prev_e = prev_d = None
    for f in frames:
        cur = np.ascontiguousarray(ycocg_of(f))
        out = np.zeros(w * h * 9 + 65536, np.uint8)
        n = LIB.zv_encode_frame_v3(
            prev_e.ctypes.data if prev_e is not None else None,
            cur.ctypes.data, w, h, out.ctypes.data, out.size)
        assert n > 0, "encode failed"
        enc += n
        got = np.zeros_like(cur)
        r = LIB.zv_decode_frame_v3(
            prev_d.ctypes.data if prev_d is not None else None,
            got.ctypes.data, w, h, out.ctypes.data, n)
        assert r == n, f"decode ret {r} != {n}"
        dec += n
        # recompute what the reference pixel path should be:
        # lossless path means 'got' must equal 'cur' exactly
        if not np.array_equal(got, cur):
            d = np.abs(got.astype(np.int32) - cur.astype(np.int32))
            idx = np.argwhere(d.sum(-1) > 0)[0]
            raise AssertionError(f"pixel mismatch at {idx}, "
                                 f"got {got[tuple(idx)]} want {cur[tuple(idx)]}")
        prev_e = cur
        prev_d = got.copy()
    return enc


def run_stateful(frames, w, h):
    ssz = LIB.zv3_state_size(w, h)
    es = ctypes.create_string_buffer(ssz); LIB.zv3_state_init(es, w, h)
    ds = ctypes.create_string_buffer(ssz); LIB.zv3_state_init(ds, w, h)
    total = 0
    prev_e = prev_d = None
    for i, f in enumerate(frames):
        cur = np.ascontiguousarray(ycocg_of(f))
        out = np.zeros(w * h * 9 + 65536, np.uint8)
        n = LIB.zv_encode_frame_v3s(
            prev_e.ctypes.data if prev_e is not None else None,
            cur.ctypes.data, w, h, out.ctypes.data, out.size, es)
        assert n > 0, f"enc frame {i} failed"
        total += n
        got = np.zeros_like(cur)
        r = LIB.zv_decode_frame_v3s(
            prev_d.ctypes.data if prev_d is not None else None,
            got.ctypes.data, w, h, out.ctypes.data, n, ds)
        assert r == n, f"dec frame {i} ret {r} != {n}"
        if not np.array_equal(got, cur):
            d = np.abs(got.astype(np.int32) - cur.astype(np.int32)).sum(-1)
            idx = np.argwhere(d > 0)[0]
            raise AssertionError(
                f"frame {i} mismatch @ {idx}: got {got[tuple(idx)]} "
                f"want {cur[tuple(idx)]} ({int(d.sum())} total err)")
        prev_e = cur
        prev_d = got.copy()
    return total


def run_stateful_lossy(frames, w, h, eps=4, eps_hi=8):
    # contract: decoder output == encoder reconstruction, and the
    # reconstruction must stay near the source within a sane bound
    ssz = LIB.zv3_state_size(w, h)
    es = ctypes.create_string_buffer(ssz); LIB.zv3_state_init(es, w, h)
    ds = ctypes.create_string_buffer(ssz); LIB.zv3_state_init(ds, w, h)
    total = 0
    prev_e = prev_d = None
    for i, f in enumerate(frames):
        cur = np.ascontiguousarray(ycocg_of(f))
        out = np.zeros(w * h * 9 + 65536, np.uint8)
        rec = np.zeros_like(cur)
        n = LIB.zv_encode_frame_v3sq(
            prev_e.ctypes.data if prev_e is not None else None,
            cur.ctypes.data, w, h, out.ctypes.data, out.size,
            eps, eps_hi, rec.ctypes.data, es)
        assert n > 0, f"enc frame {i} failed"
        total += n
        got = np.zeros_like(cur)
        r = LIB.zv_decode_frame_v3s(
            prev_d.ctypes.data if prev_d is not None else None,
            got.ctypes.data, w, h, out.ctypes.data, n, ds)
        assert r == n, f"dec frame {i} ret {r} != {n}"
        if not np.array_equal(got, rec):
            d = np.abs(got.astype(np.int32) - rec.astype(np.int32)).sum(-1)
            idx = np.argwhere(d > 0)[0]
            raise AssertionError(
                f"frame {i} recon mismatch @ {idx}: got {got[tuple(idx)]} "
                f"want {rec[tuple(idx)]} ({int(d.sum())} total err)")
        mx = np.abs(rec.astype(np.int32) - cur.astype(np.int32)).max()
        assert mx <= 64 * eps_hi, f"frame {i} error {mx} way out of bound"
        prev_e = rec
        prev_d = got.copy()
    return total


def main():
    cases = [
        ("static",  96,  96, 6),
        ("scroll",  96,  96, 6),
        ("scroll", 640, 360, 4),
        ("noise",   96,  96, 4),
        ("noise",   64,  48, 4),
        ("text",   128, 128, 6),
        ("scroll", 100,  60, 4),   # not TILE_SZ-aligned
        ("noise",   33,  17, 3),   # tiny odd dims
        ("static",  16,  16, 4),   # exactly one tile
        ("noise",   99,  25, 3),   # odd edge tiles force SPLIT remainder
        ("scroll",  17, 105, 3),   # odd height edge tiles
        ("noise",   17, 105, 3),
        ("scroll",  19,  19, 3),
        ("noise",  103,  33, 3),
        ("cfl",    96,  96, 6),   # affine chroma leaves + cross-frame replay
        ("cfl",    50,  34, 4),   # odd dims with CFL
        ("cflout", 64,  64, 4),   # CFL residuals beyond the hi-escape bound
        ("c420",   96,  96, 5),   # block-constant chroma -> half-res planes
        ("c420mix", 96,  96, 6),  # 420 flag toggling across frames
        ("band",   128,  48, 4),  # SPLIT row directly above RUN tiles
        ("band",   64,  48, 5),
    ]
    ok = True
    for kind, w, h, n in cases:
        frames = gen_frames(kind, w, h, n)
        try:
            a = run_stateless(frames, w, h)
            print(f"  stateless {kind:7s} {w}x{h}x{n}: {a}B OK")
        except AssertionError as e:
            ok = False; print(f"  stateless {kind} {w}x{h}: FAIL {e}")
        try:
            b = run_stateful(frames, w, h)
            print(f"  stateful  {kind:7s} {w}x{h}x{n}: {b}B OK")
        except AssertionError as e:
            ok = False; print(f"  stateful  {kind} {w}x{h}: FAIL {e}")
        try:
            c = run_stateful_lossy(frames, w, h)
            print(f"  lossy-sq  {kind:7s} {w}x{h}x{n}: {c}B OK")
        except AssertionError as e:
            ok = False; print(f"  lossy-sq  {kind} {w}x{h}: FAIL {e}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
