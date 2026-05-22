#!/usr/bin/env python3

import os, sys, ctypes, numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from encoder import rgb_to_ycocgr, _LIB
from decoder import ycocgr_to_rgb

_LIB.zv_decode_frame.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32,
]
_LIB.zv_decode_frame.restype = ctypes.c_int32

def roundtrip_frame(prev_rgb, curr_rgb, w, h):
    prev_yc = rgb_to_ycocgr(prev_rgb) if prev_rgb is not None else None
    curr_yc = rgb_to_ycocgr(curr_rgb)
    buf = (ctypes.c_uint8 * (8 * w * h + 4096))()
    prev_ptr = prev_yc.ctypes.data_as(ctypes.c_void_p) if prev_yc is not None else None
    n = _LIB.zv_encode_frame(prev_ptr, curr_yc.ctypes.data_as(ctypes.c_void_p),
                             w, h, buf, len(buf))
    payload = bytes(buf[:n])
    dec_yc = np.zeros_like(curr_yc)
    prev_ptr2 = prev_yc.ctypes.data_as(ctypes.c_void_p) if prev_yc is not None else None
    in_buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
    _LIB.zv_decode_frame(prev_ptr2, dec_yc.ctypes.data_as(ctypes.c_void_p),
                         w, h, ctypes.cast(in_buf, ctypes.c_void_p), len(payload))
    dec_rgb = ycocgr_to_rgb(dec_yc)
    ok_yc = np.array_equal(curr_yc, dec_yc)
    return ok_yc, n, dec_rgb

def main():
    np.random.seed(0)

    w, h = 32, 16
    frame0 = np.random.randint(0, 256, (h, w, 3), dtype=np.uint8)
    ok, n, _ = roundtrip_frame(None, frame0, w, h)
    print(f"test1 random keyframe  32x16   ok={ok}  {n}B  {n*8/(w*h):.2f} bpp")
    assert ok

    w, h = 128, 64
    yy, xx = np.mgrid[0:h, 0:w]
    frame1 = np.stack([((xx * 4) & 0xFF), ((yy * 4) & 0xFF), ((xx+yy) & 0xFF)], axis=-1).astype(np.uint8)
    ok, n, _ = roundtrip_frame(None, frame1, w, h)
    print(f"test2 gradient keyframe 128x64 ok={ok}  {n}B  {n*8/(w*h):.2f} bpp")
    assert ok

    frame2 = frame1.copy()
    ok, n, _ = roundtrip_frame(frame1, frame2, w, h)
    print(f"test3 identical inter  128x64  ok={ok}  {n}B  {n*8/(w*h):.4f} bpp")
    assert ok

    frame3 = frame1.copy()
    frame3[30, 40] = [0, 0, 0]
    ok, n, _ = roundtrip_frame(frame1, frame3, w, h)
    print(f"test4 one-pixel change 128x64  ok={ok}  {n}B  {n*8/(w*h):.4f} bpp")
    assert ok

    frame4 = frame1.copy()
    frame4[10, :, :] = np.random.randint(0, 256, (w, 3), dtype=np.uint8)
    ok, n, _ = roundtrip_frame(frame1, frame4, w, h)
    print(f"test5 one-row change   128x64  ok={ok}  {n}B  {n*8/(w*h):.4f} bpp")
    assert ok

    w, h = 256, 128
    frame5 = np.random.randint(0, 256, (h, w, 3), dtype=np.uint8)
    ok, n, _ = roundtrip_frame(None, frame5, w, h)
    print(f"test6 random keyframe  256x128 ok={ok}  {n}B  {n*8/(w*h):.2f} bpp")
    assert ok

    print("\nALL ROUND-TRIPS OK")

if __name__ == "__main__":
    main()
