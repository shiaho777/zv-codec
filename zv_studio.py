#!/usr/bin/env python3

import os
import sys
import time
import struct
import ctypes
import threading
import platform
import queue
from concurrent.futures import ThreadPoolExecutor

import tkinter as tk
from tkinter import ttk, filedialog, messagebox

import numpy as np

try:
    import cv2
except ImportError:
    cv2 = None

try:
    from PIL import Image, ImageTk
except ImportError:
    Image = None
    ImageTk = None


def _lib_filename():
    s = platform.system().lower()
    if "darwin" in s:
        return "libzv_core.dylib"
    if "windows" in s or "cygwin" in s:
        return "zv_core.dll"
    return "libzv_core.so"


def _load_lib():
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, _lib_filename()),
        os.path.join(here, "libzv_core.dylib"),
        os.path.join(here, "libzv_core.so"),
        os.path.join(here, "zv_core.dll"),
    ]
    for p in candidates:
        if os.path.exists(p):
            return ctypes.CDLL(p), p
    raise FileNotFoundError(
        "cannot find zv_core shared library. expected one of:\n  "
        + "\n  ".join(candidates)
    )


LIB, LIB_PATH = _load_lib()

LIB.rgb_to_ycocgr_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
LIB.rgb_to_ycocgr_frame.restype = None
LIB.ycocgr_to_rgb_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
LIB.ycocgr_to_rgb_frame.restype = None
LIB.zv_encode_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                ctypes.c_void_p, ctypes.c_int32]
LIB.zv_encode_frame.restype = ctypes.c_int32
LIB.zv_decode_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                ctypes.c_void_p, ctypes.c_int32]
LIB.zv_decode_frame.restype = ctypes.c_int32
LIB.zv_encode_plane_stripe.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                       ctypes.c_void_p, ctypes.c_int32]
LIB.zv_encode_plane_stripe.restype = ctypes.c_int32
LIB.zv_decode_plane_stripe.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                       ctypes.c_void_p, ctypes.c_int32]
LIB.zv_decode_plane_stripe.restype = ctypes.c_int32


def rgb_to_ycocgr(rgb):
    h, w, _ = rgb.shape
    out = np.empty((h, w, 3), dtype=np.int16)
    LIB.rgb_to_ycocgr_frame(rgb.ctypes.data_as(ctypes.c_void_p),
                            out.ctypes.data_as(ctypes.c_void_p), w, h)
    return out


def ycocgr_to_rgb(yc):
    h, w, _ = yc.shape
    out = np.empty((h, w, 3), dtype=np.uint8)
    LIB.ycocgr_to_rgb_frame(yc.ctypes.data_as(ctypes.c_void_p),
                            out.ctypes.data_as(ctypes.c_void_p), w, h)
    return out


def stripe_boundaries(h, n, align=16):
    n_blocks = h // align
    if n_blocks == 0:
        return [(0, h)]
    n = max(1, min(n, n_blocks))
    base = n_blocks // n
    extra = n_blocks - base * n
    bs = []
    y = 0
    for i in range(n):
        bh = (base + (1 if i < extra else 0)) * align
        bs.append((y, bh))
        y += bh
    if y < h:
        ly, lh = bs[-1]
        bs[-1] = (ly, lh + (h - y))
    return bs


class ZVReader:
    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        magic = self.f.read(2)
        if magic != b"ZV":
            self.f.close()
            raise ValueError("not a ZV file")
        self.version = self.f.read(1)[0]
        if self.version not in (10, 11):
            self.f.close()
            raise ValueError(f"unsupported ZV version: {self.version}")
        self.w, self.h = struct.unpack(">HH", self.f.read(4))
        self.fps = self.f.read(1)[0]
        self.fc, = struct.unpack(">I", self.f.read(4))
        self.sizes = [struct.unpack(">I", self.f.read(4))[0] for _ in range(self.fc)]
        self.payload_start = self.f.tell()
        self.frame_offsets = []
        p = self.payload_start
        for s in self.sizes:
            self.frame_offsets.append(p)
            p += s

    def read_payload(self, idx):
        self.f.seek(self.frame_offsets[idx])
        return self.f.read(self.sizes[idx])

    def close(self):
        try:
            self.f.close()
        except Exception:
            pass


