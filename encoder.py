#!/usr/bin/env python3

import os
import sys
import struct
import ctypes
import numpy as np

try:
    import cv2
except ImportError:
    print("need opencv-python: pip install opencv-python")
    sys.exit(1)

ZV_VERSION = 10

_LIB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "libzv_core.dylib")
_LIB = ctypes.CDLL(_LIB_PATH)

_LIB.rgb_to_ycocgr_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.rgb_to_ycocgr_frame.restype = None
_LIB.ycocgr_to_rgb_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb_frame.restype = None

_LIB.zv_encode_frame.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32,
]
_LIB.zv_encode_frame.restype = ctypes.c_int32

def rgb_to_ycocgr(rgb: np.ndarray) -> np.ndarray:
    h, w, _ = rgb.shape
    out = np.empty((h, w, 3), dtype=np.int16)
    _LIB.rgb_to_ycocgr_frame(
        rgb.ctypes.data_as(ctypes.c_void_p),
        out.ctypes.data_as(ctypes.c_void_p),
        w, h,
    )
    return out

def encode(input_path: str, output_path: str):
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

    print(f"video: {w}x{h} @ {fps}fps, {fc} frames (expected)")

    max_frame_bytes = 8 * w * h + 4096
    out_buf = (ctypes.c_uint8 * max_frame_bytes)()

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
            n = _LIB.zv_encode_frame(
                prev_ptr,
                cur_ycocg.ctypes.data_as(ctypes.c_void_p),
                w, h, out_buf, max_frame_bytes,
            )
            if n <= 0:
                print(f"frame {fi}: encode failed"); sys.exit(1)
            fout.write(bytes(out_buf[:n]))
            sizes.append(int(n))
            total_bytes += n
            prev_ycocg = cur_ycocg
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

    src_size = os.path.getsize(input_path)
    zv_size = os.path.getsize(output_path)
    print()
    print(f"source   : {src_size:>14,} B")
    print(f"zv       : {zv_size:>14,} B   ({src_size/zv_size:.2f}x vs source)")
    print(f"payload  : {total_bytes:>14,} B")
    print(f"avg bpp  : {total_bytes*8/(fi*w*h):.3f}")
    print(f"output   : {output_path}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: python encoder.py <input_video> <output.zv>")
        sys.exit(1)
    encode(sys.argv[1], sys.argv[2])
