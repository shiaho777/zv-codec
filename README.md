# ZV

[简体中文](README.zh-CN.md)

A small video and audio codec. No neural networks, no external dependencies.
Optional reversible Haar leaves (`OP_TX` / `OP_TXI`) sit beside the causal
predictor; they never ship on old streams.

v17 at eps=0 is **bit-exact 8-bit RGB** (reversible YCoCg-R, no 1/2-bit pre-quant).
v18 is the same payload with **10-bit RGB**. Near-lossless (eps>0) quantizes
residuals in that full-scale domain, so PSNR is no longer capped around 43 dB.
On screen / near-still content, lossless size usually beats x265 lossless RGB
and lands near JPEG XL / AV1 lossless. Audio is strictly lossless; ZA v2
(block LPC) tracks FLAC -8 on music. A `ZVM1` container wraps a ZV video
track plus optional ZA audio.

## What this is, and what it isn't

This is a hackable reference implementation of a probabilistic-coding video
pipeline. It is interesting on niche workloads (screen capture, UI demos,
near-still content) and useful as a teaching artifact: the whole encoder,
decoder, and audio codec fit on a screen at a time.

It is **not** a replacement for H.264 / H.265 / AV1. Those formats benefit from
decades of research, hardware decoders, and ecosystem support. ZV is a
hackable RGB lossless pipeline that happens to do well on screen content and,
on camera RGB, now beats FFV1 / x265 lossless RGB (AV1 lossless in YUV is
still smaller).

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
| FLAC -8 | 784 KB | 2.20× | strict |
| **ZV audio v2** | **782 KB** | **2.20×** | strict |

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

Each pixel is coded as its residual under a causal predictor shared by
encoder and decoder. 16×16 tiles pick a leaf op by rate (and, when
`eps>0`, rate+distortion). Most camera texture stays on `PREDNFX` (dense
residuals, one rANS model per neighborhood). Keyframes start on that fold
so the per-neighborhood models actually train; tiles that still want
left/up runs use `OP_PREDX` (PRED flags, folded residual). Smooth luma
squares may switch to a reversible Haar of the temporal residual
(`OP_TX`) or of the pixels minus a border DC (`OP_TXI`).

