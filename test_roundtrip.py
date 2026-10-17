#!/usr/bin/env python3

import os, sys, ctypes, numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from encoder import rgb_to_ycocgr, rgb16_to_ycocgr, _LIB
from decoder import ycocgr_to_rgb, ycocgr_to_rgb16

_LIB.zv_encode_frame_v3s.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p]
_LIB.zv_encode_frame_v3s.restype = ctypes.c_int32
_LIB.zv_decode_frame_v3s.argtypes = _LIB.zv_encode_frame_v3s.argtypes[:]
_LIB.zv_decode_frame_v3s.restype = ctypes.c_int32
_LIB.zv3_state_size.argtypes = [ctypes.c_int, ctypes.c_int]
_LIB.zv3_state_size.restype = ctypes.c_int32
_LIB.zv3_state_init.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.zv3_state_init.restype = None

for _fn in ("zv_encode_frame", "zv_decode_frame",
            "zv_encode_frame_v2", "zv_decode_frame_v2",
            "zv_encode_frame_v3", "zv_decode_frame_v3"):
    _f = getattr(_LIB, _fn)
    _f.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p,
        ctypes.c_int, ctypes.c_int,
        ctypes.c_void_p, ctypes.c_int32,
    ]
    _f.restype = ctypes.c_int32

def make_roundtrip(enc_fn, dec_fn):
    def roundtrip_frame(prev_rgb, curr_rgb, w, h):
        prev_yc = rgb_to_ycocgr(prev_rgb) if prev_rgb is not None else None
        curr_yc = rgb_to_ycocgr(curr_rgb)
        buf = (ctypes.c_uint8 * (8 * w * h + 65536))()
        prev_ptr = prev_yc.ctypes.data_as(ctypes.c_void_p) if prev_yc is not None else None
        n = enc_fn(prev_ptr, curr_yc.ctypes.data_as(ctypes.c_void_p),
                   w, h, buf, len(buf))
        payload = bytes(buf[:n])
        dec_yc = np.zeros_like(curr_yc)
        prev_ptr2 = prev_yc.ctypes.data_as(ctypes.c_void_p) if prev_yc is not None else None
        in_buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
        dec_fn(prev_ptr2, dec_yc.ctypes.data_as(ctypes.c_void_p),
               w, h, ctypes.cast(in_buf, ctypes.c_void_p), len(payload))
        dec_rgb = ycocgr_to_rgb(dec_yc)
        ok_yc = np.array_equal(curr_yc, dec_yc)
        return ok_yc, n, dec_rgb
    return roundtrip_frame

def run_suite(tag, roundtrip_frame):
    np.random.seed(0)

    w, h = 32, 16
    frame0 = np.random.randint(0, 256, (h, w, 3), dtype=np.uint8)
    ok, n, _ = roundtrip_frame(None, frame0, w, h)
    print(f"[{tag}] test1 random keyframe  32x16   ok={ok}  {n}B  {n*8/(w*h):.2f} bpp")
    assert ok

    w, h = 128, 64
    yy, xx = np.mgrid[0:h, 0:w]
    frame1 = np.stack([((xx * 4) & 0xFF), ((yy * 4) & 0xFF), ((xx+yy) & 0xFF)], axis=-1).astype(np.uint8)
    ok, n, _ = roundtrip_frame(None, frame1, w, h)
    print(f"[{tag}] test2 gradient keyframe 128x64 ok={ok}  {n}B  {n*8/(w*h):.2f} bpp")
    assert ok

    frame2 = frame1.copy()
    ok, n, _ = roundtrip_frame(frame1, frame2, w, h)
    print(f"[{tag}] test3 identical inter  128x64  ok={ok}  {n}B  {n*8/(w*h):.4f} bpp")
    assert ok

    frame3 = frame1.copy()
    frame3[30, 40] = [0, 0, 0]
    ok, n, _ = roundtrip_frame(frame1, frame3, w, h)
    print(f"[{tag}] test4 one-pixel change 128x64  ok={ok}  {n}B  {n*8/(w*h):.4f} bpp")
    assert ok

    frame4 = frame1.copy()
    frame4[10, :, :] = np.random.randint(0, 256, (w, 3), dtype=np.uint8)
    ok, n, _ = roundtrip_frame(frame1, frame4, w, h)
    print(f"[{tag}] test5 one-row change   128x64  ok={ok}  {n}B  {n*8/(w*h):.4f} bpp")
    assert ok

    w, h = 256, 128
    frame5 = np.random.randint(0, 256, (h, w, 3), dtype=np.uint8)
    ok, n, _ = roundtrip_frame(None, frame5, w, h)
    print(f"[{tag}] test6 random keyframe  256x128 ok={ok}  {n}B  {n*8/(w*h):.2f} bpp")
    assert ok

    # repeated blocks: exercises tile ops (ICOPY/PAL) on screen-like content
    w, h = 256, 128
    frame6 = np.zeros((h, w, 3), dtype=np.uint8) + 30
    strip = np.random.randint(0, 256, (16, w, 3)).astype(np.uint8)
    frame6[0:16] = strip; frame6[32:48] = strip
    frame6[64:80] = strip; frame6[96:112] = strip
    col = np.random.randint(0, 256, (h, 16, 3)).astype(np.uint8)
    frame6[:, 16:32] = col; frame6[:, 48:64] = col; frame6[:, 80:96] = col
    ok, n, _ = roundtrip_frame(None, frame6, w, h)
    print(f"[{tag}] test7 repeated blocks  256x128 ok={ok}  {n}B  {n*8/(w*h):.2f} bpp")
    assert ok

