#!/usr/bin/env python3
"""Fixed small screen-content scoreboard for the shipped v16 path.

Calls zv_encode_frame_v3sq and zv_decode_frame_v3s — the same entry
points encoder.py / decoder.py use — on four deterministic classes
(static, scroll, text, near-still). Each class is at most 128x64 and
4 frames. Prints the metrics the iteration gate compares, and asserts
lossless bit-identity plus the near-lossless contract already enforced
by test_synthetic.py (decoder == encoder reconstruction, max abs error
<= 64 * eps_hi in YCoCg).
"""
import os
import sys
import time
import ctypes
import numpy as np

# Codec knobs are process-wide and cached on first use. Drop them so the
# scoreboard always measures the shipped defaults.
for _k in list(os.environ):
    if _k.startswith("Z") and _k.isupper():
        os.environ.pop(_k, None)

HERE = os.path.dirname(os.path.abspath(__file__))
LIB = ctypes.CDLL(os.path.join(HERE, "libzv_core.dylib"))

LIB.rgb_to_ycocgr_frame.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
LIB.rgb_to_ycocgr_frame.restype = None
LIB.zv3_state_size.argtypes = [ctypes.c_int, ctypes.c_int]
LIB.zv3_state_size.restype = ctypes.c_int32
LIB.zv3_state_init.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
LIB.zv3_state_init.restype = None
LIB.zv_encode_frame_v3sq.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32, ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_void_p]
LIB.zv_encode_frame_v3sq.restype = ctypes.c_int32
LIB.zv_decode_frame_v3s.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p]
LIB.zv_decode_frame_v3s.restype = ctypes.c_int32

EPS = 4
EPS_HI = 8
# Pre-existing near-lossless guard from test_synthetic.run_stateful_lossy.
ERR_BOUND = 64 * EPS_HI
CLASSES = ("static", "scroll", "text", "nearstill")


