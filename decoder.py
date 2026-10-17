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

ZV_VERSIONS = (10, 12, 14, 16, 17, 18)  # 17 = RGB8 lossless, 18 = RGB10

_LIB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "libzv_core.dylib")
_LIB = ctypes.CDLL(_LIB_PATH)

_LIB.ycocgr_to_rgb_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb_frame.restype = None
_LIB.ycocgr_to_rgb_full_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb_full_frame.restype = None
_LIB.ycocgr_to_rgb16_full_frame.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
_LIB.ycocgr_to_rgb16_full_frame.restype = None
for _fn in ("zv_decode_frame", "zv_decode_frame_v2", "zv_decode_frame_v3"):
    _f = getattr(_LIB, _fn)
    _f.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p,
        ctypes.c_int, ctypes.c_int,
        ctypes.c_void_p, ctypes.c_int32,
    ]
    _f.restype = ctypes.c_int32

_LIB.zv3_state_size.argtypes = [ctypes.c_int, ctypes.c_int]
_LIB.zv3_state_size.restype = ctypes.c_int32
_LIB.zv3_state_init.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.zv3_state_init.restype = None
_LIB.zv_decode_frame_v3s.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p,
]
_LIB.zv_decode_frame_v3s.restype = ctypes.c_int32

def _decode_frame_fn(ver):
    if ver == 10: return _LIB.zv_decode_frame
    if ver == 12: return _LIB.zv_decode_frame_v2
    if ver in (16, 17, 18): return _LIB.zv_decode_frame_v3s
    return _LIB.zv_decode_frame_v3

def ycocgr_to_rgb(ycocg: np.ndarray, ver: int = 16) -> np.ndarray:
    h, w, _ = ycocg.shape
    if ver >= 18:
        out16 = np.empty((h, w, 3), dtype=np.uint16)
        _LIB.ycocgr_to_rgb16_full_frame(
            ycocg.ctypes.data_as(ctypes.c_void_p),
            out16.ctypes.data_as(ctypes.c_void_p),
            w, h, 1023)
        return np.clip(out16 >> 2, 0, 255).astype(np.uint8)
    out = np.empty((h, w, 3), dtype=np.uint8)
    fn = _LIB.ycocgr_to_rgb_full_frame if ver >= 17 else _LIB.ycocgr_to_rgb_frame
    fn(
        ycocg.ctypes.data_as(ctypes.c_void_p),
        out.ctypes.data_as(ctypes.c_void_p),
        w, h,
    )
    return out

def ycocgr_to_rgb16(ycocg: np.ndarray, maxc: int = 1023) -> np.ndarray:
    h, w, _ = ycocg.shape
    out = np.empty((h, w, 3), dtype=np.uint16)
    _LIB.ycocgr_to_rgb16_full_frame(
        ycocg.ctypes.data_as(ctypes.c_void_p),
        out.ctypes.data_as(ctypes.c_void_p),
        w, h, maxc)
    return out

def decode(input_path: str, output_path=None, playback: bool = True):
    from zv_mux import open_video_file, decode_audio
    f, audio_meta = open_video_file(input_path)
    try:
        if f.read(2) != b"ZV":
            print("not a ZV file"); sys.exit(1)
        ver = f.read(1)[0]
        if ver not in ZV_VERSIONS:
            print(f"unsupported version: {ver} (expected one of {ZV_VERSIONS})"); sys.exit(1)
        decode_frame = _decode_frame_fn(ver)
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
        dec_state = None
        if ver in (16, 17, 18):
            dec_state = (ctypes.c_uint8 * _LIB.zv3_state_size(w, h))()
            _LIB.zv3_state_init(dec_state, w, h)
        delay = max(1, 1000 // fps if fps > 0 else 42)
        start = time.time()

        for fi in range(fc):
            buf = f.read(sizes[fi])
            in_buf = (ctypes.c_uint8 * len(buf)).from_buffer_copy(buf)
            del buf
            prev_ptr = prev_ycocg.ctypes.data_as(ctypes.c_void_p) if prev_ycocg is not None else None
            if ver in (16, 17, 18):
                decode_frame(
                    prev_ptr,
                    curr_ycocg.ctypes.data_as(ctypes.c_void_p),
                    w, h,
                    ctypes.cast(in_buf, ctypes.c_void_p), len(in_buf),
                    dec_state,
                )
            else:
                decode_frame(
                    prev_ptr,
                    curr_ycocg.ctypes.data_as(ctypes.c_void_p),
                    w, h,
                    ctypes.cast(in_buf, ctypes.c_void_p), len(in_buf),
                )
            rgb = ycocgr_to_rgb(curr_ycocg, ver)
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
        if audio_meta is not None:
            pcm = decode_audio(audio_meta)
            wav_path = (output_path + ".wav") if output_path else (input_path + ".wav")
            sr = audio_meta["sample_rate"]
            ch = audio_meta["n_channels"]
            import wave
            with wave.open(wav_path, "wb") as wf:
                wf.setnchannels(ch)
                wf.setsampwidth(2)
                wf.setframerate(sr)
                wf.writeframes(pcm.tobytes())
            print(f"audio    : {wav_path}  {pcm.size} samples  {ch}ch @{sr}Hz")
    finally:
        f.close()

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
