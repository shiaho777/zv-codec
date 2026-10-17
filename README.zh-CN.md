# ZV

[English](README.md)

一个小型视频与音频编解码器。无神经网络、无外部依赖。可选的可逆 Haar 叶
（`OP_TX` / `OP_TXI`）和因果预测器并列，旧码流不会发出它们。

v17 在 eps=0 时是 **8-bit RGB 逐位无损**（可逆 YCoCg-R，不再做 1/2 bit 预量化）。
v18 是同一套载荷，输入输出改为 **10-bit RGB**。近无损（eps>0）的残差量化也在
全精度域进行，PSNR 不再被钉在 ~43 dB。屏幕/近静止内容上，无损体积通常小于
x265 lossless RGB，接近 JPEG XL / AV1 lossless。音频严格无损；ZA v2（分块 LPC）
在音乐上与 FLAC -8 相当。`ZVM1` 容器可把 ZV 视频和 ZA 音频包在一个文件里。

## 这是什么，不是什么

这是一个可读、可改的概率编码视频管线参考实现。它在小众场景（屏幕录制、
UI 演示、近静止画面）上有意思，作为教学样例也合适——整套编码器、解码器
和音频编解码代码都能一屏看完。

它**不是** H.264 / H.265 / AV1 的替代品。那些格式背后是数十年研究、硬件
解码和庞大的生态。ZV 是一套可改的 RGB 无损管线：屏幕内容是本职，相机 RGB
无损现在也小于 FFV1 / x265 lossless RGB（AV1 lossless YUV 仍然更小）。

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
| FLAC -8 | 784 KB | 2.20× | 严格 |
| **ZV audio v2** | **782 KB** | **2.20×** | 严格 |

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
由编解码器双方共享地在线维护。16×16 tile 按码率（`eps>0` 时按率失真）选叶
操作。相机纹理大多走 `PREDNFX`（稠密残差，每个邻域一个 rANS 模型）。关键帧从
该 fold 起算，邻域模型才能热起来；仍需要左右/上方 run 的块走 `OP_PREDX`
（PRED 的 flag，fold 残差）。平滑亮度方块可以改走时域残差的可逆 Haar
（`OP_TX`）或像素减边界 DC 的 Haar（`OP_TXI`）。