class ZVDecoder:
    def __init__(self, reader, threads=8):
        self.r = reader
        self.w, self.h = reader.w, reader.h
        self.version = reader.version
        if self.version == 10:
            self.cur = np.zeros((self.h, self.w, 3), dtype=np.int16)
            self.prev = None
        else:
            self.cur_Y = np.zeros((self.h, self.w), dtype=np.int16)
            self.cur_Co = np.zeros((self.h, self.w), dtype=np.int16)
            self.cur_Cg = np.zeros((self.h, self.w), dtype=np.int16)
            self.prev_Y = self.prev_Co = self.prev_Cg = None
            self.pool = ThreadPoolExecutor(max_workers=max(8, threads * 3))
        self.last_idx = -1

    def reset(self):
        if self.version == 10:
            self.prev = None
        else:
            self.prev_Y = self.prev_Co = self.prev_Cg = None
        self.last_idx = -1

    def _decode_v10(self, idx):
        buf = self.r.read_payload(idx)
        in_buf = (ctypes.c_uint8 * len(buf)).from_buffer_copy(buf)
        prev_ptr = self.prev.ctypes.data_as(ctypes.c_void_p) if self.prev is not None else None
        LIB.zv_decode_frame(prev_ptr, self.cur.ctypes.data_as(ctypes.c_void_p),
                            self.w, self.h, ctypes.cast(in_buf, ctypes.c_void_p), len(buf))
        if self.prev is None:
            self.prev = self.cur.copy()
        else:
            np.copyto(self.prev, self.cur)
        return ycocgr_to_rgb(self.cur)

    def _decode_v11(self, idx):
        buf = self.r.read_payload(idx)
        pos = 0
        _flags = buf[pos]; pos += 1
        n_real = buf[pos]; pos += 1
        stripes = stripe_boundaries(self.h, n_real)
        plane_payloads = [[None] * n_real for _ in range(3)]
        for plane_id in range(3):
            for sid in range(n_real):
                sl, = struct.unpack(">I", buf[pos:pos + 4]); pos += 4
                plane_payloads[plane_id][sid] = bytes(buf[pos:pos + sl]); pos += sl

        def task(plane_id, sid):
            payload = plane_payloads[plane_id][sid]
            y0, sh = stripes[sid]
            cur_p = (self.cur_Y, self.cur_Co, self.cur_Cg)[plane_id]
            prev_p = (self.prev_Y, self.prev_Co, self.prev_Cg)[plane_id]
            cur_offset = y0 * self.w
            cur_ptr = cur_p.ctypes.data + cur_offset * 2
            prev_ptr = (prev_p.ctypes.data + cur_offset * 2) if prev_p is not None else 0
            in_buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
            LIB.zv_decode_plane_stripe(ctypes.c_void_p(prev_ptr), ctypes.c_void_p(cur_ptr),
                                       self.w, sh, ctypes.cast(in_buf, ctypes.c_void_p), len(payload))

        futs = [self.pool.submit(task, p, s) for p in range(3) for s in range(n_real)]
        for ft in futs:
            ft.result()

        if self.prev_Y is None:
            self.prev_Y = self.cur_Y.copy()
            self.prev_Co = self.cur_Co.copy()
            self.prev_Cg = self.cur_Cg.copy()
        else:
            np.copyto(self.prev_Y, self.cur_Y)
            np.copyto(self.prev_Co, self.cur_Co)
            np.copyto(self.prev_Cg, self.cur_Cg)

        yc = np.empty((self.h, self.w, 3), dtype=np.int16)
        yc[..., 0] = self.cur_Y
        yc[..., 1] = self.cur_Co
        yc[..., 2] = self.cur_Cg
        return ycocgr_to_rgb(yc)

    def decode_to(self, target):
        if target < self.last_idx:
            self.reset()
        while self.last_idx < target:
            self.last_idx += 1
            if self.version == 10:
                rgb = self._decode_v10(self.last_idx)
            else:
                rgb = self._decode_v11(self.last_idx)
        return rgb

    def close(self):
        if self.version == 11:
            try:
                self.pool.shutdown(wait=False)
            except Exception:
                pass


