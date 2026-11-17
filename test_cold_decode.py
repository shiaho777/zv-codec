"""Regression test: encode in one process, decode in a FRESH process.

The original test suite encodes and decodes in the same process, which
masked a fatal bug: the decoder read uninitialized scratch memory that
the encoder happened to fill with near-correct pixels.  A standalone
decoder (decoder.py, zv_studio.py, any real player) failed on every
frame.  This test reproduces the real-world path - encode to a file,
then decode that file in a separate process with no encoder state -
and asserts bit-exact RGB reconstruction.

Run:  python3 test_cold_decode.py
"""
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def _make_frames(w, h, n):
    """Synthetic screen-like content: flat background, moving window,
    scrolling text band, a few colored rects.  No external assets."""
    rng = np.random.default_rng(42)
    frames = []
    bg = np.full((h, w, 3), 24, dtype=np.uint8)
    for i in range(n):
        fr = bg.copy()
        # moving window
        x = 40 + (i * 7) % (w - 200)
        fr[60:200, x:x + 180] = [200, 205, 210]
        fr[60:72, x:x + 180] = [60, 90, 160]
        # scrolling text band (high-contrast edges)
        off = (i * 3) % 40
        for cx in range(-off, w, 40):
            if 0 <= cx < w - 8:
                fr[300:316, cx:cx + 8] = [230, 230, 230]
        # static colored rects
        fr[400:440, 100:260] = [180, 60, 60]
        fr[400:440, 300:460] = [60, 160, 80]
        # sparse noise (exercises residual / escape paths)
        mask = rng.random((h, w)) < 0.002
        fr[mask] = rng.integers(0, 256, (mask.sum(), 3), dtype=np.uint8)
        frames.append(fr)
    return frames


def _encode_to_file(frames, path, w, h):
    import ctypes
    from encoder import _LIB, rgb_to_ycocgr
    fc = len(frames)
    buf = (ctypes.c_uint8 * (8 * w * h + 65536))()
    st = (ctypes.c_uint8 * _LIB.zv3_state_size(w, h))()
    _LIB.zv3_state_init(st, w, h)
    prev = None
    payloads = []
    for fr in frames:
        yc = rgb_to_ycocgr(fr)
        pp = prev.ctypes.data_as(ctypes.c_void_p) if prev is not None else None
        n = _LIB.zv_encode_frame_v3sq(
            pp, yc.ctypes.data_as(ctypes.c_void_p), w, h,
            buf, len(buf), 0, 0, None, st)
        assert n > 0, f"encode returned {n}"
        payloads.append(bytes(buf[:n]))
        prev = yc
    with open(path, "wb") as f:
        f.write(b"ZV")
        f.write(bytes([17]))
        f.write(struct.pack(">HH", w, h))
        f.write(bytes([30]))
        f.write(struct.pack(">I", fc))
        for p in payloads:
            f.write(struct.pack(">I", len(p)))
        for p in payloads:
            f.write(p)


def _decode_in_fresh_process(zv_path, raw_out, w, h, fc):
    """Spawn a separate Python process that ONLY decodes - no encoder
    import, no shared state.  This is the real-world standalone path."""
    script = f'''
import sys, os, struct, ctypes
import numpy as np
sys.path.insert(0, {HERE!r})
from decoder import _LIB, ycocgr_to_rgb, _decode_frame_fn
w, h, fc = {w}, {h}, {fc}
f = open({zv_path!r}, "rb")
assert f.read(2) == b"ZV"
ver = f.read(1)[0]
f.read(5)
fc2, = struct.unpack(">I", f.read(4))
sizes = [struct.unpack(">I", f.read(4))[0] for _ in range(fc2)]
dec_fn = _decode_frame_fn(ver)
st = (ctypes.c_uint8 * _LIB.zv3_state_size(w, h))()
_LIB.zv3_state_init(st, w, h)
curr = np.zeros((h, w, 3), dtype=np.int16)
prev = None
out = open({raw_out!r}, "wb")
for fi in range(fc):
    buf = f.read(sizes[fi])
    ib = (ctypes.c_uint8 * len(buf)).from_buffer_copy(buf)
    pp = prev.ctypes.data_as(ctypes.c_void_p) if prev is not None else None
    r = dec_fn(pp, curr.ctypes.data_as(ctypes.c_void_p), w, h,
               ctypes.cast(ib, ctypes.c_void_p), len(ib), st)
    assert r >= 0, f"frame {{fi}} decode returned {{r}}"
    rgb = ycocgr_to_rgb(curr, ver)
    out.write(rgb.tobytes())
    if prev is None:
        prev = curr.copy()
    else:
        np.copyto(prev, curr)
out.close()
'''
    res = subprocess.run([sys.executable, "-c", script],
                         capture_output=True, text=True, timeout=120)
    if res.returncode != 0:
        raise RuntimeError(f"cold decode process failed:\n{res.stderr}")


def main():
    w, h, fc = 320, 240, 20
    frames = _make_frames(w, h, fc)
    with tempfile.TemporaryDirectory() as td:
        zv = os.path.join(td, "test.zv")
        raw = os.path.join(td, "decoded.rgb")
        _encode_to_file(frames, zv, w, h)
        _decode_in_fresh_process(zv, raw, w, h, fc)
        decoded = np.frombuffer(open(raw, "rb").read(), np.uint8)
        decoded = decoded.reshape(fc, h, w, 3)
        for i, fr in enumerate(frames):
            assert np.array_equal(decoded[i], fr), \
                f"frame {i} NOT bit-exact in cold decode"
    print(f"COLD-DECODE REGRESSION OK: {fc} frames, {w}x{h}, bit-exact")


if __name__ == "__main__":
    main()
