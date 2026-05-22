#!/usr/bin/env python3
import os
import sys
import time
import struct
import ctypes
from concurrent.futures import ThreadPoolExecutor

import numpy as np

try:
    import cv2
except ImportError:
    print("need opencv-python: pip install opencv-python")
    sys.exit(1)

ZV_VERSION = 11

_LIB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "libzv_core.dylib")
_LIB = ctypes.CDLL(_LIB_PATH)

_LIB.ycocgr_to_rgb_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb_frame.restype = None
_LIB.zv_decode_plane_stripe.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32,
]
_LIB.zv_decode_plane_stripe.restype = ctypes.c_int32

def ycocgr_to_rgb(yY: np.ndarray, yCo: np.ndarray, yCg: np.ndarray) -> np.ndarray:
    h, w = yY.shape
    yc = np.empty((h, w, 3), dtype=np.int16)
    yc[..., 0] = yY; yc[..., 1] = yCo; yc[..., 2] = yCg
    out = np.empty((h, w, 3), dtype=np.uint8)
    _LIB.ycocgr_to_rgb_frame(
        yc.ctypes.data_as(ctypes.c_void_p),
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

def decode(input_path: str, output_path=None, playback: bool = True):
    with open(input_path, "rb") as f:
        if f.read(2) != b"ZV":
            print("not a ZV file"); sys.exit(1)
        ver = f.read(1)[0]
        if ver != ZV_VERSION:
            print(f"unsupported version: {ver} (expected {ZV_VERSION})"); sys.exit(1)
        w, h = struct.unpack(">HH", f.read(4))
        fps = f.read(1)[0]
        fc, = struct.unpack(">I", f.read(4))
        sizes = [struct.unpack(">I", f.read(4))[0] for _ in range(fc)]

        print(f"ZV v{ver}: {w}x{h} @ {fps}fps, {fc} frames")

        writer = None
        if output_path:
            fourcc = cv2.VideoWriter_fourcc(*"mp4v")
            writer = cv2.VideoWriter(output_path, fourcc, fps, (w, h))
            if not writer.isOpened():
                writer = None
                print("cannot create writer")

        prev_Y = prev_Co = prev_Cg = None
        cur_Y  = np.empty((h, w), dtype=np.int16)
        cur_Co = np.empty((h, w), dtype=np.int16)
        cur_Cg = np.empty((h, w), dtype=np.int16)

        delay = max(1, 1000 // fps if fps > 0 else 42)
        decoder_pool = ThreadPoolExecutor(max_workers=24)

        start = time.time()
        for fi in range(fc):
            buf = f.read(sizes[fi])
            pos = 0
            flags = buf[pos]; pos += 1
            n_real = buf[pos]; pos += 1
            stripes = stripe_boundaries(h, n_real)

            stripe_payloads = [[None]*n_real for _ in range(3)]
            for plane_id in range(3):
                for sid in range(n_real):
                    sl, = struct.unpack(">I", buf[pos:pos+4]); pos += 4
                    stripe_payloads[plane_id][sid] = bytes(buf[pos:pos+sl]); pos += sl

            def dec_task(plane_id, sid):
                payload = stripe_payloads[plane_id][sid]
                y0, sh = stripes[sid]
                cur_p = [cur_Y, cur_Co, cur_Cg][plane_id]
                prev_p = [prev_Y, prev_Co, prev_Cg][plane_id]
                cur_offset = y0 * w
                cur_ptr = cur_p.ctypes.data + cur_offset * 2
                prev_ptr = (prev_p.ctypes.data + cur_offset * 2) if prev_p is not None else 0
                in_buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
                _LIB.zv_decode_plane_stripe(
                    ctypes.c_void_p(prev_ptr), ctypes.c_void_p(cur_ptr),
                    w, sh, ctypes.cast(in_buf, ctypes.c_void_p), len(payload),
                )

            futs = []
            for plane_id in range(3):
                for sid in range(n_real):
                    futs.append(decoder_pool.submit(dec_task, plane_id, sid))
            for ft in futs:
                ft.result()

            rgb = ycocgr_to_rgb(cur_Y, cur_Co, cur_Cg)
            bgr = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)

            if writer is not None:
                writer.write(bgr)
            if playback:
                cv2.imshow("ZV Decoder", bgr)
                key = cv2.waitKey(delay) & 0xFF
                if key in (27, ord("q")):
                    break

            if prev_Y is None:
                prev_Y  = cur_Y.copy()
                prev_Co = cur_Co.copy()
                prev_Cg = cur_Cg.copy()
            else:
                np.copyto(prev_Y,  cur_Y)
                np.copyto(prev_Co, cur_Co)
                np.copyto(prev_Cg, cur_Cg)

            if (fi + 1) % 20 == 0 or fi == fc - 1:
                elapsed = time.time() - start
                print(f"  frame {fi+1}/{fc}  ({(fi+1)/elapsed:.1f} fps decode)")

        if writer is not None:
            writer.release()
            print(f"output: {output_path}")
        if playback:
            cv2.destroyAllWindows()
        decoder_pool.shutdown()
        print(f"done in {time.time()-start:.2f}s")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: python decoder_mt.py <input.zv> [output.mp4] [--no-play]")
        sys.exit(1)
    inp = sys.argv[1]
    out = None
    play = True
    for a in sys.argv[2:]:
        if a == "--no-play":
            play = False
        else:
            out = a
    decode(inp, out, play)
