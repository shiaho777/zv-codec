#!/usr/bin/env python3
import os, sys, ctypes, struct, subprocess, tempfile, shutil, time
import numpy as np

LIB = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), "libzv_audio.dylib"))
LIB.zv_audio_encode.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int64]
LIB.zv_audio_encode.restype = ctypes.c_int64
LIB.zv_audio_decode.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int64]
LIB.zv_audio_decode.restype = ctypes.c_int64

FFMPEG = shutil.which("ffmpeg") or "/opt/homebrew/bin/ffmpeg"

def extract_pcm(src):
                                               
    probe = subprocess.run(
        [FFMPEG, "-i", src, "-hide_banner"],
        capture_output=True, text=True
    )
    err = probe.stderr
    sr = 44100
    ch = 2
    for line in err.split("\n"):
        if "Stream" in line and "Audio:" in line:
                         
            for tok in line.split(","):
                tok = tok.strip()
                if " Hz" in tok:
                    try: sr = int(tok.split()[0])
                    except: pass
                if "stereo" in tok: ch = 2
                if "mono" in tok: ch = 1
            break
    raw_path = tempfile.mktemp(suffix=".raw")
    subprocess.run(
        [FFMPEG, "-y", "-hide_banner", "-loglevel", "error",
         "-i", src,
         "-f", "s16le", "-acodec", "pcm_s16le",
         "-ar", str(sr), "-ac", str(ch), raw_path],
        check=True
    )
    return raw_path, sr, ch

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else '录屏2026-05-02 16.31.14.mov'

    print(f"=== source: {src} ===")
    raw_path, sr, ch = extract_pcm(src)
    pcm_size = os.path.getsize(raw_path)
    n_frames = pcm_size // (ch * 2)
    duration = n_frames / sr
    print(f"PCM: {n_frames} frames × {ch} ch × 16-bit @ {sr}Hz, "
          f"{duration:.1f}s, {pcm_size:,} B raw")

    with open(raw_path, "rb") as f:
        raw = f.read()
    samples = np.frombuffer(raw, dtype=np.int16)
    if ch == 2:
        samples = samples.reshape(-1, 2)

    samples_flat = samples.flatten() if ch == 2 else samples
    samples_flat = np.ascontiguousarray(samples_flat, dtype=np.int16)
    out_buf = (ctypes.c_uint8 * (n_frames * ch * 4 + 4096))()
    t0 = time.time()
    n = LIB.zv_audio_encode(samples_flat.ctypes.data_as(ctypes.c_void_p),
                             n_frames, ch, out_buf, len(out_buf))
    t_enc = time.time() - t0
    zv_payload = bytes(out_buf[:n])
    print(f"ZV audio: {n:,} B  ({n/pcm_size:.3f} of raw) in {t_enc:.2f}s")

    dec_samples = np.zeros_like(samples_flat)
    in_buf = (ctypes.c_uint8 * len(zv_payload)).from_buffer_copy(zv_payload)
    t0 = time.time()
    LIB.zv_audio_decode(dec_samples.ctypes.data_as(ctypes.c_void_p),
                         n_frames, ch,
                         ctypes.cast(in_buf, ctypes.c_void_p), len(zv_payload))
    t_dec = time.time() - t0
    is_lossless = np.array_equal(samples_flat, dec_samples)
    if not is_lossless:
        diff = samples_flat.astype(np.int32) - dec_samples.astype(np.int32)
        print(f"NOT LOSSLESS: max diff = {np.max(np.abs(diff))}, "
              f"first mismatch at sample {np.argmax(np.abs(diff))}")
    else:
        print(f"lossless: YES (decode {t_dec:.2f}s)")

    flac_path = tempfile.mktemp(suffix=".flac")
    subprocess.run(
        [FFMPEG, "-y", "-hide_banner", "-loglevel", "error",
         "-f", "s16le", "-ar", str(sr), "-ac", str(ch), "-i", raw_path,
         "-c:a", "flac", "-compression_level", "8", flac_path],
        check=True
    )
    flac_size = os.path.getsize(flac_path)
    print(f"FLAC -8 : {flac_size:,} B  ({flac_size/pcm_size:.3f} of raw)")

    print()
    print(f"{'codec':<14}{'size (B)':>14}{'vs raw':>10}{'vs FLAC':>10}")
    print("-" * 50)
    print(f"{'raw PCM':<14}{pcm_size:>14,}{1.000:>10.3f}{pcm_size/flac_size:>10.2f}x")
    print(f"{'FLAC -8':<14}{flac_size:>14,}{flac_size/pcm_size:>10.3f}{1.000:>10.2f}x")
    delta = (n - flac_size) / flac_size * 100
    print(f"{'ZV audio':<14}{n:>14,}{n/pcm_size:>10.3f}{n/flac_size:>10.2f}x  ({'+' if delta>=0 else ''}{delta:.1f}% vs FLAC)")

    os.unlink(raw_path)
    os.unlink(flac_path)

if __name__ == "__main__":
    main()
