#!/usr/bin/env python3
import os
import sys
import time
import struct
import ctypes
import threading
from concurrent.futures import ThreadPoolExecutor

import numpy as np

try:
    import cv2
except ImportError:
    print("need opencv-python: pip install opencv-python")
    sys.exit(1)

ZV_VERSION = 11
DEFAULT_STRIPES = 8

_LIB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "libzv_core.dylib")
_LIB = ctypes.CDLL(_LIB_PATH)

_LIB.rgb_to_ycocgr_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.rgb_to_ycocgr_frame.restype = None

_LIB.zv_encode_plane_stripe.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32,
]
_LIB.zv_encode_plane_stripe.restype = ctypes.c_int32

def rgb_to_ycocgr(rgb: np.ndarray) -> np.ndarray:
    h, w, _ = rgb.shape
    out = np.empty((h, w, 3), dtype=np.int16)
    _LIB.rgb_to_ycocgr_frame(
        rgb.ctypes.data_as(ctypes.c_void_p),
        out.ctypes.data_as(ctypes.c_void_p),
        w, h,
    )
    return out

def stripe_boundaries(h: int, n: int, align: int = 16):
    n_blocks = h // align
    if n_blocks == 0:
        return [(0, h)]
    n = max(1, min(n, n_blocks))
    base = n_blocks // n
    extra = n_blocks - base * n
    boundaries = []
    y = 0
    for i in range(n):
        bh = (base + (1 if i < extra else 0)) * align
        boundaries.append((y, bh))
        y += bh
                                            
    if y < h:
        last_y, last_h = boundaries[-1]
        boundaries[-1] = (last_y, last_h + (h - y))
    return boundaries

def encode_plane_threaded(prev_plane, curr_plane, w, h, stripes, executor):
    has_prev = prev_plane is not None
    payloads = [None] * len(stripes)

    def encode_one(i, y0, sh):
                                       
        cur_offset = y0 * w
        cur_ptr = (curr_plane.ctypes.data + cur_offset * 2)                   
        if has_prev:
            prev_ptr = (prev_plane.ctypes.data + cur_offset * 2)
        else:
            prev_ptr = 0        
        max_buf = 4 * w * sh + 4096
        out_buf = (ctypes.c_uint8 * max_buf)()
        n = _LIB.zv_encode_plane_stripe(
            ctypes.c_void_p(prev_ptr),
            ctypes.c_void_p(cur_ptr),
            w, sh, out_buf, max_buf,
        )
        payloads[i] = bytes(out_buf[:n])

    futs = [executor.submit(encode_one, i, y0, sh)
            for i, (y0, sh) in enumerate(stripes)]
    for f in futs:
        f.result()
    return payloads

