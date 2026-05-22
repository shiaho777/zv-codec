# ZV

[English](README.md)

一个小型、近无损的视频与音频编解码器。约 1100 行 C，无神经网络、无变换、无外部依赖。

在屏幕录制类内容上，ZV 比 x265 严格无损 RGB 输出小 **24%**，编码速度持平；
每像素误差控制在 ±5 RGB 以内（远低于人眼可辨阈值）。音频为严格无损，
压缩率与 FLAC 相差约 3%，而代码量不到 FLAC 的 1/250。

## 这是什么，不是什么

这是一个可读、可改的概率编码视频管线参考实现。它在小众场景（屏幕录制、
UI 演示、近静止画面）上有意思，作为教学样例也合适——整套编码器、解码器
和音频编解码代码都能一屏看完。

它**不是** H.264 / H.265 / AV1 的替代品。那些格式背后是数十年研究、硬件
解码和庞大的生态，处理自然视频的能力比 ZV 强得多。ZV 不试图在通用场景
对标它们；只是恰好在一类窄内容上表现不错。

## 性能

视频，屏幕录制 3456×2234，146 帧，3.38 GB raw：

| 编码器 | 输出 | 压缩率 | 编码时间 | 无损性 |
|--------|------|--------|---------|--------|
| HuffYUV | 934 MB | 3.6× | 1.2 s | 严格 |
| FFV1 | 123 MB | 27.5× | 4.4 s | 严格 |
| x265 lossless RGB | 11.0 MB | 307× | 8.0 s | 严格 |
| **ZV (单线程)** | **8.39 MB** | **403×** | **5.6 s** | ±5 RGB |
| **ZV (8 stripes)** | **8.66 MB** | 391× | **2.3 s** | ±5 RGB |
| x264 "lossless" | 4.5 MB | 743× | 2.9 s | ±2 (YUV420) |

音频，10 秒立体声 @ 44100 Hz，1.76 MB raw：

| 编码器 | 输出 | 压缩率 | 无损性 |
|--------|------|--------|--------|
| FLAC -8 | 1409 KB | 1.25× | 严格 |
| **ZV audio** | **1447 KB** | 1.22× | 严格 |

## 构建与运行

编译两个共享库（macOS / Linux）：

```sh
clang -O3 -march=native -shared -fPIC -o libzv_core.dylib  zv_core.c
clang -O3 -march=native -shared -fPIC -o libzv_audio.dylib zv_audio.c
```

用虚拟环境装依赖，避免和系统包管理器冲突：

```sh
python3 -m venv .venv
.venv/bin/pip install numpy opencv-python pillow
```

macOS：系统自带 Python 的 Tk 在 macOS 16 上不可用，先装 Homebrew 的：

```sh
brew install python@3.12 python-tk@3.12
```

Linux：`sudo apt install python3-tk`（或 `dnf install python3-tkinter`）。

Windows：装 [Python 3](https://www.python.org/downloads/windows/) 即可，Tk 自带。

然后：

```sh
.venv/bin/python zv_studio.py             # GUI：播放 .zv，或把任意视频转为 .zv
.venv/bin/python encoder.py    in.mp4 out.zv
.venv/bin/python decoder.py    out.zv decoded.mp4 --no-play
.venv/bin/python encoder_mt.py in.mp4 out.zv [num_stripes=8]   # 多线程
.venv/bin/python decoder_mt.py out.zv decoded.mp4 --no-play
.venv/bin/python audio_test.py in.wav     # 音频编解码 + FLAC 对比
.venv/bin/python test_roundtrip.py
.venv/bin/python bench.py      in.mp4
```

## 工作原理

每个像素或音频样本以"在因果上下文下的条件熵"为代价编码。上下文统计量
由编解码器双方共享地在线维护，因此预测器选择无需信令。无变换、无率失真
搜索、无块分割信令。

```
RGB → YCoCg-R → 1-bit Y / 2-bit chroma 量化
              → 6 个空间预测器 + 1 个时域预测器
              → 405 上下文 (局部梯度)
              → per-context arg-min 模式选择
              → Rice(k) + 二进制算术编码
```

音频沿用同样的结构，3 个预测器 + 8 上下文，立体声做 mid-side 分离。

多线程编码器把每帧切成 N 个水平条带（默认 8，行数 16 的倍数）。
每个 (条带, 通道) 是独立任务；`ctypes` 调用释放 GIL，多核加速是真实的。

| 条带数 | 大小 | 时间 | 加速比 |
|--------|------|------|--------|
| 1 | 8.39 MB | 5.6 s | 1.0× |
| 4 | 8.55 MB | 2.7 s | 2.1× |
| **8** | **8.66 MB** | **2.3 s** | **2.5×** |
| 16 | 8.81 MB | 2.7 s | 2.1× |

更多细节直接看 `zv_core.c`（约 700 行）和 `zv_audio.c`（约 400 行）。

## 文件格式

```
.zv  ::= header[12]  size_table[4 × frames]  frame_payloads...
header  ::= "ZV" version w[2] h[2] fps frames[4]
```

两种载荷布局共用同一外层 header：

- v10（单线程）：每帧一个 AC 流，包含 3 个平面。
- v11（多 stripe）：按平面、按 stripe 各自独立 AC 流，前缀写明 stripe 数。
  `zv_studio.py` 和多线程驱动默认产出 v11；v10 保留作最简单的单线程路径。

音频流没有外层 header；调用方通过外层容器传递帧数和通道数。

## 适用场景

- 屏幕录制、教学视频、代码演示
- UI 动画、应用录屏
- 任何接近静止的画面

对自然视频（电影、相机拍摄）的优势会缩小；在这些场景下，H.265 / AV1
出来的文件更小、画质更高。

## 限制

- 视频仅支持 8-bit RGB；音频仅支持 16-bit PCM。
- 视频感知无损（最大 ±5 RGB 偏差），不是严格无损。
- 音频严格无损。
- 没有容器格式，视频和音频要合在一个文件需要外部 mux。

## License

MIT，见 [LICENSE](LICENSE)。
