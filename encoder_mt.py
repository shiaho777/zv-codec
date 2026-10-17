#!/usr/bin/env python3
"""v17 encode with C-level plane + stripe threads.

The frame encoder (zv_encode_frame_v3s / v3sq) already runs Y/Co/Cg on
three pthreads and, when stripes > 1, splits each plane into independent
substreams.  This wrapper just sets the stripe count and calls encoder.py.
"""
import sys
from encoder import encode

DEFAULT_STRIPES = 3
ZV_VERSION = 17

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: python encoder_mt.py <input_video> <output.zv|.zvm> [num_stripes]")
        sys.exit(1)
    n = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_STRIPES
    encode(sys.argv[1], sys.argv[2], n_stripes=n)