```
RGB → reversible YCoCg-R
    → 16×16 tiles, recursive SPLIT, per-tile op RDO
    → spatial / temporal / palette / Haar-residual leaves
    → rANS, frozen CDFs per segment, no tables in the stream
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

For full details, read `zv_core.c` and `zv_audio.c`.

## File layout

```
.zv  ::= header[12]  size_table[4 × frames]  frame_payloads...
header  ::= "ZV" version w[2] h[2] fps frames[4]
```

Two payload variants share the same outer header:

- v10 (single-threaded): one AC stream per frame covering all three planes.
- v11 (multi-stripe): per-plane, per-stripe independent AC streams, prefixed
  with a stripe count.
- v12 (single-threaded): like v10 but the entropy backend is rANS instead of
  binary arithmetic coding. Each plane stream starts with a flags byte
  (prior shape, interleaved-lane count) followed by independent rANS
  segments with adaptive per-context CDFs.
- v13 (multi-stripe): the stripe variant of v12.
- v14 (single-threaded): tile-program format. Each plane is a 16×16 tile
  grid encoded as two rANS streams: a "program" stream of per-tile
  opcodes+parameters and a "residual" stream of per-pixel records. Ops:
  `RUN` (copy tile from previous frame), `COPY` (temporal copy with a
  motion-vector offset, plus residuals), `ICOPY` (copy from the current
  frame's already-decoded region — screen-content block matching),
  `PAL` (palette of up to 16
  entries + index map), `PRED` (the full v12 causal predictor machinery),
  `NN` (per-plane learned linear predictor), `SPLIT` (recursive
  4-way subdivision into 8x8 children that each pick their own op),
  `MVRUN`/`IRUN` (skip modes: exact-match copies that ship only a motion
  vector, no residual stream), their predicted-MV variants
  `MVRUNP`/`COPYP`/`IRUNP` (MV taken from the last same-type operation -
  zero parameter bits), `RPT` (repeat the previous leaf op over a run of
  tiles - collapses uniform scroll regions), `RAW` (literal fallback).
  `OP_QS` (a stream directive - consumes no tile - that switches the
  residual quantization step for subsequent tiles), and `OP_NOISE`
  (procedural residual synthesis: the tile's causal predictor runs as
  usual, but the residual is generated from a deterministic PRNG whose
  sigma is the only transmitted parameter - statistically equivalent
  noise at a fraction of the cost).
  Op symbols are coded under order-1 context models (previous leaf op).
  Motion candidates span +-128 px plus wrap-around companions, with
  unit-step diamond refinement; a decimated dominant-motion probe seeds
  every tile with the global scroll vector. The encoder evaluates
  candidate ops per tile
  under the live adaptive models and commits the cheapest one. A plane
  whose tiles are all `RUN` collapses to a 4-byte record. Optionally, the
  program header's top bit flags a per-plane learned linear predictor:
  nine Q8 weights over causal features `[a,b,c,d2,t,median,a+b-c,luma,1]`
  are fitted by least squares at encode time, carried as the first
  program records, and applied as `clip((sum w_i f_i + 128) >> 8)` for
  tiles coded with the `NN` op. The `luma` feature lets chroma planes
  predict from the already-decoded co-located luma (CfL-style). The
  encoder encodes each plane both ways and keeps the smaller bitstream,
  so weights ship only when the learned predictor pays for itself.

  Near-lossless mode (`encoder.py in.mp4 out.zv <eps> [eps_hi]`): program
  header bit 30 marks a quantized plane; the base step and a noise seed
  ride as the first program records. Residuals are quantized as
  `round(r/qs)` and reconstructed as `pred + rq*qs`, so the per-pixel
  error stays within `2*eps_hi+1` in YCoCg space - including `OP_QS`
  tiles (bounded by their own step) and `OP_NOISE` tiles (the encoder
  only emits synthesized noise when every pixel of the tile lands within
  the bound). The encoder maintains a decoder-equivalent reconstruction
  plane: causal neighbors, inter-frame references and chroma CfL all
  read reconstructed samples, so the two sides stay bit-identical.
  Exact ops (`RUN`/`MVRUN`/`PAL`/`RAW`/`RPT`) are unaffected.
  Lossy chroma starts at `2*qs` (capped by `qs_hi`); per-tile `OP_QS`
  may coarsen further, but a lossy plane never re-opens `qs=1` (that
  made high-eps files larger than mid-eps). Screen content still has a
  frame-level lossless sibling when it is strictly smaller.
  `OP_TX` / `OP_TXI` (reversible 4/8/16 Haar, luma only, CFL_MDL
  coefficients) compete with residual leaves in the 0.25–4 bpp band.
- v15 (multi-stripe): the stripe variant of v14.
- v16 (stateful): v14 plus per-plane persistent decoder state carried
  across frames — leaf-descriptor tables (enabling `PSAME`/`VRPT`
  replay of previous-frame and above-row tiles), learned predictor
  weights, predictor-selection state, and the program/residual entropy
  models themselves. Streams are flagged `PHDR_STATEFUL` per plane.
  Model carry can hurt when the content distribution shifts abruptly,
  so each stateful lossy plane is encoded twice — warm (carried
  models) and cold (fresh seed) — and the smaller stream wins; a
  `PHDR_FRESH` header bit tells the decoder which initialization to
  use, while the winning post-stream model state carries forward
  either way. `encoder.py` now emits v17 (`zv_encode_frame_v3sq` plus
  reversible YCoCg-R); `zv_decode_frame_v3s` decodes it. v10–v16 remain
  readable.

`encoder_mt.py` and the studio GUI emit v17 too; stripe count is
`zv_encode_set_stripes` (default 1 for size, 3+ for encode speed).

The audio codec prefixes `ZA` plus a version byte, then an AC payload.
v2 (`0x02`) codes 4096-sample blocks with FLAC-style fixed predictors or
Q10 LPC (Levinson–Durbin, Hann analysis window), per-block wasted bits,
Rice-coded residual partitions, and per-block mid-side / left-side
stereo. v1 streams (`0x01`, global wasted bits + 6 predictors) and
legacy headerless streams still decode. The caller still passes frame
and channel counts.

## Suitable workloads

- Screen recordings, lecture videos, code demos
- UI animations and application capture
- Anything close to a still or near-still scene

On camera RGB lossless the picture is mixed. 16 frames of 1280×720 live
action (office, hair, fabric, wood), eps=0, bit-exact RGB:

| Codec | Output | bpp | Notes |
|-------|--------|-----|-------|
| **ZV v17** | **3.53 MB** | **1.917** | ~8.9 s encode, ~22 fps decode, RGB identity |
| FFV1 gbrp | 4.21 MB | 2.28 | strict RGB |
| x265 lossless gbrp | 7.60 MB | 4.12 | strict RGB |
| AV1 lossless yuv444 | 2.84 MB | 1.54 | YUV, not RGB identity |

Same clip, 8 frames, near-lossless (Apple M4 Pro). Rate is monotonic in `eps`:

| Setting | Size | bpp | RGB PSNR | max\|dRGB\| |
|---------|------|-----|----------|-------------|
| eps=0 | 1.85 MB | 2.010 | lossless | 0 |
| eps=1/4 | 339 KB | 0.368 | 40.11 dB | 13 |
| eps=4/16 | 83 KB | 0.090 | 30.06 dB | 42 |
| eps=8/32 | 40 KB | 0.044 | 25.03 dB | 74 |

Grain-heavy close-ups stay expensive for every lossless RGB codec (~12 bpp
here). AV1 lossless YUV444 is still ~1.25× smaller than ZV RGB. Hardware
ASIC decode is out of scope; the software decoder uses NEON for YCoCg-R,
Haar, plane de/interleave, and in-bounds MV copies. The hot path is still
serial rANS + `PREDNFX`.

## Limitations

- Video is 8-bit RGB (v17) or 10-bit RGB (v18); audio is 16-bit PCM.
- v17/v18 eps=0 is strict RGB lossless; eps>0 is bounded residual quant. v16
  and older max out around ±5 RGB.
- Audio is strictly lossless. ZA v2 matches FLAC -8 on the 10 s music clip
  above; some other material may still trail.
- `ZVM1` muxes one ZV video track and optional ZA audio (`encoder.py` writes
  it when the output name ends in `.zvm`).
- Perceptual (lossy) RD still trails AV1 / HEVC at the same bpp; the
  ZV curve above is now monotonic and usable. Hardware ASIC decode does
  not exist. RGB lossless on camera footage beats FFV1 and x265 lossless
  RGB; AV1 lossless in YUV444 is still smaller.

## License

MIT, see [LICENSE](LICENSE).