```
RGB → 可逆 YCoCg-R
    → 16×16 tile，递归 SPLIT，逐 tile 率失真选 op
    → 空间 / 时域 / 调色板 / Haar 残差叶
    → rANS，段内冻结 CDF，码流不传表
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

更多细节直接看 `zv_core.c` 和 `zv_audio.c`。

## 文件格式

```
.zv  ::= header[12]  size_table[4 × frames]  frame_payloads...
header  ::= "ZV" version w[2] h[2] fps frames[4]
```

两种载荷布局共用同一外层 header：

- v10（单线程）：每帧一个 AC 流，包含 3 个平面。
- v11（多 stripe）：按平面、按 stripe 各自独立 AC 流，前缀写明 stripe 数。
- v12（单线程）：与 v10 布局相同，但熵编码从二进制算术编码换成 rANS。
  每个平面流以 flags 字节开头（先验形状、交织 lane 数），之后是若干
  独立 rANS 分段，使用按上下文自适应的 CDF。
- v13（多 stripe）：v12 的 stripe 版本。
- v14（单线程）：tile 程序格式。每个平面切成 16×16 tile 网格，编码为
  两条 rANS 流：一条"程序"流存放逐 tile 操作码+参数，一条"残差"流存放
  逐像素记录。操作集：`RUN`（整 tile 复制上一帧同位置）、`COPY`（带运动
  矢量偏移的时间拷贝+残差）、`ICOPY`（从当前帧已解码区域拷贝——面向屏幕
  内容的块匹配）、`PAL`（≤16 项调色板+索引图）、`PRED`（完整
  v12 因果预测机制）、`NN`（每平面学习线性预测器）、`SPLIT`（递归
  四分 tile 为 8×8 子块、各自独立选操作）、`MVRUN`/`IRUN`（skip 模式：
  完全匹配的拷贝只发运动矢量、零残差流）及其预测 MV 变体
  `MVRUNP`/`COPYP`/`IRUNP`（MV 取自上一个同类型操作，零参数比特）、
  `RAW`（字面量兜底）、`OP_QS`（流指令——不占 tile——切换后续 tile 的
  残差量化步长）、`OP_NOISE`（过程化残差合成：tile 照常跑因果预测器，
  但残差由确定性 PRNG 生成，只传一个 σ 参数——统计等价的噪声，成本极
  低）。运动候选覆盖 ±128 像素并带单位步长菱形细化。编码器在每个 tile 上用当前
  自适应模型评估所有候选操作的比特成本，提交最便宜者。全部为 `RUN`
  的平面塌缩为 4 字节记录。此外程序头最高位可标记"每平面学习线性
  预测器"：编码端用最小二乘在因果特征 `[a,b,c,d2,t,median,a+b-c,luma,1]`
  上拟合 9 个 Q8 权重，作为程序流的前几条记录下发，解码端对 `NN`
  操作的 tile 用 `clip((sum w_i f_i + 128) >> 8)` 做逐像素预测。
  `luma` 特征让色度平面利用已解码的共位亮度（CfL 式）。编码器
  对每个 tile 在固定预测器库与学习预测器之间按比特成本择优，并对
  整平面两种模式各编码一遍取码流更小者——权重只在真正回本时下发。

  近无损模式（`encoder.py in.mp4 out.zv <eps> [eps_hi]`）：程序头
  bit30 标记量化平面，基准步长和噪声种子作为程序流前两条记录下发。
  残差按 `round(r/qs)` 量化、按 `pred + rq*qs` 重建，逐像素误差在
  YCoCg 域内不超过 `2*eps_hi+1`——`OP_QS` tile 按自身步长计，
  `OP_NOISE` tile 只在编码端确认合成后每像素都在界内时才发出。
  编码器维护与解码器逐位一致的重建平面：因果邻居、帧间参考、
  色度 CfL 全部读重建样本。精确操作（`RUN`/`MVRUN`/`PAL`/`RAW`/`RPT`）
  不受影响。有损色度从 `2*qs` 起（不超过 `qs_hi`）；tile 级 `OP_QS`
  可以再粗，但有损平面不会把步长重新打开到 `qs=1`（否则高 eps 文件
  会比中 eps 更大）。屏幕内容仍保留帧级无损旁路，更小时取无损。
  `OP_TX` / `OP_TXI`（可逆 4/8/16 Haar，仅亮度，系数走 CFL_MDL）
  在 0.25–4 bpp 的残差叶上竞争。
- v15（多 stripe）：v14 的 stripe 版本。
- v16（stateful）：v14 加每平面跨帧持续解码状态——叶子描述符表
  （支持 `PSAME`/`VRPT` 回放上一帧与上方 tile）、学习预测器权重、
  预测器选择状态，以及程序/残差熵模型本身。各平面以
  `PHDR_STATEFUL` 标记。模型携带在内容分布突变时可能有害，所以
  每个有状态有损平面编码两遍——暖启动（携带模型）与冷启动（全新
  种子）——取码流更小者；平面头的 `PHDR_FRESH` 位告诉解码器使用
  哪种初始化，而胜出的后验模型状态无论谁赢都继续携带。
  `encoder.py` 默认产出 v17（同一套 `zv_encode_frame_v3sq`，颜色改为可逆
  YCoCg-R）；由 `zv_decode_frame_v3s` 解码。v10–v16 向后兼容。

`encoder_mt.py` 和 studio GUI 同样产出 v17；条带数由
`zv_encode_set_stripes` 控制（默认 1 偏体积，3+ 偏速度）。

音频流前缀 `ZA` 加版本字节，随后是算术编码载荷。v2（`0x02`）按 4096
采样分块，使用 FLAC 式固定预测或 Q10 LPC（Levinson–Durbin，Hann 分析窗）、
每块 wasted bits、Rice 残差分块，以及每块 mid-side / left-side 立体声。
v1（`0x01`）与无头旧流仍可解码。调用方仍传入帧数和声道数。

## 适用场景

- 屏幕录制、教学视频、代码演示
- UI 动画、应用录屏
- 任何接近静止的画面

相机 RGB 无损是另一回事。16 帧 1280×720 实拍（办公室、头发、布料、木纹），
eps=0，RGB 逐点还原：

| 编码器 | 体积 | bpp | 备注 |
|--------|------|-----|------|
| **ZV v17** | **3.53 MB** | **1.917** | 编码约 8.9 s，解码约 22 fps，RGB 恒等 |
| FFV1 gbrp | 4.21 MB | 2.28 | 严格 RGB |
| x265 lossless gbrp | 7.60 MB | 4.12 | 严格 RGB |
| AV1 lossless yuv444 | 2.84 MB | 1.54 | YUV，不是 RGB 恒等 |

同一片段 8 帧近无损（Apple M4 Pro）。码率随 `eps` 单调：

| 设置 | 体积 | bpp | RGB PSNR | max\|dRGB\| |
|------|------|-----|----------|-------------|
| eps=0 | 1.85 MB | 2.010 | 无损 | 0 |
| eps=1/4 | 339 KB | 0.368 | 40.11 dB | 13 |
| eps=4/16 | 83 KB | 0.090 | 30.06 dB | 42 |
| eps=8/32 | 40 KB | 0.044 | 25.03 dB | 74 |

颗粒很重的特写对所有 RGB 无损都贵（这里约 12 bpp）。AV1 lossless YUV444
仍比 ZV RGB 大约小 1.25×。硬件 ASIC 解码不在范围内；软件解码在 Apple
Silicon 上对 YCoCg-R、Haar、平面拆合、以及界内 MV 拷贝走 NEON。热点仍是
串行 rANS + `PREDNFX`。

## 限制

- 视频为 8-bit RGB（v17）或 10-bit RGB（v18）；音频为 16-bit PCM。
- v17/v18 eps=0 为 RGB 严格无损；eps>0 为有界残差量化。v16 及更早最大 ±5 RGB。
- 音频严格无损。ZA v2 在上表 10 秒音乐片段上与 FLAC -8 持平；其它素材
  仍可能落后。
- `ZVM1` 可封装一条 ZV 视频轨和可选 ZA 音频（输出文件名以 `.zvm` 结尾时
  `encoder.py` 会写入）。
- 感知有损 RD 在同等 bpp 下仍落后于 AV1 / HEVC；上表曲线已经单调、能用。
  没有硬件 ASIC 解码。相机 RGB 无损已小于 FFV1 与 x265 lossless RGB；
  AV1 lossless YUV444 仍然更小。

## License

MIT，见 [LICENSE](LICENSE)。
