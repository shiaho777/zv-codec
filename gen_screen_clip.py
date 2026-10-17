#!/usr/bin/env python3
"""Generate a realistic screen-recording test clip.

Renders a dark code-editor window (line-number gutter, syntax-colored
tokens, status bar, terminal panel, blinking cursor), scrolls through a
long document, scrolls terminal output, then drags the whole window.
Output is piped as rawvideo to ffmpeg -> lossless h264 mp4.

usage: python3 gen_screen_clip.py [out.mp4] [w] [h] [frames]
"""
import os, sys, subprocess, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H = 1280, 720
N_FRAMES = 200
OUT = "/tmp/screen_clip.mp4"
FONT_PATH = "/System/Library/Fonts/Menlo.ttc"
FS = 13            # code font size
LH = 18            # line height
GUTTER = 64        # gutter width
TERM_H = 150       # terminal panel height
STATUS_H = 22

BG     = (30, 32, 36)
GUT_BG = (24, 25, 28)
GUT_FG = (105, 110, 120)
STATUS = (30, 90, 170)
TERM_BG = (18, 18, 20)
COLORS = dict(
    plain=(212, 216, 222), kw=(198, 120, 221), st=(152, 195, 121),
    num=(209, 154, 102), com=(110, 116, 126), fn=(97, 175, 239),
    ty=(229, 192, 123), op=(86, 182, 194), sel=(50, 60, 84),
)
KWS = ("static int ", "void ", "return ", "if ", "else ", "for ",
       "while ", "const ", "typedef ", "struct ", "case ", "break;")
IDS = "alpha beta ctx tile plane resid rans model pred stride width".split()


def gen_code_line(rng, font):
    """Return list of (text, color) spans forming a plausible code line."""
    spans, r = [], rng.random()
    ind = " " * rng.choice([0, 0, 4, 4, 8, 12, 16])
    if ind:
        spans.append((ind, COLORS["plain"]))
    if r < 0.08:
        spans.append(("// " + " ".join(rng.choice(IDS) for _ in range(rng.randint(2, 7))),
                      COLORS["com"]))
    elif r < 0.16:
        spans.append(("#include <" + rng.choice(IDS) + ".h>", COLORS["st"]))
    elif r < 0.22:
        spans.append(("", COLORS["plain"]))
    else:
        t = rng.random()
        if t < 0.45:
            spans.append((rng.choice(KWS), COLORS["kw"]))
        elif t < 0.6:
            spans.append((rng.choice(IDS) + "_t ", COLORS["ty"]))
        body = ""
        for _ in range(rng.randint(2, 8)):
            tt = rng.random()
            if tt < 0.15:
                body += rng.choice(["==", "!=", "<=", ">>", "&", "|", "->", " = "])
            elif tt < 0.3:
                body += str(rng.randint(0, 4095))
            elif tt < 0.42:
                body += '"' + rng.choice(IDS) + '"'
            else:
                body += rng.choice(IDS)
            body += rng.choice(["", "(", ")", ",", "; ", " "])
        spans.append((body, COLORS["plain"]))
        if rng.random() < 0.35:
            spans.append(("  // note", COLORS["com"]))
    return spans


def build_document(rng, font, doc_w, n_lines):
    """Render the full scrollable code document once."""
    doc = Image.new("RGB", (doc_w, n_lines * LH), BG)
    d = ImageDraw.Draw(doc)
    for i in range(n_lines):
        y = i * LH
        d.text((10, y + 1), str(i + 1).rjust(4), font=font, fill=GUT_FG)
        x = GUTTER + 8
        for txt, col in gen_code_line(rng, font):
            d.text((x, y + 1), txt, font=font, fill=col)
            x += d.textlength(txt, font=font)
    return doc