def encode_video_to_zv(input_path, output_path, n_stripes=8,
                      progress_cb=None, abort_flag=None):
    if cv2 is None:
        raise RuntimeError("opencv-python not installed: pip install opencv-python")

    cap = cv2.VideoCapture(input_path)
    if not cap.isOpened():
        raise RuntimeError(f"cannot open: {input_path}")

    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = int(round(cap.get(cv2.CAP_PROP_FPS) or 24))
    if fps < 1: fps = 1
    if fps > 240: fps = 240
    fc = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

    use_mt = n_stripes > 1
    version = 11 if use_mt else 10

    if use_mt:
        stripes = stripe_boundaries(h, n_stripes)
        n_real = len(stripes)
        pool = ThreadPoolExecutor(max_workers=max(8, n_real * 3))
    else:
        n_real = 1
        pool = None
        max_frame_bytes = 8 * w * h + 4096
        out_buf = (ctypes.c_uint8 * max_frame_bytes)()

    sizes = []
    fi = 0
    total_bytes = 0
    aborted = False
    t0 = time.time()

    prev_ycocg = None
    prev_Y = prev_Co = prev_Cg = None

    with open(output_path, "wb") as fout:
        fout.write(b"ZV")
        fout.write(bytes([version]))
        fout.write(struct.pack(">HH", w, h))
        fout.write(bytes([fps & 0xFF]))
        fc_pos = fout.tell()
        fout.write(struct.pack(">I", max(fc, 0)))
        size_table_pos = fout.tell()
        fout.write(b"\x00\x00\x00\x00" * max(fc, 0))

        while True:
            if abort_flag is not None and abort_flag.is_set():
                aborted = True
                break
            ok, frm = cap.read()
            if not ok:
                break
            rgb = cv2.cvtColor(frm, cv2.COLOR_BGR2RGB)
            cur_yc = rgb_to_ycocgr(rgb)
            del rgb, frm

            if not use_mt:
                prev_ptr = prev_ycocg.ctypes.data_as(ctypes.c_void_p) if prev_ycocg is not None else None
                n = LIB.zv_encode_frame(prev_ptr, cur_yc.ctypes.data_as(ctypes.c_void_p),
                                        w, h, out_buf, max_frame_bytes)
                if n <= 0:
                    raise RuntimeError(f"encode failed at frame {fi}")
                fout.write(bytes(out_buf[:n]))
                sizes.append(int(n))
                total_bytes += n
                if prev_ycocg is None:
                    prev_ycocg = cur_yc.copy()
                else:
                    np.copyto(prev_ycocg, cur_yc)
            else:
                cur_Y = np.ascontiguousarray(cur_yc[..., 0])
                cur_Co = np.ascontiguousarray(cur_yc[..., 1])
                cur_Cg = np.ascontiguousarray(cur_yc[..., 2])
                del cur_yc

                results = {}

                def enc_task(plane_id, sid, prev_p, cur_p, y0, sh):
                    cur_offset = y0 * w
                    cur_ptr = cur_p.ctypes.data + cur_offset * 2
                    prev_ptr = (prev_p.ctypes.data + cur_offset * 2) if prev_p is not None else 0
                    max_buf = 4 * w * sh + 4096
                    out_buf_local = (ctypes.c_uint8 * max_buf)()
                    n = LIB.zv_encode_plane_stripe(ctypes.c_void_p(prev_ptr), ctypes.c_void_p(cur_ptr),
                                                   w, sh, out_buf_local, max_buf)
                    results[(plane_id, sid)] = bytes(out_buf_local[:n])

                futs = []
                planes = [(prev_Y, cur_Y), (prev_Co, cur_Co), (prev_Cg, cur_Cg)]
                for plane_id, (prev_p, cur_p) in enumerate(planes):
                    for sid, (y0, sh) in enumerate(stripes):
                        futs.append(pool.submit(enc_task, plane_id, sid, prev_p, cur_p, y0, sh))
                for ft in futs:
                    ft.result()

                frame_buf = bytearray()
                frame_buf.append(0)
                frame_buf.append(n_real)
                for plane_id in range(3):
                    for sid in range(n_real):
                        p = results[(plane_id, sid)]
                        frame_buf.extend(struct.pack(">I", len(p)))
                        frame_buf.extend(p)
                fout.write(bytes(frame_buf))
                sizes.append(len(frame_buf))
                total_bytes += len(frame_buf)
                prev_Y, prev_Co, prev_Cg = cur_Y, cur_Co, cur_Cg

            fi += 1
            if progress_cb is not None:
                elapsed = time.time() - t0
                progress_cb(fi, fc, total_bytes, elapsed)

        if fi != fc:
            fout.seek(fc_pos)
            fout.write(struct.pack(">I", fi))
        fout.seek(size_table_pos)
        for sz in sizes:
            fout.write(struct.pack(">I", sz))
        if fi < fc:
            payload_end = size_table_pos + fi * 4 + total_bytes
            fout.truncate(payload_end)

    cap.release()
    if pool is not None:
        pool.shutdown()

    return {
        "frames": fi,
        "fps": fps,
        "w": w, "h": h,
        "version": version,
        "bytes": total_bytes,
        "elapsed": time.time() - t0,
        "aborted": aborted,
    }


