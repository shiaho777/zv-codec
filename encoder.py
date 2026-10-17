#!/usr/bin/env python3

import os
import sys
import struct
import ctypes
from typing import Optional
import numpy as np

try:
    import cv2
except ImportError:
    print("need opencv-python: pip install opencv-python")
    sys.exit(1)

ZV_VERSION = 17   # 17 = v16 stateful tiles + reversible YCoCg-R (RGB-lossless at eps=0)

_LIB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "libzv_core.dylib")
_LIB = ctypes.CDLL(_LIB_PATH)

_LIB.rgb_to_ycocgr_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.rgb_to_ycocgr_frame.restype = None
_LIB.ycocgr_to_rgb_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb_frame.restype = None
_LIB.rgb_to_ycocgr_full_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.rgb_to_ycocgr_full_frame.restype = None
_LIB.ycocgr_to_rgb_full_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb_full_frame.restype = None
_LIB.rgb16_to_ycocgr_full_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.rgb16_to_ycocgr_full_frame.restype = None
_LIB.ycocgr_to_rgb16_full_frame.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb16_full_frame.restype = None
_LIB.zv_encode_set_stripes.argtypes = [ctypes.c_int]
_LIB.zv_encode_set_stripes.restype = None
_LIB.zv_encode_get_stripes.argtypes = []
_LIB.zv_encode_get_stripes.restype = ctypes.c_int

_LIB.zv3_state_size.argtypes = [ctypes.c_int, ctypes.c_int]
_LIB.zv3_state_size.restype = ctypes.c_int32
_LIB.zv3_state_init.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.zv3_state_init.restype = None

_LIB.zv_encode_frame_v3sq.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32,
    ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
]
_LIB.zv_encode_frame_v3sq.restype = ctypes.c_int32

def rgb_to_ycocgr(rgb: np.ndarray) -> np.ndarray:
    """Reversible YCoCg-R (v17).  Bit-exact with ycocgr_to_rgb_full."""
    h, w, _ = rgb.shape
    out = np.empty((h, w, 3), dtype=np.int16)
    _LIB.rgb_to_ycocgr_full_frame(
        rgb.ctypes.data_as(ctypes.c_void_p),
        out.ctypes.data_as(ctypes.c_void_p),
        w, h,
    )
    return out

def rgb_to_ycocgr_quant(rgb: np.ndarray) -> np.ndarray:
    """Legacy v10–v16 pre-quant (1-bit Y / 2-bit chroma)."""
    h, w, _ = rgb.shape
    out = np.empty((h, w, 3), dtype=np.int16)
    _LIB.rgb_to_ycocgr_frame(
        rgb.ctypes.data_as(ctypes.c_void_p),
        out.ctypes.data_as(ctypes.c_void_p),
        w, h,
    )
    return out

def rgb16_to_ycocgr(rgb: np.ndarray) -> np.ndarray:
    """10/16-bit RGB (uint16) → reversible YCoCg-R."""
    h, w, _ = rgb.shape
    rgb = np.ascontiguousarray(rgb, dtype=np.uint16)
    out = np.empty((h, w, 3), dtype=np.int16)
    _LIB.rgb16_to_ycocgr_full_frame(
        rgb.ctypes.data_as(ctypes.c_void_p),
        out.ctypes.data_as(ctypes.c_void_p),
        w, h,
    )
    return out

