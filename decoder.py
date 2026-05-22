#!/usr/bin/env python3

import os
import sys
import time
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

_LIB.ycocgr_to_rgb_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb_frame.restype = None
_LIB.zv_decode_frame.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32,
]
_LIB.zv_decode_frame.restype = ctypes.c_int32

def ycocgr_to_rgb(ycocg: np.ndarray) -> np.ndarray:
    h, w, _ = ycocg.shape
    out = np.empty((h, w, 3), dtype=np.uint8)
    _LIB.ycocgr_to_rgb_frame(
        ycocg.ctypes.data_as(ctypes.c_void_p),
        out.ctypes.data_as(ctypes.c_void_p),
        w, h,
    )
    return out

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

        curr_ycocg = np.zeros((h, w, 3), dtype=np.int16)
        prev_ycocg = None
        delay = max(1, 1000 // fps if fps > 0 else 42)
        start = time.time()

        for fi in range(fc):
            buf = f.read(sizes[fi])
            in_buf = (ctypes.c_uint8 * len(buf)).from_buffer_copy(buf)
            del buf
            prev_ptr = prev_ycocg.ctypes.data_as(ctypes.c_void_p) if prev_ycocg is not None else None
            _LIB.zv_decode_frame(
                prev_ptr,
                curr_ycocg.ctypes.data_as(ctypes.c_void_p),
                w, h,
                ctypes.cast(in_buf, ctypes.c_void_p), len(in_buf),
            )
            rgb = ycocgr_to_rgb(curr_ycocg)
            bgr = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
            del rgb

            if writer is not None:
                writer.write(bgr)
            if playback:
                cv2.imshow("ZV Decoder", bgr)
                key = cv2.waitKey(delay) & 0xFF
                if key in (27, ord("q")):
                    break
            del bgr

            if prev_ycocg is None:
                prev_ycocg = curr_ycocg.copy()
            else:
                np.copyto(prev_ycocg, curr_ycocg)

            if (fi + 1) % 20 == 0 or fi == fc - 1:
                elapsed = time.time() - start
                print(f"  frame {fi+1}/{fc}  ({(fi+1)/elapsed:.1f} fps decode)")

        if writer is not None:
            writer.release()
            print(f"output: {output_path}")
        if playback:
            cv2.destroyAllWindows()

        print(f"done in {time.time()-start:.2f}s")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: python decoder.py <input.zv> [output.mp4] [--no-play]")
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