def encode(input_path: str, output_path: str, n_stripes: int = DEFAULT_STRIPES):
    cap = cv2.VideoCapture(input_path)
    if not cap.isOpened():
        print(f"cannot open: {input_path}"); sys.exit(1)

    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = int(cap.get(cv2.CAP_PROP_FPS)) or 24
    fc = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

    print(f"video: {w}x{h} @ {fps}fps, {fc} frames; stripes={n_stripes}")

    stripes = stripe_boundaries(h, n_stripes)
    n_real = len(stripes)
    print(f"  stripe layout: {n_real} stripes, heights = {[s[1] for s in stripes]}")

    prev_Y = prev_Co = prev_Cg = None
    sizes = []
    fi = 0
    total_bytes = 0
    encoder_pool = ThreadPoolExecutor(max_workers=max(8, n_real * 3))

    with open(output_path, "wb") as fout:
                
        fout.write(b"ZV")
        fout.write(bytes([ZV_VERSION]))
        fout.write(struct.pack(">HH", w, h))
        fout.write(bytes([fps & 0xFF]))
        fc_pos = fout.tell()
        fout.write(struct.pack(">I", fc))
        size_table_pos = fout.tell()
        fout.write(b"\x00\x00\x00\x00" * fc)

        t0 = time.time()
        while True:
            ok, frm = cap.read()
            if not ok:
                break
            rgb = cv2.cvtColor(frm, cv2.COLOR_BGR2RGB)
            cur_yc = rgb_to_ycocgr(rgb)
            del rgb
            cur_Y  = np.ascontiguousarray(cur_yc[..., 0])
            cur_Co = np.ascontiguousarray(cur_yc[..., 1])
            cur_Cg = np.ascontiguousarray(cur_yc[..., 2])
            del cur_yc

            all_tasks = []
            results = {}
            def enc_task(plane_id, stripe_id, prev_p, cur_p, y0, sh):
                cur_offset = y0 * w
                cur_ptr = (cur_p.ctypes.data + cur_offset * 2)
                prev_ptr = (prev_p.ctypes.data + cur_offset * 2) if prev_p is not None else 0
                max_buf = 4 * w * sh + 4096
                out_buf = (ctypes.c_uint8 * max_buf)()
                n = _LIB.zv_encode_plane_stripe(
                    ctypes.c_void_p(prev_ptr), ctypes.c_void_p(cur_ptr),
                    w, sh, out_buf, max_buf,
                )
                results[(plane_id, stripe_id)] = bytes(out_buf[:n])

            futs = []
            for plane_id, (prev_p, cur_p) in enumerate([
                (prev_Y, cur_Y), (prev_Co, cur_Co), (prev_Cg, cur_Cg)
            ]):
                for stripe_id, (y0, sh) in enumerate(stripes):
                    futs.append(encoder_pool.submit(
                        enc_task, plane_id, stripe_id, prev_p, cur_p, y0, sh
                    ))
            for f in futs:
                f.result()

            y_payloads  = [results[(0, i)] for i in range(n_real)]
            co_payloads = [results[(1, i)] for i in range(n_real)]
            cg_payloads = [results[(2, i)] for i in range(n_real)]

            frame_buf = bytearray()
            frame_buf.append(0)                                                       
            frame_buf.append(n_real)
            for plane_payloads in (y_payloads, co_payloads, cg_payloads):
                for p in plane_payloads:
                    frame_buf.extend(struct.pack(">I", len(p)))
                    frame_buf.extend(p)

            fout.write(bytes(frame_buf))
            sizes.append(len(frame_buf))
            total_bytes += len(frame_buf)

            prev_Y, prev_Co, prev_Cg = cur_Y, cur_Co, cur_Cg
            fi += 1
            if fi % 10 == 0 or fi == fc:
                elapsed = time.time() - t0
                avg_bpp = total_bytes * 8 / (fi * w * h)
                print(f"  frame {fi}/{fc}  size={sizes[-1]:,}B  "
                      f"avg={avg_bpp:.3f} bpp  {fi/elapsed:.1f} fps")

        if fi != fc:
            fout.seek(fc_pos)
            fout.write(struct.pack(">I", fi))
        fout.seek(size_table_pos)
        for sz in sizes:
            fout.write(struct.pack(">I", sz))
        if fi < fc:
            fout.seek(size_table_pos + fi * 4)
            fout.write(b"")
            fout.truncate(size_table_pos + fi * 4 + total_bytes)

    cap.release()
    encoder_pool.shutdown()

    src_size = os.path.getsize(input_path)
    zv_size = os.path.getsize(output_path)
    elapsed = time.time() - t0
    print()
    print(f"source   : {src_size:>14,} B")
    print(f"zv       : {zv_size:>14,} B   ({src_size/zv_size:.2f}x vs source)")
    print(f"payload  : {total_bytes:>14,} B")
    print(f"avg bpp  : {total_bytes*8/(fi*w*h):.3f}")
    print(f"time     : {elapsed:.2f}s   ({fi/elapsed:.1f} fps)")
    print(f"output   : {output_path}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: python encoder_mt.py <input_video> <output.zv> [num_stripes]")
        sys.exit(1)
    n = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_STRIPES
    encode(sys.argv[1], sys.argv[2], n)
