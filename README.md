# ZV

[简体中文](README.zh-CN.md)

A small lossless-ish video and audio codec, written in about 1100 lines of C.
No neural networks, no transforms, no external dependencies.

On screen-recording content, ZV compresses **24% smaller than x265 strict-lossless RGB**
at the same encode speed, while keeping per-pixel error within ±5 RGB (well below
the human just-noticeable-difference threshold). The audio codec matches FLAC
within 3% on lossless mode at less than 1/250 of FLAC's source size.

## What this is, and what it isn't

This is a hackable reference implementation of a probabilistic-coding video
pipeline. It is interesting on niche workloads (screen capture, UI demos,
near-still content) and useful as a teaching artifact: the whole encoder,
decoder, and audio codec fit on a screen at a time.

It is **not** a replacement for H.264 / H.265 / AV1. Those formats benefit from
decades of research, hardware decoders, and ecosystem support, and they handle
natural video much better than ZV does. ZV does not aspire to compete with them
in the general case; it just happens to do well on a narrow class of content.

## Performance

Video, screen recording 3456×2234, 146 frames, 3.38 GB raw:

| Codec | Output | Ratio | Encode time | Lossless |
|-------|--------|-------|-------------|----------|
| HuffYUV | 934 MB | 3.6× | 1.2 s | strict |
| FFV1 | 123 MB | 27.5× | 4.4 s | strict |
| x265 lossless RGB | 11.0 MB | 307× | 8.0 s | strict |
| **ZV (single-threaded)** | **8.39 MB** | **403×** | **5.6 s** | ±5 RGB |
| **ZV (8 stripes)** | **8.66 MB** | 391× | **2.3 s** | ±5 RGB |
| x264 "lossless" | 4.5 MB | 743× | 2.9 s | ±2 (YUV420) |

Audio, 10 s stereo @ 44100 Hz, 1.76 MB raw:

| Codec | Output | Ratio | Lossless |
|-------|--------|-------|----------|
| FLAC -8 | 1409 KB | 1.25× | strict |
| **ZV audio** | **1447 KB** | 1.22× | strict |

## Build and run

Compile the two shared libraries (macOS / Linux):

```sh
clang -O3 -march=native -shared -fPIC -o libzv_core.dylib  zv_core.c
clang -O3 -march=native -shared -fPIC -o libzv_audio.dylib zv_audio.c
```

Install Python deps. Use a virtual environment so you don't fight your
system package manager:

```sh
python3 -m venv .venv
.venv/bin/pip install numpy opencv-python pillow
```

macOS: the system Python's bundled Tk does not work on macOS 16. Install
Homebrew Python and Tk first:

```sh
brew install python@3.12 python-tk@3.12
```

Linux: `sudo apt install python3-tk` (or `dnf install python3-tkinter`).

Windows: install [Python 3](https://www.python.org/downloads/windows/),
which already bundles Tk.

Then:

```sh
.venv/bin/python zv_studio.py             # GUI: play .zv, convert any video to .zv
.venv/bin/python encoder.py    in.mp4 out.zv
.venv/bin/python decoder.py    out.zv decoded.mp4 --no-play
.venv/bin/python encoder_mt.py in.mp4 out.zv [num_stripes=8]   # multi-threaded
.venv/bin/python decoder_mt.py out.zv decoded.mp4 --no-play
.venv/bin/python audio_test.py in.wav     # audio codec + FLAC comparison
.venv/bin/python test_roundtrip.py
.venv/bin/python bench.py      in.mp4
```

## How it works

Each pixel or audio sample is coded as its conditional entropy under a
causal context shared between encoder and decoder. There is no transform,
no rate-distortion search, and no signaling for predictor mode or block
partition.

```
RGB → YCoCg-R → 1-bit Y / 2-bit chroma quantization
              → 6 spatial predictors + 1 temporal predictor
              → 405 contexts (local gradient class)
              → per-context arg-min mode selection
              → Rice(k) + binary arithmetic coding
```

Audio uses the same shape with 3 predictors and 8 contexts, plus mid-side
decorrelation for stereo.

The multi-threaded encoder splits each frame into N horizontal stripes
(default 8, multiple of 16 rows). Each (stripe, channel) pair is an
independent task; `ctypes` calls release the GIL, so multi-core speedup is
real.

| Stripes | Size | Time | Speedup |
|---------|------|------|---------|
| 1 | 8.39 MB | 5.6 s | 1.0× |
| 4 | 8.55 MB | 2.7 s | 2.1× |
| **8** | **8.66 MB** | **2.3 s** | **2.5×** |
| 16 | 8.81 MB | 2.7 s | 2.1× |

For full details, read `zv_core.c` (~700 lines) and `zv_audio.c` (~400 lines).

## File layout

```
.zv  ::= header[12]  size_table[4 × frames]  frame_payloads...
header  ::= "ZV" version w[2] h[2] fps frames[4]
```

Two payload variants share the same outer header:

- v10 (single-threaded): one AC stream per frame covering all three planes.
- v11 (multi-stripe): per-plane, per-stripe independent AC streams, prefixed
  with a stripe count. Both `zv_studio.py` and the multi-threaded driver
  produce v11 by default; v10 is kept for the simplest single-threaded path.

The audio codec has no outer header; the caller passes frame and channel
counts through an external container.

## Suitable workloads

- Screen recordings, lecture videos, code demos
- UI animations and application capture
- Anything close to a still or near-still scene

The advantage narrows on natural video; on movies and camera footage,
H.265 and AV1 will produce smaller files at higher visual quality.

## Limitations

- Video accepts 8-bit RGB only; audio accepts 16-bit PCM only.
- Video is perceptually lossless (max ±5 RGB), not strictly lossless.
- Audio is strictly lossless.
- No container format yet — video and audio must be muxed externally if you
  want a single file.

## License

MIT, see [LICENSE](LICENSE).