def make_roundtrip_v3s():
    def roundtrip_frame(prev_rgb, curr_rgb, w, h):
        prev_yc = rgb_to_ycocgr(prev_rgb) if prev_rgb is not None else None
        curr_yc = rgb_to_ycocgr(curr_rgb)
        buf = (ctypes.c_uint8 * (8 * w * h + 65536))()
        st_e = (ctypes.c_uint8 * _LIB.zv3_state_size(w, h))()
        st_d = (ctypes.c_uint8 * _LIB.zv3_state_size(w, h))()
        _LIB.zv3_state_init(st_e, w, h)
        _LIB.zv3_state_init(st_d, w, h)
        prev_ptr = prev_yc.ctypes.data_as(ctypes.c_void_p) if prev_yc is not None else None
        n = _LIB.zv_encode_frame_v3s(
            prev_ptr, curr_yc.ctypes.data_as(ctypes.c_void_p),
            w, h, buf, len(buf), st_e)
        payload = bytes(buf[:n])
        dec_yc = np.zeros_like(curr_yc)
        in_buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
        _LIB.zv_decode_frame_v3s(
            prev_ptr, dec_yc.ctypes.data_as(ctypes.c_void_p),
            w, h, ctypes.cast(in_buf, ctypes.c_void_p), len(payload), st_d)
        dec_rgb = ycocgr_to_rgb(dec_yc, ver=17)
        ok_yc = np.array_equal(curr_yc, dec_yc)
        ok_rgb = np.array_equal(curr_rgb, dec_rgb)
        return ok_yc and ok_rgb, n, dec_rgb
    return roundtrip_frame

def run_v18_rgb10():
    w, h = 96, 48
    rng = np.random.default_rng(2)
    a = rng.integers(0, 1024, (h, w, 3), dtype=np.uint16)
    b = a.copy()
    b[3, 5, 0] = (int(a[3, 5, 0]) + 13) % 1024
    frames = [a, b]
    st_e = (ctypes.c_uint8 * _LIB.zv3_state_size(w, h))()
    st_d = (ctypes.c_uint8 * _LIB.zv3_state_size(w, h))()
    _LIB.zv3_state_init(st_e, w, h)
    _LIB.zv3_state_init(st_d, w, h)
    prev = None
    for i, rgb in enumerate(frames):
        yc = rgb16_to_ycocgr(rgb)
        buf = (ctypes.c_uint8 * (8 * w * h + 65536))()
        prev_yc = rgb16_to_ycocgr(frames[i - 1]) if i else None
        prev_ptr = prev_yc.ctypes.data_as(ctypes.c_void_p) if prev_yc is not None else None
        n = _LIB.zv_encode_frame_v3s(
            prev_ptr, yc.ctypes.data_as(ctypes.c_void_p),
            w, h, buf, len(buf), st_e)
        assert n > 0, n
        payload = bytes(buf[:n])
        dec_yc = np.zeros_like(yc)
        in_buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
        _LIB.zv_decode_frame_v3s(
            prev_ptr, dec_yc.ctypes.data_as(ctypes.c_void_p),
            w, h, ctypes.cast(in_buf, ctypes.c_void_p), len(payload), st_d)
        rec = ycocgr_to_rgb16(dec_yc, 1023)
        assert np.array_equal(rec, rgb), f"v18 frame {i} not lossless 10-bit"
        print(f"[v18-RGB10] frame {i}  {n}B  ok=True")
    print("[v18-RGB10] 10-bit RGB round-trip ok")


def run_mux():
    from zv_mux import pack, unpack
    video = b"ZV" + bytes([17]) + b"\x00" * 20
    audio = b"ZA" + bytes([2]) + b"\x11" * 40
    blob = pack(video, audio, n_frames=10, n_channels=2, sample_rate=44100)
    v2, meta = unpack(blob)
    assert v2 == video
    assert meta["payload"] == audio
    assert meta["n_frames"] == 10 and meta["n_channels"] == 2
    assert meta["sample_rate"] == 44100
    v3, meta3 = unpack(pack(video, None))
    assert v3 == video and meta3 is None
    print("[ZVM1] pack/unpack ok")


def main():
    run_suite("v1-AC",   make_roundtrip(_LIB.zv_encode_frame,    _LIB.zv_decode_frame))
    run_suite("v2-rANS", make_roundtrip(_LIB.zv_encode_frame_v2, _LIB.zv_decode_frame_v2))
    run_suite("v3-prog", make_roundtrip(_LIB.zv_encode_frame_v3, _LIB.zv_decode_frame_v3))
    run_suite("v17-RGB", make_roundtrip_v3s())
    run_v18_rgb10()
    run_mux()
    print("\nALL ROUND-TRIPS OK")

if __name__ == "__main__":
    main()
