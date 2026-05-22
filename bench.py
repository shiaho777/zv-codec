#!/usr/bin/env python3
import os, sys, time, subprocess, struct, ctypes, shutil, tempfile
import numpy as np
import cv2

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from encoder import _LIB, rgb_to_ycocgr
from decoder import ycocgr_to_rgb

_LIB.zv_encode_frame.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32,
]
_LIB.zv_encode_frame.restype = ctypes.c_int32
_LIB.zv_decode_frame.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_int, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int32,
]
_LIB.zv_decode_frame.restype = ctypes.c_int32

FFMPEG = shutil.which("ffmpeg") or "/opt/homebrew/bin/ffmpeg"

def probe_and_dump_raw(src, raw_path):
    cap = cv2.VideoCapture(src)
    if not cap.isOpened():
        raise RuntimeError(f"cannot open {src}")
    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = int(cap.get(cv2.CAP_PROP_FPS)) or 24
    fc = 0
    with open(raw_path, "wb") as f:
        while True:
            ok, frm = cap.read()
            if not ok: break
            rgb = cv2.cvtColor(frm, cv2.COLOR_BGR2RGB)
            f.write(rgb.tobytes())
            fc += 1
    cap.release()
    return w, h, fps, fc

def read_frame_from_raw(f, w, h):
    need = w * h * 3
    buf = f.read(need)
    if len(buf) < need:
        return None
    return np.frombuffer(buf, dtype=np.uint8).reshape(h, w, 3)

def zv_encode_stream(raw_path, zv_path, w, h, fc, fps):
    max_frame_bytes = 8 * w * h + 4096
    out_buf = (ctypes.c_uint8 * max_frame_bytes)()
    prev_yc = None
    sizes = []
    t0 = time.time()
    with open(raw_path, "rb") as fin, open(zv_path, "wb") as fout:
                       
        fout.write(b"ZV" + bytes([10]) + struct.pack(">HH", w, h)
                   + bytes([fps & 0xFF]) + struct.pack(">I", fc))
                                                                      
        size_table_off = fout.tell()
        fout.write(b"\x00" * (fc * 4))

        for fi in range(fc):
            rgb = read_frame_from_raw(fin, w, h)
            if rgb is None:
                raise RuntimeError(f"unexpected EOF at frame {fi}")
            cur_yc = rgb_to_ycocgr(rgb)
            del rgb
            prev_ptr = prev_yc.ctypes.data_as(ctypes.c_void_p) if prev_yc is not None else None
            n = _LIB.zv_encode_frame(
                prev_ptr, cur_yc.ctypes.data_as(ctypes.c_void_p),
                w, h, out_buf, max_frame_bytes,
            )
            fout.write(bytes(out_buf[:n]))
            sizes.append(n)
            prev_yc = cur_yc

        fout.seek(size_table_off)
        for sz in sizes:
            fout.write(struct.pack(">I", sz))
    return time.time() - t0, sum(sizes)

def zv_decode_and_verify(zv_path, raw_path, w, h, fc):
    curr_yc = np.zeros((h, w, 3), dtype=np.int16)
    prev_yc = None
    t0 = time.time()
    mismatches = 0
    first_bad = None
    with open(zv_path, "rb") as fzv, open(raw_path, "rb") as fraw:
        assert fzv.read(3) == b"ZV\x0a"
        fzv.read(2 + 2 + 1 + 4)              
        sizes = [struct.unpack(">I", fzv.read(4))[0] for _ in range(fc)]

        for fi in range(fc):
            buf = fzv.read(sizes[fi])
            in_buf = (ctypes.c_uint8 * len(buf)).from_buffer_copy(buf)
            del buf
            prev_ptr = prev_yc.ctypes.data_as(ctypes.c_void_p) if prev_yc is not None else None
            _LIB.zv_decode_frame(
                prev_ptr, curr_yc.ctypes.data_as(ctypes.c_void_p),
                w, h, ctypes.cast(in_buf, ctypes.c_void_p), len(in_buf),
            )
            dec_rgb = ycocgr_to_rgb(curr_yc)
            orig_rgb = read_frame_from_raw(fraw, w, h)
            if not np.array_equal(dec_rgb, orig_rgb):
                mismatches += 1
                if first_bad is None:
                    first_bad = fi
            del dec_rgb, orig_rgb
            if prev_yc is None:
                prev_yc = curr_yc.copy()
            else:
                np.copyto(prev_yc, curr_yc)
    dec_time = time.time() - t0
    return dec_time, mismatches, first_bad

def ffmpeg_encode_from_rawfile(raw_path, w, h, fps, args, out_path):
    cmd = [FFMPEG, "-y", "-hide_banner", "-loglevel", "error",
           "-f", "rawvideo", "-pix_fmt", "rgb24",
           "-s", f"{w}x{h}", "-r", str(fps),
           "-i", raw_path] + args + [out_path]
    t0 = time.time()
    r = subprocess.run(cmd, capture_output=True)
    dt = time.time() - t0
    if r.returncode != 0:
        return None, dt, r.stderr.decode("utf-8", errors="replace")[-400:]
    return os.path.getsize(out_path), dt, None