def encode(input_path: str, output_path: str, eps: int = 0, eps_hi: int = 0,
           n_stripes: Optional[int] = None, mux_audio: Optional[bool] = None):
    cap = cv2.VideoCapture(input_path)
    if not cap.isOpened():
        print(f"cannot open: {input_path}")
        sys.exit(1)

    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = int(cap.get(cv2.CAP_PROP_FPS)) or 24
    fc = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    if fc <= 0:
        fc = 0
        while cap.read()[0]: fc += 1
        cap.release()
        cap = cv2.VideoCapture(input_path)

    if n_stripes is not None:
        _LIB.zv_encode_set_stripes(int(n_stripes))
    print(f"video: {w}x{h} @ {fps}fps, {fc} frames (expected)  stripes={_LIB.zv_encode_get_stripes()}")

    max_frame_bytes = 8 * w * h + 65536
    out_buf = (ctypes.c_uint8 * max_frame_bytes)()
    # near-lossless: the decoder's reference is the RECONSTRUCTED frame,
    # so once eps>0 the previous recon (not the original) feeds prev
    if eps_hi <= eps:
        eps_hi = 4 * eps          # coarse tiles may reach 4x base bound
    recon_buf = np.empty((h, w, 3), dtype=np.int16) if eps > 0 else None

    enc_state = (ctypes.c_uint8 * _LIB.zv3_state_size(w, h))()
    _LIB.zv3_state_init(enc_state, w, h)

    prev_ycocg = None
    sizes = []
    fi = 0
    total_bytes = 0

    with open(output_path, "wb") as fout:
        fout.write(b"ZV")
        fout.write(bytes([ZV_VERSION]))
        fout.write(struct.pack(">HH", w, h))
        fout.write(bytes([fps & 0xFF]))
        fc_pos = fout.tell()
        fout.write(struct.pack(">I", fc))
        size_table_pos = fout.tell()
        fout.write(b"\x00\x00\x00\x00" * fc)

        while True:
            ok, frame_bgr = cap.read()
            if not ok: break
            rgb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
            cur_ycocg = rgb_to_ycocgr(rgb)
            del frame_bgr, rgb

            prev_ptr = prev_ycocg.ctypes.data_as(ctypes.c_void_p) if prev_ycocg is not None else None
            n = _LIB.zv_encode_frame_v3sq(
                prev_ptr,
                cur_ycocg.ctypes.data_as(ctypes.c_void_p),
                w, h, out_buf, max_frame_bytes,
                eps, eps_hi,
                recon_buf.ctypes.data_as(ctypes.c_void_p) if eps > 0 else None,
                enc_state,
            )
            if n <= 0:
                print(f"frame {fi}: encode failed"); sys.exit(1)
            fout.write(bytes(out_buf[:n]))
            sizes.append(int(n))
            total_bytes += n
            prev_ycocg = recon_buf if eps > 0 else cur_ycocg
            fi += 1
            if fi % 20 == 0 or fi == fc:
                avg_bpp = total_bytes * 8 / (fi * w * h)
                print(f"  frame {fi}/{fc}  last={n:,} B  avg={avg_bpp:.3f} bpp")

        if fi != fc:
            fout.seek(fc_pos)
            fout.write(struct.pack(">I", fi))
        fout.seek(size_table_pos)
        for sz in sizes:
            fout.write(struct.pack(">I", sz))
        if fi < fc:
            fout.seek(size_table_pos + fi * 4)
            payload_end = size_table_pos + fi * 4 + total_bytes
            fout.seek(payload_end)
            fout.truncate()

    cap.release()

    try:
        src_size = os.path.getsize(input_path)
    except OSError:
        import glob
        src_size = sum(os.path.getsize(p) for p in glob.glob(input_path))
    zv_size = os.path.getsize(output_path)
    print()
    print(f"source   : {src_size:>14,} B")
    print(f"zv       : {zv_size:>14,} B   ({src_size/zv_size:.2f}x vs source)")
    print(f"payload  : {total_bytes:>14,} B")
    print(f"avg bpp  : {total_bytes*8/(fi*w*h):.3f}")
    if eps > 0:
        print(f"eps      : {eps} (bound; coarse tiles <= {eps_hi})")
    else:
        print(f"color    : reversible YCoCg-R (RGB lossless)")
    print(f"output   : {output_path}")

    want_mux = mux_audio
    if want_mux is None:
        want_mux = output_path.endswith(".zvm")
    if want_mux:
        try:
            from zv_mux import encode_audio_from_video, pack
            got = encode_audio_from_video(input_path)
        except Exception as e:
            got = None
            print(f"audio mux skipped: {e}")
        if got:
            za, nfr, nch, sr = got
            with open(output_path, "rb") as f:
                video = f.read()
            muxed = pack(video, za, nfr, nch, sr)
            dest = output_path if output_path.endswith(".zvm") else output_path + ".zvm"
            with open(dest, "wb") as f:
                f.write(muxed)
            print(f"mux      : {dest}  video {len(video):,} B  audio {len(za):,} B  "
                  f"{nfr} frames {nch}ch @{sr}Hz")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: python encoder.py <input_video> <output.zv|.zvm> [eps] [eps_hi] [stripes]")
        sys.exit(1)
    eps = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    eps_hi = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    nst = int(sys.argv[5]) if len(sys.argv) > 5 else None
    encode(sys.argv[1], sys.argv[2], eps, eps_hi, n_stripes=nst)
