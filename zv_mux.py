#!/usr/bin/env python3
"""ZVM1 container: one ZV video blob plus optional ZA audio.

    ZVM1
    u32be video_len
    video bytes                 # a complete ZV file (magic ZV ...)
    u32be audio_len             # 0 = no audio
    if audio_len:
        u32be n_frames
        u16be n_channels
        u32be sample_rate
        audio_len bytes         # ZA payload from zv_audio_encode
"""
import os
import struct
import tempfile
import shutil
import subprocess
import ctypes
import numpy as np

MAGIC = b"ZVM1"


def pack(video_zv, audio_za=None,
         n_frames=0, n_channels=0, sample_rate=0):
    out = bytearray(MAGIC)
    out += struct.pack(">I", len(video_zv))
    out += video_zv
    if not audio_za:
        out += struct.pack(">I", 0)
        return bytes(out)
    out += struct.pack(">I", len(audio_za))
    out += struct.pack(">IHI", int(n_frames), int(n_channels), int(sample_rate))
    out += audio_za
    return bytes(out)


def unpack(blob: bytes):
    if len(blob) < 8 or blob[:4] != MAGIC:
        raise ValueError("not a ZVM1 container")
    vlen = struct.unpack(">I", blob[4:8])[0]
    off = 8
    if off + vlen + 4 > len(blob):
        raise ValueError("truncated ZVM1 video")
    video = blob[off:off + vlen]
    off += vlen
    alen = struct.unpack(">I", blob[off:off + 4])[0]
    off += 4
    if alen == 0:
        return video, None
    if off + 10 + alen > len(blob):
        raise ValueError("truncated ZVM1 audio")
    n_frames, n_ch, sr = struct.unpack(">IHI", blob[off:off + 10])
    off += 10
    audio = blob[off:off + alen]
    return video, {
        "payload": audio,
        "n_frames": n_frames,
        "n_channels": n_ch,
        "sample_rate": sr,
    }


def is_zvm(path: str) -> bool:
    try:
        with open(path, "rb") as f:
            return f.read(4) == MAGIC
    except OSError:
        return False


def open_video_file(path: str):
    """Return (binary_file, audio_meta_or_None). Caller closes the file."""
    with open(path, "rb") as f:
        head = f.read(4)
        rest = f.read()
    blob = head + rest
    if head == MAGIC:
        video, audio = unpack(blob)
        import io
        return io.BytesIO(video), audio
    import io
    return io.BytesIO(blob), None


def _audio_lib():
    here = os.path.dirname(os.path.abspath(__file__))
    for name in ("libzv_audio.dylib", "libzv_audio.so", "zv_audio.dll"):
        p = os.path.join(here, name)
        if os.path.exists(p):
            lib = ctypes.CDLL(p)
            lib.zv_audio_encode.argtypes = [
                ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                ctypes.c_void_p, ctypes.c_int64]
            lib.zv_audio_encode.restype = ctypes.c_int64
            lib.zv_audio_decode.argtypes = [
                ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                ctypes.c_void_p, ctypes.c_int64]
            lib.zv_audio_decode.restype = ctypes.c_int64
            return lib
    return None


def extract_pcm(src: str):
    ffmpeg = shutil.which("ffmpeg") or "/opt/homebrew/bin/ffmpeg"
    if not os.path.exists(ffmpeg):
        return None
    probe = subprocess.run(
        [ffmpeg, "-i", src, "-hide_banner"], capture_output=True, text=True)
    err = probe.stderr or ""
    if "Audio:" not in err:
        return None
    sr, ch = 44100, 2
    for line in err.split("\n"):
        if "Stream" in line and "Audio:" in line:
            for tok in line.split(","):
                tok = tok.strip()
                if " Hz" in tok:
                    try:
                        sr = int(tok.split()[0])
                    except ValueError:
                        pass
                if "stereo" in tok:
                    ch = 2
                if "mono" in tok:
                    ch = 1
            break
    raw_path = tempfile.mktemp(suffix=".s16le")
    r = subprocess.run(
        [ffmpeg, "-y", "-hide_banner", "-loglevel", "error",
         "-i", src, "-f", "s16le", "-acodec", "pcm_s16le",
         "-ar", str(sr), "-ac", str(ch), raw_path],
        capture_output=True)
    if r.returncode != 0 or not os.path.exists(raw_path):
        return None
    try:
        pcm = np.fromfile(raw_path, dtype=np.int16)
    finally:
        try:
            os.remove(raw_path)
        except OSError:
            pass
    if pcm.size < ch:
        return None
    n_frames = pcm.size // ch
    pcm = np.ascontiguousarray(pcm[: n_frames * ch])
    return pcm, n_frames, ch, sr


def encode_audio_from_video(src: str):
    """Return (za_bytes, n_frames, n_ch, sr) or None."""
    lib = _audio_lib()
    if lib is None:
        return None
    got = extract_pcm(src)
    if got is None:
        return None
    pcm, n_frames, ch, sr = got
    cap = n_frames * ch * 8 + 65536
    out = np.zeros(cap, dtype=np.uint8)
    n = lib.zv_audio_encode(
        pcm.ctypes.data, n_frames, ch, out.ctypes.data, cap)
    if n <= 0:
        return None
    return bytes(out[:n]), n_frames, ch, sr


def decode_audio(meta: dict) -> np.ndarray:
    lib = _audio_lib()
    if lib is None:
        raise RuntimeError("libzv_audio not found")
    n_frames = meta["n_frames"]
    n_ch = meta["n_channels"]
    rec = np.zeros(n_frames * n_ch, dtype=np.int16)
    payload = meta["payload"]
    buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
    lib.zv_audio_decode(
        rec.ctypes.data, n_frames, n_ch,
        ctypes.cast(buf, ctypes.c_void_p), len(payload))
    return rec