class PlayerTab(ttk.Frame):
    def __init__(self, master):
        super().__init__(master)
        self.reader = None
        self.decoder = None
        self.playing = False
        self.cur_idx = 0
        self.tk_img = None
        self.last_tick = 0.0

        toolbar = ttk.Frame(self)
        toolbar.pack(fill="x", padx=8, pady=6)
        ttk.Button(toolbar, text="Open .zv", command=self.open_file).pack(side="left")
        self.info_var = tk.StringVar(value="no file loaded")
        ttk.Label(toolbar, textvariable=self.info_var, foreground="#888").pack(side="left", padx=12)

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True, padx=8, pady=4)
        self.canvas = tk.Canvas(body, bg="black", highlightthickness=0, width=640, height=360)
        self.canvas.pack(fill="both", expand=True)
        self.canvas.bind("<Configure>", lambda e: self._redraw())

        ctrl = ttk.Frame(self)
        ctrl.pack(fill="x", padx=8, pady=6)
        self.play_btn = ttk.Button(ctrl, text="\u25B6", width=4, command=self.toggle_play)
        self.play_btn.pack(side="left")
        ttk.Button(ctrl, text="|\u25C0", width=4, command=self.step_back).pack(side="left")
        ttk.Button(ctrl, text="\u25B6|", width=4, command=self.step_forward).pack(side="left")
        self.seek_var = tk.IntVar(value=0)
        self.seek = ttk.Scale(ctrl, from_=0, to=0, orient="horizontal",
                              command=self.on_seek)
        self.seek.pack(side="left", fill="x", expand=True, padx=8)
        self.status_var = tk.StringVar(value="\u2014")
        ttk.Label(ctrl, textvariable=self.status_var, width=20, anchor="e").pack(side="right")

        self.bind_all("<space>", lambda e: self.toggle_play() if self.reader else None)
        self.bind_all("<Left>", lambda e: self.step_back() if self.reader else None)
        self.bind_all("<Right>", lambda e: self.step_forward() if self.reader else None)
        self.bind_all("<Home>", lambda e: self.seek_to(0) if self.reader else None)
        self.bind_all("<End>", lambda e: self.seek_to(self.reader.fc - 1) if self.reader else None)

        self._cached_img = None

    def open_file(self, path=None):
        if path is None:
            path = filedialog.askopenfilename(title="Open .zv file",
                                              filetypes=[("ZV video", "*.zv"), ("All files", "*.*")])
        if not path:
            return
        try:
            r = ZVReader(path)
        except Exception as e:
            messagebox.showerror("Cannot open", str(e))
            return
        self.close_current()
        self.reader = r
        self.decoder = ZVDecoder(r)
        self.cur_idx = 0
        self.seek.configure(from_=0, to=max(0, r.fc - 1))
        self.seek_var.set(0)
        self.info_var.set(f"{os.path.basename(path)}  -  {r.w}x{r.h} @ {r.fps}fps  -  "
                          f"{r.fc} frames  -  v{r.version}")
        self.seek_to(0)

    def close_current(self):
        self.playing = False
        if self.decoder is not None:
            self.decoder.close()
            self.decoder = None
        if self.reader is not None:
            self.reader.close()
            self.reader = None
        self.canvas.delete("all")
        self._cached_img = None

    def seek_to(self, target):
        if self.reader is None:
            return
        if target < 0: target = 0
        if target >= self.reader.fc: target = self.reader.fc - 1
        rgb = self.decoder.decode_to(target)
        self.cur_idx = target
        self._cached_img = rgb
        self.seek.set(target)
        self.status_var.set(f"frame {target + 1} / {self.reader.fc}")
        self._redraw()

    def _redraw(self):
        if self._cached_img is None:
            return
        if Image is None or ImageTk is None:
            self.canvas.delete("all")
            self.canvas.create_text(self.canvas.winfo_width() // 2, self.canvas.winfo_height() // 2,
                                    text="Pillow not installed: pip install pillow",
                                    fill="white")
            return
        cw = max(1, self.canvas.winfo_width())
        ch = max(1, self.canvas.winfo_height())
        ih, iw, _ = self._cached_img.shape
        scale = min(cw / iw, ch / ih)
        if scale <= 0: scale = 1.0
        tw, th = max(1, int(iw * scale)), max(1, int(ih * scale))
        pil = Image.fromarray(self._cached_img)
        if (tw, th) != (iw, ih):
            pil = pil.resize((tw, th), Image.BILINEAR)
        self.tk_img = ImageTk.PhotoImage(pil)
        self.canvas.delete("all")
        self.canvas.create_image(cw // 2, ch // 2, image=self.tk_img)

    def on_seek(self, val):
        if self.reader is None: return
        target = int(float(val))
        if target == self.cur_idx: return
        self.playing = False
        self.play_btn.configure(text="\u25B6")
        self.seek_to(target)

    def toggle_play(self):
        if self.reader is None: return
        if self.playing:
            self.playing = False
            self.play_btn.configure(text="\u25B6")
        else:
            if self.cur_idx >= self.reader.fc - 1:
                self.seek_to(0)
            self.playing = True
            self.play_btn.configure(text="\u25AE\u25AE")
            self.last_tick = time.time()
            self.after(1, self._tick)

    def _tick(self):
        if not self.playing or self.reader is None:
            return
        interval = 1.0 / max(1, self.reader.fps)
        now = time.time()
        steps = int((now - self.last_tick) / interval)
        if steps > 0:
            target = min(self.reader.fc - 1, self.cur_idx + steps)
            if target == self.reader.fc - 1 and self.cur_idx == target:
                self.playing = False
                self.play_btn.configure(text="\u25B6")
                return
            self.seek_to(target)
            self.last_tick += steps * interval
            if self.cur_idx >= self.reader.fc - 1:
                self.playing = False
                self.play_btn.configure(text="\u25B6")
                return
        self.after(max(1, int(interval * 1000 / 2)), self._tick)

    def step_back(self):
        if self.reader is None: return
        self.playing = False
        self.play_btn.configure(text="\u25B6")
        self.seek_to(self.cur_idx - 1)

    def step_forward(self):
        if self.reader is None: return
        self.playing = False
        self.play_btn.configure(text="\u25B6")
        self.seek_to(self.cur_idx + 1)


class ConverterTab(ttk.Frame):
    def __init__(self, master):
        super().__init__(master)
        self.input_path = None
        self.output_path = None
        self.thread = None
        self.abort_flag = threading.Event()
        self.event_q = queue.Queue()

        toolbar = ttk.Frame(self)
        toolbar.pack(fill="x", padx=8, pady=6)
        ttk.Button(toolbar, text="Choose video...", command=self.choose_input).pack(side="left")
        self.input_var = tk.StringVar(value="no input")
        ttk.Label(toolbar, textvariable=self.input_var, foreground="#888").pack(side="left", padx=8)

        opts = ttk.Frame(self)
        opts.pack(fill="x", padx=8, pady=4)
        ttk.Label(opts, text="Stripes (1 = single-thread v10, 8 = multi-thread v11):").pack(side="left")
        self.stripes_var = tk.IntVar(value=8)
        sp = ttk.Spinbox(opts, from_=1, to=32, textvariable=self.stripes_var, width=5)
        sp.pack(side="left", padx=8)

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True, padx=8, pady=4)
        self.log = tk.Text(body, height=14, bg="#111", fg="#ddd",
                           font="TkFixedFont",
                           insertbackground="#ddd", relief="flat")
        self.log.pack(fill="both", expand=True)
        self.log.configure(state="disabled")

        self.prog = ttk.Progressbar(self, mode="determinate")
        self.prog.pack(fill="x", padx=8, pady=4)

        bot = ttk.Frame(self)
        bot.pack(fill="x", padx=8, pady=6)
        self.start_btn = ttk.Button(bot, text="Start conversion", command=self.start)
        self.start_btn.pack(side="left")
        self.cancel_btn = ttk.Button(bot, text="Cancel", command=self.cancel, state="disabled")
        self.cancel_btn.pack(side="left", padx=6)

        self.after(50, self._poll_events)

    def _log(self, msg):
        self.log.configure(state="normal")
        self.log.insert("end", msg + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def choose_input(self):
        if cv2 is None:
            messagebox.showerror("Missing dependency",
                                 "opencv-python is required for the converter.\n"
                                 "install with:  pip install opencv-python")
            return
        p = filedialog.askopenfilename(title="Choose video file",
                                       filetypes=[("Video", "*.mp4 *.mov *.webm *.mkv *.avi *.m4v"),
                                                  ("All files", "*.*")])
        if not p:
            return
        self.input_path = p
        self.input_var.set(os.path.basename(p))
        cap = cv2.VideoCapture(p)
        if cap.isOpened():
            w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            fps = cap.get(cv2.CAP_PROP_FPS) or 0
            fc = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
            dur = fc / fps if fps > 0 else 0
            self._log(f"input: {os.path.basename(p)}  {w}x{h} @ {fps:.2f}fps  {fc} frames  ({dur:.2f}s)")
            cap.release()
        else:
            self._log(f"input: {p}  (cannot probe; conversion may still work)")

    def start(self):
        if self.thread and self.thread.is_alive():
            return
        if cv2 is None:
            messagebox.showerror("Missing dependency",
                                 "opencv-python is required for the converter.\n"
                                 "install with:  pip install opencv-python")
            return
        if not self.input_path:
            self.choose_input()
            if not self.input_path:
                return
        default_out = os.path.splitext(self.input_path)[0] + ".zv"
        out = filedialog.asksaveasfilename(title="Save .zv as...",
                                           defaultextension=".zv",
                                           initialfile=os.path.basename(default_out),
                                           filetypes=[("ZV video", "*.zv")])
        if not out:
            return
        self.output_path = out
        self.abort_flag.clear()
        self.start_btn.configure(state="disabled")
        self.cancel_btn.configure(state="normal")
        self.prog["value"] = 0
        self._log(f"\nstart: {os.path.basename(self.input_path)} -> {os.path.basename(out)}")
        self._log(f"stripes: {self.stripes_var.get()}")

        n_stripes = max(1, int(self.stripes_var.get()))

        def progress_cb(fi, fc, total_bytes, elapsed):
            self.event_q.put(("progress", fi, fc, total_bytes, elapsed))

        def worker():
            try:
                result = encode_video_to_zv(self.input_path, self.output_path,
                                            n_stripes=n_stripes,
                                            progress_cb=progress_cb,
                                            abort_flag=self.abort_flag)
                self.event_q.put(("done", result))
            except Exception as e:
                self.event_q.put(("error", str(e)))

        self.thread = threading.Thread(target=worker, daemon=True)
        self.thread.start()

    def cancel(self):
        if self.thread and self.thread.is_alive():
            self.abort_flag.set()
            self._log("aborting...")

    def _poll_events(self):
        try:
            while True:
                ev = self.event_q.get_nowait()
                if ev[0] == "progress":
                    _, fi, fc, total_bytes, elapsed = ev
                    if fc > 0:
                        self.prog["value"] = 100.0 * fi / fc
                    rate = fi / elapsed if elapsed > 0 else 0
                    if fi % 5 == 0 or (fc > 0 and fi == fc):
                        self._log(f"  {fi}/{fc}  {rate:.1f} fps  {total_bytes/1024:.0f} KB")
                elif ev[0] == "done":
                    res = ev[1]
                    self.start_btn.configure(state="normal")
                    self.cancel_btn.configure(state="disabled")
                    self.prog["value"] = 100
                    if res["aborted"]:
                        self._log(f"aborted after {res['frames']} frames")
                    else:
                        self._log(f"done: {res['frames']} frames @ {res['fps']}fps  "
                                  f"v{res['version']}  "
                                  f"{res['bytes']/1024:.0f} KB  in {res['elapsed']:.1f}s  "
                                  f"({res['frames']/res['elapsed']:.1f} fps)")
                        self._log(f"output: {self.output_path}")
                elif ev[0] == "error":
                    self.start_btn.configure(state="normal")
                    self.cancel_btn.configure(state="disabled")
                    messagebox.showerror("Conversion failed", ev[1])
                    self._log(f"ERROR: {ev[1]}")
        except queue.Empty:
            pass
        self.after(50, self._poll_events)


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ZV Studio")
        self.geometry("1024x720")
        self.minsize(720, 480)

        try:
            style = ttk.Style(self)
            for theme in ("clam", "alt", "default"):
                if theme in style.theme_names():
                    style.theme_use(theme)
                    break
        except Exception:
            pass

        nb = ttk.Notebook(self)
        nb.pack(fill="both", expand=True)
        self.player = PlayerTab(nb)
        self.converter = ConverterTab(nb)
        nb.add(self.player, text="Play")
        nb.add(self.converter, text="Convert")

        status = ttk.Frame(self)
        status.pack(fill="x")
        ttk.Label(status, text=f"library: {os.path.basename(LIB_PATH)}",
                  foreground="#666").pack(side="left", padx=8, pady=2)

        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def on_close(self):
        try:
            self.player.close_current()
        except Exception:
            pass
        try:
            if self.converter.thread and self.converter.thread.is_alive():
                self.converter.abort_flag.set()
        except Exception:
            pass
        self.destroy()


def main():
    if Image is None or ImageTk is None:
        sys.stderr.write("WARNING: Pillow not installed; player will not show frames.\n"
                         "         install with:  pip install pillow\n")
    if cv2 is None:
        sys.stderr.write("WARNING: opencv-python not installed; converter disabled.\n"
                         "         install with:  pip install opencv-python\n")
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()