def build_terminal(rng, font, w, n_lines):
    term = Image.new("RGB", (w, n_lines * LH), TERM_BG)
    d = ImageDraw.Draw(term)
    prompts = ["$ clang -O3 ", "> building ", "[ok] ", "warn: ", "$ ./run "]
    for i in range(n_lines):
        txt = rng.choice(prompts) + " ".join(
            rng.choice(IDS + ["zv_core.c", "plane.o", "-lm", "16:16"])
            for _ in range(rng.randint(1, 6)))
        col = COLORS["st"] if txt.startswith("[ok]") else (200, 200, 200)
        d.text((8, i * LH + 1), txt, font=font, fill=col)
    return term


def draw_window_titlebar(d, x0, y0, w):
    d.rectangle([x0, y0, x0 + w, y0 + 26], fill=(40, 41, 45))
    for i, c in enumerate(((255, 95, 86), (255, 189, 46), (39, 201, 63))):
        cx = x0 + 16 + i * 18
        d.ellipse([cx, y0 + 9, cx + 10, y0 + 19], fill=c)
    d.text((x0 + w // 2 - 60, y0 + 6), "zv_core.c - Editor", fill=(180, 180, 185))


def gradient_wallpaper(w, h):
    """macOS-style vertical gradient wallpaper with a soft glow."""
    top = np.array([80, 60, 130]); bot = np.array([20, 30, 70])
    t = np.linspace(0, 1, h)[:, None, None]
    img = np.broadcast_to(top * (1 - t) + bot * t, (h, w, 3)).astype(np.float32).copy()
    yy, xx = np.mgrid[0:h, 0:w]
    glow = np.exp(-(((xx - w * 0.7) ** 2 + (yy - h * 0.3) ** 2) / (w * 0.35) ** 2))
    img += glow[..., None] * np.array([40, 30, 60])
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def make_photo_patch(rng, pw, ph):
    """Noise + smooth blobs: a camera-thumbnail-ish region."""
    yy, xx = np.mgrid[0:ph, 0:pw]
    base = np.stack([np.sin(xx / 9) * np.cos(yy / 7),
                     np.sin(xx / 13 + yy / 11),
                     np.cos((xx + yy) / 12)], -1) * 60 + 128
    noise = np.random.default_rng(rng.randint(0, 9999)).normal(0, 14, (ph, pw, 3))
    return np.clip(base + noise, 0, 255).astype(np.uint8)


def shadow_blit(frame, win, x0, y0, rad=8, alpha=90):
    """Rounded-rect drop shadow under the window."""
    sh = Image.new("L", frame.size, 0)
    sd = ImageDraw.Draw(sh)
    sd.rectangle([x0 + rad, y0 + rad, x0 + win.width + rad,
                  y0 + win.height + rad], fill=alpha)
    sh = sh.filter(__import__("PIL.ImageFilter", fromlist=["GaussianBlur"]).GaussianBlur(rad // 2))
    black = Image.new("RGB", frame.size, (0, 0, 0))
    frame.paste(black, (0, 0), sh)


def main():
    out = OUT
    w, h, n = W, H, N_FRAMES
    a = sys.argv[1:]
    if a: out = a[0]
    if len(a) > 1: w = int(a[1])
    if len(a) > 2: h = int(a[2])
    if len(a) > 3: n = int(a[3])

    rng = random.Random(7)
    font = ImageFont.truetype(FONT_PATH, FS)
    rng_doc = random.Random(1234)
    doc_lines = (h // LH) * 6
    print("rendering document ...", flush=True)
    doc = build_document(rng_doc, font, w - 40, doc_lines)
    term = build_terminal(rng_doc, font, w - 40, 40)
    zwall = os.environ.get("ZWALL", "1") != "0"
    znoise = os.environ.get("ZNOISE", "1") != "0"
    zphoto = os.environ.get("ZPHOTO", "1") != "0"
    wallpaper = gradient_wallpaper(w, h) if zwall else \
        Image.new("RGB", (w, h), (18, 18, 20))
    photo = make_photo_patch(rng, 220, 150) if zphoto else None
    body_h = h - 26 - STATUS_H - TERM_H
    grain = np.random.default_rng(42)

    ffmpeg = subprocess.Popen(
        ["ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
         "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{w}x{h}",
         "-r", "30", "-i", "-",
         "-c:v", "libx264", "-qp", "0", "-preset", "fast", out],
        stdin=subprocess.PIPE)

    sel_line = rng.randint(4, doc_lines - 4)
    for fi in range(n):
        scroll = int(fi * 2.4)                 # editor smooth scroll
        tscroll = int(fi * 1.1)                # terminal scroll
        frame = wallpaper.copy()
        d = ImageDraw.Draw(frame)

        # window drag in the last quarter
        wx = wy = 0
        if fi > n * 3 // 4:
            k = fi - n * 3 // 4
            wx, wy = k * 2, k // 2

        win = Image.new("RGB", (w - 40, h), BG)
        wd = ImageDraw.Draw(win)
        draw_window_titlebar(wd, 0, 0, w - 40)

        # code viewport
        view = doc.crop((0, scroll, w - 40, scroll + body_h))
        win.paste(view, (0, 26))
        # gutter overlay
        wd.rectangle([0, 26, GUTTER, 26 + body_h], outline=(38, 40, 44))
        wd.rectangle([0, 26, GUTTER, 26 + body_h], fill=None)

        # selection highlight + blinking cursor
        sy = sel_line * LH - scroll + 26
        if 26 <= sy < 26 + body_h - LH:
            wd.rectangle([GUTTER, sy, w - 40, sy + LH], fill=COLORS["sel"])
            # redraw the code line over the selection
            x = GUTTER + 8
            for txt, col in gen_code_line(random.Random(sel_line * 31 + 5), font):
                wd.text((x, sy + 1), txt, font=font, fill=col)
                x += wd.textlength(txt, font=font)
        if (fi // 15) % 2 == 0:
            cy = (sel_line + 1) * LH - scroll + 26
            if 26 <= cy < 26 + body_h - LH:
                wd.rectangle([GUTTER + 30, cy, GUTTER + 37, cy + LH - 3],
                             fill=(220, 220, 220))

        # terminal panel
        tv = term.crop((0, tscroll, w - 40, tscroll + TERM_H))
        win.paste(tv, (0, 26 + body_h))
        wd.line([0, 26 + body_h, w - 40, 26 + body_h], fill=(60, 62, 66))

        # status bar
        wd.rectangle([0, h - STATUS_H, w - 40, h], fill=STATUS)
        wd.text((10, h - STATUS_H + 4),
                f"main*  UTF-8  C  Ln {sel_line + scroll // LH}, Col 8   frame {fi}",
                font=font, fill=(235, 235, 235))

        # photo thumbnail pasted into the doc area (camera-ish content)
        if photo is not None:
            win_np = np.array(win)
            py0 = 60; px0 = win_np.shape[1] - 260
            win_np[py0:py0 + 150, px0:px0 + 220] = photo
            win = Image.fromarray(win_np)

        if zwall:
            shadow_blit(frame, win, 20 + wx, wy)
        frame.paste(win, (20 + wx, wy))
        if znoise:
            # subtle luminance noise: real captures are never perfectly clean
            g = grain.normal(0, 1.2, (h, w, 1))
            fn = np.asarray(frame).astype(np.int16) + g.astype(np.int16)
            frame = Image.fromarray(np.clip(fn, 0, 255).astype(np.uint8))
        ffmpeg.stdin.write(np.asarray(frame, np.uint8).tobytes())
        if fi % 50 == 0:
            print(f"  frame {fi}/{n}", flush=True)

    ffmpeg.stdin.close()
    ffmpeg.wait()
    print(f"done -> {out} ({os.path.getsize(out):,} B)")


if __name__ == "__main__":
    main()