def _ui(w, h, dx, cursor, dense):
    """Desktop-like frame: title bar, glyph runs, optional cursor."""
    img = np.zeros((h, w, 3), dtype=np.uint8)
    img[:, :] = (32, 36, 42)
    x0 = 4 + dx
    x1 = w - 4
    if x0 < 0:
        x0 = 0
    img[4:h - 4, x0:x1] = (22, 24, 28)
    img[4:16, x0:x1] = (46, 78, 120)
    # three traffic-light dots
    for i, col in enumerate(((220, 80, 70), (230, 180, 60), (80, 190, 90))):
        img[7:11, x0 + 6 + i * 8:x0 + 10 + i * 8] = col
    rows = range(0, h - 28, 6 if dense else 10)
    for row, y0 in enumerate(rows):
        y = 20 + y0
        if y + 3 >= h - 4:
            break
        span = 18 + ((row * 13 + 7) % (w // 3))
        x = x0 + 8
        if x + span > x1 - 2:
            span = max(0, x1 - 2 - x)
        img[y:y + 2, x:x + span] = (214, 214, 206)
        gap = x + span + 3
        if gap + 8 < x1:
            img[y:y + 2, gap:gap + 8] = (90, 170, 110)
    cy = 20 + (cursor % 4) * (6 if dense else 10)
    cx = x0 + 8 + (cursor % 5)
    if 0 <= cy < h - 6 and 0 <= cx < w - 3:
        img[cy:cy + 5, cx:cx + 2] = (240, 240, 236)
    return img


def make_classes():
    """Return {name: (w, h, list of rgb uint8 frames)}."""
    out = {}
    w, h, n = 96, 64, 4
    out["static"] = (w, h, [_ui(w, h, 0, 0, False) for _ in range(n)])
    out["scroll"] = (w, h, [_ui(w, h, i * 2, 0, False) for i in range(n)])
    w, h = 128, 64
    out["text"] = (w, h, [_ui(w, h, i, 0, True) for i in range(n)])
    w, h = 96, 64
    out["nearstill"] = (w, h, [_ui(w, h, 0, i, False) for i in range(n)])
    return out


def to_yc(rgb):
    h, w, _ = rgb.shape
    yc = np.empty((h, w, 3), dtype=np.int16)
    LIB.rgb_to_ycocgr_frame(
        np.ascontiguousarray(rgb).ctypes.data_as(ctypes.c_void_p),
        yc.ctypes.data_as(ctypes.c_void_p), w, h)
    return yc


def _fresh(w, h):
    n = LIB.zv3_state_size(w, h)
    buf = ctypes.create_string_buffer(n)
    LIB.zv3_state_init(buf, w, h)
    return buf


def encode_stream(frames_yc, w, h, eps, eps_hi):
    """Encode from a fresh state. Returns (payload_bytes, list of packets, recon_list or None)."""
    es = _fresh(w, h)
    cap = 8 * w * h + 65536
    out = np.zeros(cap, np.uint8)
    prev = None
    packets = []
    recons = []
    total = 0
    for cur in frames_yc:
        rec = np.empty_like(cur) if eps > 0 else None
        n = LIB.zv_encode_frame_v3sq(
            prev.ctypes.data if prev is not None else None,
            cur.ctypes.data, w, h, out.ctypes.data, cap,
            eps, eps_hi,
            rec.ctypes.data if rec is not None else None,
            es)
        if n <= 0:
            raise RuntimeError(f"encode failed ({w}x{h} eps={eps})")
        packets.append(bytes(out[:n]))
        total += n
        if eps > 0:
            recons.append(rec.copy())
            prev = rec
        else:
            recons.append(None)
            prev = cur
    return total, packets, recons


def decode_stream(packets, frames_yc, w, h):
    ds = _fresh(w, h)
    prev = None
    max_err = 0
    gots = []
    for pkt, cur in zip(packets, frames_yc):
        raw = np.frombuffer(pkt, dtype=np.uint8)
        got = np.zeros_like(cur)
        r = LIB.zv_decode_frame_v3s(
            prev.ctypes.data if prev is not None else None,
            got.ctypes.data, w, h, raw.ctypes.data, len(pkt), ds)
        if r != len(pkt):
            raise RuntimeError(f"decode ret {r} != {len(pkt)}")
        gots.append(got)
        prev = got
    return gots


def check_pair(frames_yc, gots, recons, eps):
    max_err = 0
    for i, (cur, got) in enumerate(zip(frames_yc, gots)):
        if eps == 0:
            if not np.array_equal(got, cur):
                d = np.abs(got.astype(np.int32) - cur.astype(np.int32))
                raise AssertionError(f"lossless mismatch frame {i} max {d.max()}")
        else:
            rec = recons[i]
            if not np.array_equal(got, rec):
                d = np.abs(got.astype(np.int32) - rec.astype(np.int32))
                raise AssertionError(
                    f"recon mismatch frame {i} max {d.max()}")
            err = int(np.abs(rec.astype(np.int32) - cur.astype(np.int32)).max())
            if err > max_err:
                max_err = err
            if err > ERR_BOUND:
                raise AssertionError(
                    f"frame {i} error {err} exceeds bound {ERR_BOUND}")
    return max_err


def median_time(fn):
    """Time fn three times and return (last_result, median_seconds).

    When the result is an encode tuple, also require the payload byte
    count to be identical across the three runs.
    """
    samples = []
    last = None
    payloads = []
    for _ in range(3):
        t0 = time.perf_counter()
        last = fn()
        samples.append(time.perf_counter() - t0)
        if isinstance(last, tuple) and last and isinstance(last[0], int):
            payloads.append(last[0])
    if len(payloads) == 3 and any(p != payloads[0] for p in payloads):
        raise AssertionError(f"nondeterministic payload {payloads}")
    samples.sort()
    return last, samples[1]


def run_class(name, w, h, rgbs, eps):
    yc = [to_yc(f) for f in rgbs]
    (total, packets, recons), enc_s = median_time(
        lambda: encode_stream(yc, w, h, eps, EPS_HI if eps else 0))
    (gots,), dec_s = median_time(lambda: (decode_stream(packets, yc, w, h),))
    # median_time wraps the tuple; decode_stream returns a list.
    err = check_pair(yc, gots, recons, eps)
    return total, enc_s, dec_s, err


def main():
    classes = make_classes()
    # Guard the workload contract so a future edit cannot silently grow it.
    for name, (w, h, frames) in classes.items():
        assert w <= 640 and h <= 360 and len(frames) <= 8, name
        assert frames[0].shape == (h, w, 3)

    sums = {
        "ll_bytes": 0, "ll_enc": 0.0, "ll_dec": 0.0, "ll_err": 0,
        "nl_bytes": 0, "nl_enc": 0.0, "nl_dec": 0.0, "nl_err": 0,
    }
    per = {}
    for name in CLASSES:
        w, h, frames = classes[name]
        b, ee, de, err = run_class(name, w, h, frames, 0)
        nb, ne, nd, nerr = run_class(name, w, h, frames, EPS)
        per[name] = (b, nb, err, nerr, ee, de, ne, nd)
        sums["ll_bytes"] += b
        sums["ll_enc"] += ee
        sums["ll_dec"] += de
        sums["ll_err"] = max(sums["ll_err"], err)
        sums["nl_bytes"] += nb
        sums["nl_enc"] += ne
        sums["nl_dec"] += nd
        sums["nl_err"] = max(sums["nl_err"], nerr)
        print(f"class {name} lossless_bytes {b} nearlossless_bytes {nb} "
              f"ll_err {err} nl_err {nerr}")

    print(f"lossless_payload_bytes {sums['ll_bytes']}")
    print(f"lossless_encode_seconds {sums['ll_enc']:.6f}")
    print(f"lossless_decode_seconds {sums['ll_dec']:.6f}")
    print(f"lossless_max_abs_error {sums['ll_err']}")
    print(f"nearlossless_payload_bytes {sums['nl_bytes']}")
    print(f"nearlossless_encode_seconds {sums['nl_enc']:.6f}")
    print(f"nearlossless_decode_seconds {sums['nl_dec']:.6f}")
    print(f"nearlossless_max_abs_error {sums['nl_err']}")
    if sums["ll_err"] != 0:
        raise SystemExit("lossless max error is not 0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
