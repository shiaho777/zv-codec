#!/usr/bin/env python3
"""Lossless round-trip + FLAC size check for zv_audio v2 (block LPC)."""
import os, sys, ctypes, tempfile, subprocess, shutil
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
candidates = [
    os.path.join(HERE, "libzv_audio.dylib"),
    "/tmp/zvbench/libzv_audio.dylib",
]
lib_path = next((p for p in candidates if os.path.exists(p)), None)
if lib_path is None:
    raise SystemExit("libzv_audio.dylib not found")

LIB = ctypes.CDLL(lib_path)
LIB.zv_audio_encode.argtypes = [
    ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int64]
LIB.zv_audio_encode.restype = ctypes.c_int64
LIB.zv_audio_decode.argtypes = [
    ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int64]
LIB.zv_audio_decode.restype = ctypes.c_int64

FFMPEG = shutil.which("ffmpeg") or "/opt/homebrew/bin/ffmpeg"
FLAC = shutil.which("flac") or "/opt/homebrew/bin/flac"


def roundtrip(samples, n_ch):
    n_frames = samples.size // n_ch
    cap = n_frames * n_ch * 8 + 65536
    out = np.zeros(cap, dtype=np.uint8)
    n = LIB.zv_audio_encode(samples.ctypes.data, n_frames, n_ch, out.ctypes.data, cap)
    assert n > 0, n
    rec = np.zeros_like(samples)
    LIB.zv_audio_decode(rec.ctypes.data, n_frames, n_ch, out.ctypes.data, n)
    assert np.array_equal(rec, samples), "audio not lossless"
    return int(n)


def main():
    rng = np.random.default_rng(0)
    n = 44100
    t = np.linspace(0, 1, n, endpoint=False)
    sine = (0.4 * np.sin(2 * np.pi * 440 * t) * 32767).astype(np.int16)
    silence = np.zeros(n, dtype=np.int16)
    noise = rng.integers(-32768, 32767, n, dtype=np.int16)
    even = (sine.astype(np.int32) & ~7).astype(np.int16)  # wasted bits

    s1 = roundtrip(sine.copy(), 1)
    s0 = roundtrip(silence.copy(), 1)
    sn = roundtrip(noise.copy(), 1)
    se = roundtrip(even.copy(), 1)
    stereo = np.empty(n * 2, dtype=np.int16)
    stereo[0::2] = sine
    stereo[1::2] = (0.4 * np.sin(2 * np.pi * 660 * t) * 32767).astype(np.int16)
    ss = roundtrip(stereo, 2)
    print(f"sine     {s1:8d} B")
    print(f"silence  {s0:8d} B")
    print(f"noise    {sn:8d} B")
    print(f"even     {se:8d} B  (wasted-bit)")
    print(f"stereo   {ss:8d} B")

    from zv_mux import pack, unpack, decode_audio
    n_frames = stereo.size // 2
    cap = n_frames * 2 * 8 + 65536
    out = np.zeros(cap, dtype=np.uint8)
    n = LIB.zv_audio_encode(stereo.ctypes.data, n_frames, 2, out.ctypes.data, cap)
    blob = pack(b"ZV\x11" + b"\x00" * 16, bytes(out[:n]), n_frames, 2, 44100)
    vid, meta = unpack(blob)
    rec = decode_audio(meta)
    assert np.array_equal(rec, stereo)
    print(f"zvm1     {len(blob):8d} B  audio mux ok")

    if os.path.exists("/tmp/zvbench/audio.pcm") and os.path.exists(FLAC):
        raw = np.fromfile("/tmp/zvbench/audio.pcm", dtype=np.int16)
        n_frames = raw.size // 2
        zv_n = roundtrip(np.ascontiguousarray(raw), 2)
        wav = "/tmp/zvbench/audio_rt.wav"
        flacp = "/tmp/zvbench/audio_rt.flac"
        subprocess.run(
            [FFMPEG, "-y", "-hide_banner", "-loglevel", "error",
             "-f", "s16le", "-ar", "44100", "-ac", "2", "-i", "/tmp/zvbench/audio.pcm",
             wav], check=True)
        subprocess.run([FLAC, "-8", "-f", "-o", flacp, wav], check=True)
        fl = os.path.getsize(flacp)
        print(f"twitter  ZV {zv_n:,}  FLAC {fl:,}  ZV/FLAC {zv_n/fl:.3f}")
    print("AUDIO ROUND-TRIPS OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