def ffmpeg_decode_and_compare(codec_out_path, raw_path, w, h, fc):
    cmd = [FFMPEG, "-y", "-hide_banner", "-loglevel", "error",
           "-i", codec_out_path, "-f", "rawvideo", "-pix_fmt", "rgb24",
           "-fflags", "+bitexact", "-"]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    frame_bytes = w * h * 3
    max_diff = 0
    first_bad = None
    with open(raw_path, "rb") as fraw:
        for fi in range(fc):
            a = proc.stdout.read(frame_bytes)
            b = fraw.read(frame_bytes)
            if len(a) < frame_bytes or len(b) < frame_bytes:
                proc.kill()
                return False, fi, -1
            if a != b:
                if first_bad is None:
                    first_bad = fi
                                                         
                if max_diff == 0:
                    aa = np.frombuffer(a, dtype=np.uint8)
                    bb = np.frombuffer(b, dtype=np.uint8)
                    max_diff = int(np.max(np.abs(aa.astype(np.int16) - bb.astype(np.int16))))
    proc.stdout.close()
    proc.wait()
    return first_bad is None, first_bad, max_diff

CODECS = [
    ("ZV v10",             None),                  
    ("FFV1",               ["-c:v", "ffv1", "-level", "3", "-coder", "1",
                            "-context", "1", "-g", "1", "-slices", "24",
                            "-slicecrc", "1"]),
    ("HuffYUV",            ["-c:v", "huffyuv"]),
    ("x265 lossless RGB",  ["-c:v", "libx265", "-pix_fmt", "gbrp",
                            "-x265-params", "lossless=1:log-level=error",
                            "-preset", "slow"]),
    ("x264 lossless RGB",  ["-c:v", "libx264", "-pix_fmt", "gbrp",
                            "-qp", "0", "-preset", "slow"]),
]
EXT = {
    "FFV1":              "mkv",
    "HuffYUV":           "avi",
    "x265 lossless RGB": "mkv",
    "x264 lossless RGB": "mkv",
}

def main():
    argv = sys.argv[1:]
    if not argv:
        print("usage: python bench.py <source_video> [--keep] [--only a,b]")
        sys.exit(1)
    src = argv[0]
    keep = "--keep" in argv
    only = None
    for a in argv:
        if a.startswith("--only="):
            only = set(a.split("=", 1)[1].split(","))

    print(f"=== source: {src} ===")
    raw_path = os.path.join(tempfile.gettempdir(), "bench_raw.rgb")
    t0 = time.time()
    w, h, fps, fc = probe_and_dump_raw(src, raw_path)
    dump_t = time.time() - t0
    raw_bytes = w * h * 3 * fc
    src_size = os.path.getsize(src)
    print(f"  {w}x{h} @ {fps}fps, {fc} frames (dump {dump_t:.1f}s)")
    print(f"  raw RGB total   : {raw_bytes:>14,} B  ({raw_bytes/1e9:.3f} GB)")
    print(f"  source file     : {src_size:>14,} B")
    print(f"  raw file on disk: {raw_path}")
    print()

    results = []                                                  

    if only is None or "zv" in only:
        print("[ZV] encoding ...", flush=True)
        zv_path = os.path.join(tempfile.gettempdir(), "bench.zv")
        enc_t, zv_size = zv_encode_stream(raw_path, zv_path, w, h, fc, fps)
        print("[ZV] decoding+verifying ...", flush=True)
        dec_t, mism, first_bad = zv_decode_and_verify(zv_path, raw_path, w, h, fc)
        is_lossless = (mism == 0)
        extra = f"dec {fc/dec_t:.0f} fps"
        results.append(("ZV v10", zv_size, enc_t, is_lossless, 0 if is_lossless else -1, extra))
        if not keep:
            try: os.unlink(zv_path)
            except OSError: pass
        print(f"[ZV] done: {zv_size:,} B, enc {enc_t:.2f}s, dec {dec_t:.2f}s, "
              f"lossless={is_lossless}", flush=True)

    for name, args in CODECS[1:]:
        if only is not None and name.split()[0].lower() not in only:
            continue
        print(f"[{name}] encoding ...", flush=True)
        out_path = os.path.join(tempfile.gettempdir(), f"bench.{name.replace(' ','_')}.{EXT[name]}")
        sz, t, err = ffmpeg_encode_from_rawfile(raw_path, w, h, fps, args, out_path)
        if sz is None:
            results.append((name, None, t, False, -1, (err or "")[-60:]))
            print(f"[{name}] FAIL", flush=True)
            continue
        print(f"[{name}] verifying ...", flush=True)
        ok, first_bad, md = ffmpeg_decode_and_compare(out_path, raw_path, w, h, fc)
        results.append((name, sz, t, ok, md, ""))
        if not keep:
            try: os.unlink(out_path)
            except OSError: pass
        print(f"[{name}] done: {sz:,} B, enc {t:.2f}s, lossless={ok}", flush=True)

    if not keep:
        try: os.unlink(raw_path)
        except OSError: pass

    print()
    print("=" * 88)
    print(f"{'codec':<22}{'size (B)':>15}{'ratio':>9}{'bpp':>8}{'enc(s)':>9}  {'lossless?':<14}{'vs ZV':<10}")
    print("-" * 88)
    zv_ref = next((r[1] for r in results if r[0].startswith("ZV")), None)
    for name, size, t, lossless, md, note in results:
        if size is None:
            print(f"{name:<22}{'FAIL':>15}{'':>9}{'':>8}{t:>9.2f}  {'-':<14}{note[-10:]:<10}")
            continue
        ratio = raw_bytes / size
        bpp = size * 8 / (fc * w * h)
        status = "YES" if lossless else f"NO(d<={md})"
        delta = ""
        if zv_ref is not None and name != "ZV v10":
            p = (size - zv_ref) / zv_ref * 100
            delta = f"{'+' if p>=0 else ''}{p:.0f}%"
        print(f"{name:<22}{size:>15,}{ratio:>8.1f}x{bpp:>8.3f}{t:>9.2f}  {status:<14}{delta:<10}")
    print("=" * 88)

if __name__ == "__main__":
    main()
