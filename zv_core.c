
#if defined(__STDC__) && !defined(__STDC_NO_FP_CONTRACT__)
#pragma STDC FP_CONTRACT OFF
#endif
#include <stdint.h>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Debug-env cache: getenv() serializes on a libc lock and was being
   evaluated per decoded symbol/pixel - it dominated the decode hot
   path.  Resolve every flag once, then read cached bytes. */
static struct {
    uint8_t zrs, zd2, zd, zwn, zop, zhi, zr, zpx, zpix, zoldhi, zlv,
            zerr, zcfl, zseq, zseg, zpx2, zps2, zps, zpc, znpt, znops,
            zhi2, zbrk2, zbrk, znrs, znllp, znoup, znfg, znpal, znmv,
            znic, znpx, zt, ztxv, zntx, zntxl, ztxw, ztxf, ztxc, zmv, zedp, zrl0, znopflt, znopb, znopc,
            znopre, zpre, zprelog, znof0, zlite0, zpoi, zap24, z1p, zrh, zacc, zwin, zwd, zmh, znwr, znxp, ready;
    long ztxk, ztxb, ztxct;
    int zab, zns, zrw, zsm, znsl, znsc, zdmax, zdstep, zngmv, znrep, zsk, zhib;
    char *zlog, *zrdump, *zspg;
} ZE_;
static void zenv_init(void) {
    if (ZE_.ready) return;
    ZE_.zrs    = getenv("ZRS")    != NULL;
    ZE_.zd2    = getenv("ZD2")    != NULL;
    ZE_.zd     = getenv("ZD")     != NULL;
    ZE_.zwn    = getenv("ZWN")    != NULL;
    ZE_.zop    = getenv("ZOP")    != NULL;
    ZE_.zhi    = getenv("ZHI")    != NULL;
    ZE_.zr     = getenv("ZR")     != NULL;
    ZE_.zpx    = getenv("ZPX")    != NULL;
    ZE_.zpix   = getenv("ZPIX")   != NULL;
    ZE_.zoldhi = getenv("ZOLDHI") != NULL;
    ZE_.zlv    = getenv("ZLV")    != NULL;
    ZE_.zerr   = getenv("ZERR")   != NULL;
    ZE_.zcfl   = getenv("ZCFL")   != NULL;
    ZE_.zseq   = getenv("ZSEQ")   != NULL;
    ZE_.zseg   = getenv("ZSEG")   != NULL;
    ZE_.zpx2   = getenv("ZPX2")   != NULL;
    ZE_.zps2   = getenv("ZPS2")   != NULL;
    ZE_.zps    = getenv("ZPS")    != NULL;
    ZE_.zpc    = getenv("ZPC")    != NULL;
    ZE_.znpt   = getenv("ZNPT")   != NULL;
    ZE_.znops  = getenv("ZNOPS")  != NULL;
    ZE_.zhi2   = getenv("ZHI2")   != NULL;
    ZE_.zbrk2  = getenv("ZBRK2")  != NULL;
    ZE_.zbrk   = getenv("ZBRK")   != NULL;
    ZE_.znrs   = getenv("ZNRS")   != NULL;
    ZE_.zdmax  = getenv("ZDMAX")  ? atoi(getenv("ZDMAX")) : -1;
    ZE_.zdstep = getenv("ZDSTEP") ? atoi(getenv("ZDSTEP")) : -1;
    ZE_.zngmv  = getenv("ZNGMV") != NULL;
    ZE_.znrep  = getenv("ZNREP") != NULL;
    ZE_.znllp  = getenv("ZNLLP")  != NULL;
    ZE_.znoup  = getenv("ZNOUP")  != NULL;
    ZE_.znfg   = getenv("ZNFG")   != NULL;
    ZE_.znpal  = getenv("ZNPAL")  != NULL;
    ZE_.znmv   = getenv("ZNMV")   != NULL;
    ZE_.znic   = getenv("ZNIC")   != NULL;
    ZE_.znpx   = getenv("ZNPS")   != NULL;   /* pred-family sims */
    ZE_.zt     = getenv("ZT")     != NULL;
    ZE_.zspg   = getenv("ZSPG");
    ZE_.zlog   = getenv("ZLOG");
    ZE_.zrdump = getenv("ZRDUMP");
    ZE_.ztxv   = getenv("ZTXV")  != NULL;
    ZE_.zntx   = getenv("ZNTX")  != NULL;
    ZE_.zntxl  = getenv("ZNTXL") != NULL;
    ZE_.zedp   = getenv("ZEDP")   != NULL;
    ZE_.zrl0   = getenv("ZRL0")  != NULL;
    ZE_.znopflt= getenv("ZNOPFLT") != NULL;
    ZE_.znopb  = getenv("ZNOPB")  != NULL;
    ZE_.znopc  = getenv("ZNOPC")  != NULL;
    ZE_.znopre = getenv("ZNOPRE") != NULL;
    /* default 80: emit=0 subtree 80% over the leaf vetoes the full
       transactional probe.  25 was a 2.2x speed win but cost ~1% on
       live camera; 80 keeps nearly all split winners (measured +0.4%
       vs full probe) and still cuts the 99% losing-probe tail.
       ZPRE=-1 restores always-full-probe (uint8 wrap to 255). */
    ZE_.zpre   = getenv("ZPRE") ? atoi(getenv("ZPRE")) : 80;
    ZE_.zprelog= getenv("ZPRELOG") != NULL;
    ZE_.znof0  = getenv("ZNOF0")  != NULL;
    ZE_.zlite0 = getenv("ZLITE0") != NULL;
    ZE_.zpoi   = getenv("ZPOI")   != NULL;
    ZE_.zap24  = getenv("ZAP24")  != NULL;
    ZE_.z1p    = getenv("Z1P")    != NULL;
    ZE_.ztxw   = getenv("ZTXW")  != NULL;
    ZE_.ztxf   = getenv("ZTXF")  != NULL;
    ZE_.ztxc   = getenv("ZTXC")  != NULL;
    ZE_.zmv    = getenv("ZMV")   != NULL;
    ZE_.zrh    = getenv("ZRH")   != NULL;
    ZE_.zacc   = getenv("ZACC")  != NULL;
    ZE_.zwin   = getenv("ZWIN")  != NULL;
    ZE_.zwd    = getenv("ZWD")   != NULL;
    ZE_.zmh    = getenv("ZMH")   != NULL;
    ZE_.znwr   = getenv("ZNWR")  != NULL;
    ZE_.znxp   = getenv("ZNXP")  != NULL;
    { const char *e;
      e = getenv("ZTXK");  ZE_.ztxk  = e ? atol(e) : -1;
      e = getenv("ZTXB");  ZE_.ztxb  = e ? atol(e) : -1;
      e = getenv("ZTXCT"); ZE_.ztxct = e ? atol(e) : -1;
      e = getenv("ZAB");   ZE_.zab   = e ? atoi(e) : 110;
      e = getenv("ZNS");   ZE_.zns   = e ? atoi(e) : 1;
      e = getenv("ZRW");   ZE_.zrw   = e ? atoi(e) : 16;
      e = getenv("ZSM");   ZE_.zsm   = e ? atoi(e) : 4096;
      e = getenv("ZNSL");  ZE_.znsl  = e ? atoi(e) : 3;
      e = getenv("ZNSC");  ZE_.znsc  = e ? atoi(e) : 2;
      e = getenv("ZSK");   ZE_.zsk   = e ? atoi(e) : 0;
      e = getenv("ZHIB");  ZE_.zhib  = e ? atoi(e) : 9; }
    ZE_.ready  = 1;
}
#define ZE(x)  (ZE_.ready || (zenv_init(), 0), ZE_.x)
#define ZEP(x) (ZE_.ready || (zenv_init(), 0), ZE_.x)

/* Per-thread scratch arenas: the frame entry points used to malloc/free
   several MB of planes and tables every call - visible in profiles as
   madvise churn.  Each slot grows once to the largest size seen and is
   reused; slots are distinct so nested calls never share a buffer. */
enum { TLS_PLANES6, TLS_PLANES9, TLS_VOPS, TLS_C420, TLS_ENCRECS,
       TLS_PB, TLS_SST, TLS_TDP, TLS_TDC, TLS_EDSTK, TLS_N };
_Atomic long ztile_ns;
_Atomic long zsec[6];
_Atomic long ztxw, ztxl, ztxo;
_Atomic long zcalls_e;
#define ZT_ACC(slot, t0) do { if (ZE(zt)) { \
        struct timespec zt_n; clock_gettime(CLOCK_MONOTONIC, &zt_n); \
        atomic_fetch_add(&zsec[slot], \
            (zt_n.tv_sec - (t0).tv_sec) * 1000000000L + (zt_n.tv_nsec - (t0).tv_nsec)); \
        (t0) = zt_n; } } while (0)
static _Thread_local void  *tls_a[TLS_N];
static _Thread_local size_t tls_c[TLS_N];
static void *tls_get(int i, size_t n) {
    if (tls_c[i] < n) {
        free(tls_a[i]);
        tls_a[i] = malloc(n);
        tls_c[i] = tls_a[i] ? n : 0;
    }
    return tls_a[i];
}

typedef struct {
    uint8_t *buf;
    size_t   cap, pos;
    uint64_t acc;
    int      nbits;
} bw_t;

static void bw_init(bw_t *bw, uint8_t *buf, size_t cap) {
    bw->buf = buf; bw->cap = cap;
    bw->pos = 0; bw->acc = 0; bw->nbits = 0;
}

static void bw_put(bw_t *bw, uint64_t v, int n) {
    if (n <= 0) return;
    uint64_t m = (n == 64) ? ~(uint64_t)0 : (((uint64_t)1 << n) - 1);
    bw->acc = (bw->acc << n) | (v & m);
    bw->nbits += n;
    while (bw->nbits >= 8) {
        bw->nbits -= 8;
        bw->buf[bw->pos++] = (uint8_t)(bw->acc >> bw->nbits);
    }
    bw->acc &= (bw->nbits == 0) ? 0 : ((uint64_t)1 << bw->nbits) - 1;
}

static size_t bw_finish(bw_t *bw) {
    if (bw->nbits > 0) {
        bw->buf[bw->pos++] = (uint8_t)(bw->acc << (8 - bw->nbits));
        bw->acc = 0; bw->nbits = 0;
    }
    return bw->pos;
}

typedef struct {
    const uint8_t *buf;
    size_t   size, pos;
    uint64_t acc;
    int      nbits;
} br_t;

static void br_init(br_t *br, const uint8_t *buf, size_t size) {
    br->buf = buf; br->size = size;
    br->pos = 0; br->acc = 0; br->nbits = 0;
}

static uint64_t br_get(br_t *br, int n) {
    if (n <= 0) return 0;
    while (br->nbits < n) {
        uint8_t b = (br->pos < br->size) ? br->buf[br->pos++] : 0;
        br->acc = (br->acc << 8) | b;
        br->nbits += 8;
    }
    br->nbits -= n;
    uint64_t m = (n == 64) ? ~(uint64_t)0 : (((uint64_t)1 << n) - 1);
    return (br->acc >> br->nbits) & m;
}

typedef struct {
    uint32_t low, high;
    uint32_t pending;
    bw_t    *bw;
} ac_enc_t;

typedef struct {
    uint32_t low, high, code;
    br_t    *br;
} ac_dec_t;

#define AC_QTR    0x40000000u
#define AC_HALF   0x80000000u
#define AC_3Q     0xC0000000u
#define AC_BYPASS 0x8000u

static void ac_init_enc(ac_enc_t *e, bw_t *bw) {
    e->low = 0; e->high = 0xFFFFFFFFu; e->pending = 0; e->bw = bw;
}

static void ac_flush_pending(ac_enc_t *e, int bit) {
    bw_put(e->bw, (uint64_t)bit, 1);
    uint64_t inv = (uint64_t)(bit ^ 1);
    while (e->pending > 0) { bw_put(e->bw, inv, 1); e->pending--; }
}

static void ac_enc_bit(ac_enc_t *e, int bit, uint32_t p0) {
    uint64_t range = (uint64_t)(e->high - e->low) + 1;
    uint64_t split = (range * (uint64_t)p0) >> 16;
    if (split < 1) split = 1;
    if (split >= range) split = range - 1;
    uint32_t bound = e->low + (uint32_t)split;
    if (bit == 0) e->high = bound - 1;
    else          e->low  = bound;
    for (;;) {
        if (e->high < AC_HALF) {
            ac_flush_pending(e, 0);
            e->low <<= 1; e->high = (e->high << 1) | 1u;
        } else if (e->low >= AC_HALF) {
            ac_flush_pending(e, 1);
            e->low = (e->low - AC_HALF) << 1;
            e->high = ((e->high - AC_HALF) << 1) | 1u;
        } else if (e->low >= AC_QTR && e->high < AC_3Q) {
            e->pending++;
            e->low = (e->low - AC_QTR) << 1;
            e->high = ((e->high - AC_QTR) << 1) | 1u;
        } else break;
    }
}

static void ac_finish_enc(ac_enc_t *e) {
    e->pending++;
    if (e->low < AC_QTR) ac_flush_pending(e, 0);
    else                 ac_flush_pending(e, 1);
}

static void ac_init_dec(ac_dec_t *d, br_t *br) {
    d->low = 0; d->high = 0xFFFFFFFFu; d->br = br;
    d->code = 0;
    for (int i = 0; i < 32; i++) d->code = (d->code << 1) | (uint32_t)br_get(br, 1);
}

static int ac_dec_bit(ac_dec_t *d, uint32_t p0) {
    uint64_t range = (uint64_t)(d->high - d->low) + 1;
    uint64_t split = (range * (uint64_t)p0) >> 16;
    if (split < 1) split = 1;
    if (split >= range) split = range - 1;
    uint32_t bound = d->low + (uint32_t)split;
    int bit;
    if (d->code < bound) { bit = 0; d->high = bound - 1; }
    else                 { bit = 1; d->low  = bound;     }
    for (;;) {
        if (d->high < AC_HALF) {
            d->low <<= 1; d->high = (d->high << 1) | 1u;
            d->code = (d->code << 1) | (uint32_t)br_get(d->br, 1);
        } else if (d->low >= AC_HALF) {
            d->low = (d->low - AC_HALF) << 1;
            d->high = ((d->high - AC_HALF) << 1) | 1u;
            d->code = ((d->code - AC_HALF) << 1) | (uint32_t)br_get(d->br, 1);
        } else if (d->low >= AC_QTR && d->high < AC_3Q) {
            d->low = (d->low - AC_QTR) << 1;
            d->high = ((d->high - AC_QTR) << 1) | 1u;
            d->code = ((d->code - AC_QTR) << 1) | (uint32_t)br_get(d->br, 1);
        } else break;
    }
    return bit;
}

static inline void bm_upd(uint16_t *p0, int bit) {
    if (bit == 0) *p0 = (uint16_t)(*p0 + ((0xFFFFu - *p0) >> 5));
    else          *p0 = (uint16_t)(*p0 - (*p0 >> 5));
    if (*p0 < 1u)      *p0 = 1u;
    if (*p0 > 0xFFFEu) *p0 = 0xFFFEu;
}

#define UNARY_LIMIT 24

static void bypass_eg0_put(ac_enc_t *e, uint32_t v) {
    uint32_t m = v + 1;
    int L = 0;
    uint32_t t = m >> 1;
    while (t) { t >>= 1; L++; }
    for (int i = 0; i < L; i++) ac_enc_bit(e, 0, AC_BYPASS);
    ac_enc_bit(e, 1, AC_BYPASS);
    for (int i = L - 1; i >= 0; i--) ac_enc_bit(e, (m >> i) & 1u, AC_BYPASS);
}

static uint32_t bypass_eg0_get(ac_dec_t *d) {
    int L = 0;
    while (ac_dec_bit(d, AC_BYPASS) == 0) { L++; if (L > 31) break; }
    uint32_t m = 1;
    for (int i = 0; i < L; i++) m = (m << 1) | (uint32_t)ac_dec_bit(d, AC_BYPASS);
    return m - 1;
}

static void sym_put(ac_enc_t *e, uint32_t u, int k, uint16_t *mdl) {
    uint32_t q = u >> k;
    uint32_t rem = (k > 0) ? (u & ((1u << k) - 1)) : 0u;
    int i = 0;
    while (q > 0 && i < UNARY_LIMIT) {
        ac_enc_bit(e, 1, mdl[i]); bm_upd(&mdl[i], 1);
        i++; q--;
    }
    if (i < UNARY_LIMIT) {
        ac_enc_bit(e, 0, mdl[i]); bm_upd(&mdl[i], 0);
    } else {
        bypass_eg0_put(e, q);
    }
    for (int b = k - 1; b >= 0; b--) ac_enc_bit(e, (rem >> b) & 1u, AC_BYPASS);
}

static uint32_t sym_get(ac_dec_t *d, int k, uint16_t *mdl) {
    uint32_t q = 0;
    int i = 0;
    while (i < UNARY_LIMIT) {
        int bit = ac_dec_bit(d, mdl[i]); bm_upd(&mdl[i], bit);
        if (bit == 0) break;
        q++; i++;
    }
    if (i == UNARY_LIMIT) q += bypass_eg0_get(d);
    uint32_t rem = 0;
    for (int b = 0; b < k; b++) rem = (rem << 1) | (uint32_t)ac_dec_bit(d, AC_BYPASS);
    return (q << k) | rem;
}

static inline uint32_t zigzag_enc(int v) {
    return (v >= 0) ? ((uint32_t)v << 1) : (((uint32_t)(-v) << 1) - 1);
}
static inline int zigzag_dec(uint32_t u) {
    return (u & 1u) ? -(int)((u >> 1) + 1) : (int)(u >> 1);
}

static inline void rgb_to_ycocgr(int R, int G, int B, int *Y, int *Co, int *Cg) {
    int co = R - B;
    int t  = B + (co >> 1);
    int cg = G - t;
    int y  = t + (cg >> 1);
    *Y = y; *Co = co; *Cg = cg;
}

static inline void ycocgr_to_rgb(int Y, int Co, int Cg, int *R, int *G, int *B) {
    int t = Y  - (Cg >> 1);
    int g = Cg + t;
    int b = t  - (Co >> 1);
    int r = Co + b;
    *R = r; *G = g; *B = b;
}

void rgb_to_ycocgr_frame(const uint8_t *rgb, int16_t *ycocg, int w, int h) {
    size_t n = (size_t)w * h, i = 0;
#if defined(__ARM_NEON)
    int16x8_t one = vdupq_n_s16(1), two = vdupq_n_s16(2);
    for (; i + 8 <= n; i += 8) {
        uint8x8x3_t v = vld3_u8(rgb + i * 3);
        int16x8_t R = vreinterpretq_s16_u16(vmovl_u8(v.val[0]));
        int16x8_t G = vreinterpretq_s16_u16(vmovl_u8(v.val[1]));
        int16x8_t B = vreinterpretq_s16_u16(vmovl_u8(v.val[2]));
        int16x8_t Co = vsubq_s16(R, B);
        int16x8_t t  = vaddq_s16(B, vshrq_n_s16(Co, 1));
        int16x8_t Cg = vsubq_s16(G, t);
        int16x8_t Y  = vaddq_s16(t, vshrq_n_s16(Cg, 1));
        int16x8_t Coa = vaddq_s16(Co, vbslq_s16(vcgeq_s16(Co, vdupq_n_s16(0)), two,
                                                vnegq_s16(two)));
        int16x8_t Cga = vaddq_s16(Cg, vbslq_s16(vcgeq_s16(Cg, vdupq_n_s16(0)), two,
                                                vnegq_s16(two)));
        int16x8x3_t o;
        o.val[0] = vshrq_n_s16(vaddq_s16(Y, one), 1);
        o.val[1] = vshrq_n_s16(Coa, 2);
        o.val[2] = vshrq_n_s16(Cga, 2);
        vst3q_s16(ycocg + i * 3, o);
    }
#endif
    for (; i < n; i++) {
        int Y, Co, Cg;
        rgb_to_ycocgr(rgb[i*3], rgb[i*3+1], rgb[i*3+2], &Y, &Co, &Cg);
        ycocg[i*3]   = (int16_t)((Y  + 1) >> 1);
        ycocg[i*3+1] = (int16_t)((Co + (Co >= 0 ? 2 : -2)) >> 2);
        ycocg[i*3+2] = (int16_t)((Cg + (Cg >= 0 ? 2 : -2)) >> 2);
    }
}

void ycocgr_to_rgb_frame(const int16_t *ycocg, uint8_t *rgb, int w, int h) {
    size_t n = (size_t)w * h, i = 0;
#if defined(__ARM_NEON)
    int16x8_t z = vdupq_n_s16(0), hi = vdupq_n_s16(255);
    for (; i + 8 <= n; i += 8) {
        int16x8x3_t v = vld3q_s16(ycocg + i * 3);
        int16x8_t Y  = vshlq_n_s16(v.val[0], 1);
        int16x8_t Co = vshlq_n_s16(v.val[1], 2);
        int16x8_t Cg = vshlq_n_s16(v.val[2], 2);
        int16x8_t t = vsubq_s16(Y, vshrq_n_s16(Cg, 1));
        int16x8_t G = vaddq_s16(Cg, t);
        int16x8_t B = vsubq_s16(t, vshrq_n_s16(Co, 1));
        int16x8_t R = vaddq_s16(Co, B);
        uint8x8x3_t o;
        o.val[0] = vqmovun_s16(vmaxq_s16(z, vminq_s16(R, hi)));
        o.val[1] = vqmovun_s16(vmaxq_s16(z, vminq_s16(G, hi)));
        o.val[2] = vqmovun_s16(vmaxq_s16(z, vminq_s16(B, hi)));
        vst3_u8(rgb + i * 3, o);
    }
#endif
    for (; i < n; i++) {
        int Y  = ycocg[i*3]   << 1;
        int Co = ycocg[i*3+1] << 2;
        int Cg = ycocg[i*3+2] << 2;
        int R, G, B;
        ycocgr_to_rgb(Y, Co, Cg, &R, &G, &B);
        if (R < 0) R = 0; else if (R > 255) R = 255;
        if (G < 0) G = 0; else if (G > 255) G = 255;
        if (B < 0) B = 0; else if (B > 255) B = 255;
        rgb[i*3] = (uint8_t)R; rgb[i*3+1] = (uint8_t)G; rgb[i*3+2] = (uint8_t)B;
    }
}

/* v17: reversible YCoCg-R, no extra luma/chroma quant.  8-bit RGB round-trips
   bit-exactly when the plane codec is lossless (eps=0).  Co/Cg occupy
   [-255,255]; Y occupies [0,255].  Near-lossless residual quant still
   applies in this full-scale domain, so PSNR is no longer capped at ~43 dB. */
void rgb_to_ycocgr_full_frame(const uint8_t *rgb, int16_t *ycocg, int w, int h) {
    size_t n = (size_t)w * h, i = 0;
#if defined(__ARM_NEON)
    for (; i + 8 <= n; i += 8) {
        uint8x8x3_t v = vld3_u8(rgb + i * 3);
        int16x8_t R = vreinterpretq_s16_u16(vmovl_u8(v.val[0]));
        int16x8_t G = vreinterpretq_s16_u16(vmovl_u8(v.val[1]));
        int16x8_t B = vreinterpretq_s16_u16(vmovl_u8(v.val[2]));
        int16x8_t Co = vsubq_s16(R, B);
        int16x8_t t  = vaddq_s16(B, vshrq_n_s16(Co, 1));
        int16x8_t Cg = vsubq_s16(G, t);
        int16x8_t Y  = vaddq_s16(t, vshrq_n_s16(Cg, 1));
        int16x8x3_t o;
        o.val[0] = Y; o.val[1] = Co; o.val[2] = Cg;
        vst3q_s16(ycocg + i * 3, o);
    }
#endif
    for (; i < n; i++) {
        int Y, Co, Cg;
        rgb_to_ycocgr(rgb[i*3], rgb[i*3+1], rgb[i*3+2], &Y, &Co, &Cg);
        ycocg[i*3]   = (int16_t)Y;
        ycocg[i*3+1] = (int16_t)Co;
        ycocg[i*3+2] = (int16_t)Cg;
    }
}

void ycocgr_to_rgb_full_frame(const int16_t *ycocg, uint8_t *rgb, int w, int h) {
    size_t n = (size_t)w * h, i = 0;
#if defined(__ARM_NEON)
    int16x8_t z = vdupq_n_s16(0), hi = vdupq_n_s16(255);
    for (; i + 8 <= n; i += 8) {
        int16x8x3_t v = vld3q_s16(ycocg + i * 3);
        int16x8_t Y = v.val[0], Co = v.val[1], Cg = v.val[2];
        int16x8_t t = vsubq_s16(Y, vshrq_n_s16(Cg, 1));
        int16x8_t G = vaddq_s16(Cg, t);
        int16x8_t B = vsubq_s16(t, vshrq_n_s16(Co, 1));
        int16x8_t R = vaddq_s16(Co, B);
        uint8x8x3_t o;
        o.val[0] = vqmovun_s16(vmaxq_s16(z, vminq_s16(R, hi)));
        o.val[1] = vqmovun_s16(vmaxq_s16(z, vminq_s16(G, hi)));
        o.val[2] = vqmovun_s16(vmaxq_s16(z, vminq_s16(B, hi)));
        vst3_u8(rgb + i * 3, o);
    }
#endif
    for (; i < n; i++) {
        int R, G, B;
        ycocgr_to_rgb(ycocg[i*3], ycocg[i*3+1], ycocg[i*3+2], &R, &G, &B);
        if (R < 0) R = 0; else if (R > 255) R = 255;
        if (G < 0) G = 0; else if (G > 255) G = 255;
        if (B < 0) B = 0; else if (B > 255) B = 255;
        rgb[i*3] = (uint8_t)R; rgb[i*3+1] = (uint8_t)G; rgb[i*3+2] = (uint8_t)B;
    }
}

/* 10-bit RGB (0..1023) in uint16 lanes.  Same reversible lift as v17; the
   plane codec already stores int16 Y/Co/Cg so 10-bit fits without a new
   residual path.  File version 18. */
void rgb16_to_ycocgr_full_frame(const uint16_t *rgb, int16_t *ycocg, int w, int h) {
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
        int Y, Co, Cg;
        rgb_to_ycocgr(rgb[i*3], rgb[i*3+1], rgb[i*3+2], &Y, &Co, &Cg);
        ycocg[i*3]   = (int16_t)Y;
        ycocg[i*3+1] = (int16_t)Co;
        ycocg[i*3+2] = (int16_t)Cg;
    }
}

void ycocgr_to_rgb16_full_frame(const int16_t *ycocg, uint16_t *rgb,
                                int w, int h, int maxc) {
    if (maxc < 1) maxc = 1023;
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
        int R, G, B;
        ycocgr_to_rgb(ycocg[i*3], ycocg[i*3+1], ycocg[i*3+2], &R, &G, &B);
        if (R < 0) R = 0; else if (R > maxc) R = maxc;
        if (G < 0) G = 0; else if (G > maxc) G = maxc;
        if (B < 0) B = 0; else if (B > maxc) B = maxc;
        rgb[i*3] = (uint16_t)R; rgb[i*3+1] = (uint16_t)G; rgb[i*3+2] = (uint16_t)B;
    }
}

void zv_encode_set_stripes(int n) {
    zenv_init();
    if (n < 1) n = 1;
    if (n > 10) n = 10;
    ZE_.zns = n;
}

int zv_encode_get_stripes(void) {
    zenv_init();
    return (int)ZE_.zns;
}

static void deinterleave_planes(const int16_t *in, int16_t *Y, int16_t *Co, int16_t *Cg,
                                int w, int h) {
    size_t n = (size_t)w * h, i = 0;
#if defined(__ARM_NEON)
    for (; i + 8 <= n; i += 8) {
        int16x8x3_t v = vld3q_s16(in + i * 3);
        vst1q_s16(Y  + i, v.val[0]);
        vst1q_s16(Co + i, v.val[1]);
        vst1q_s16(Cg + i, v.val[2]);
    }
#endif
    for (; i < n; i++) {
        Y [i] = in[i*3 + 0];
        Co[i] = in[i*3 + 1];
        Cg[i] = in[i*3 + 2];
    }
}

static void interleave_planes(const int16_t *Y, const int16_t *Co, const int16_t *Cg,
                              int16_t *out, int w, int h) {
    size_t n = (size_t)w * h, i = 0;
#if defined(__ARM_NEON)
    for (; i + 8 <= n; i += 8) {
        int16x8x3_t v;
        v.val[0] = vld1q_s16(Y  + i);
        v.val[1] = vld1q_s16(Co + i);
        v.val[2] = vld1q_s16(Cg + i);
        vst3q_s16(out + i * 3, v);
    }
#endif
    for (; i < n; i++) {
        out[i*3 + 0] = Y [i];
        out[i*3 + 1] = Co[i];
        out[i*3 + 2] = Cg[i];
    }
}

static inline int med_pred(int a, int b, int c) {
    int mx = a > b ? a : b, mn = a < b ? a : b;
    if (c >= mx) return mn;
    if (c <= mn) return mx;
    return a + b - c;
}

/* chroma-420 helpers: a plane qualifies when every 2x2 block is constant
   (captured content is natively 4:2:0 - coding it at quarter resolution
   stays lossless).  dn2 keeps the top-left sample; both sides use the
   same rule so the decimated reference planes stay bit-identical. */
static int plane_is_420(const int16_t *p, int w, int h)
{
    if ((w & 1) || (h & 1) || w < 2 || h < 2) return 0;
    for (int y = 0; y < h; y += 2) {
        const int16_t *r0 = p + (size_t)y * w, *r1 = r0 + w;
        for (int x = 0; x < w; x += 2)
            if (r0[x+1] != r0[x] || r1[x] != r0[x] || r1[x+1] != r0[x])
                return 0;
    }
    return 1;
}
static void plane_dn2(int16_t *d, const int16_t *s, int w, int h)
{
    int w2 = w >> 1, h2 = h >> 1;
    for (int y = 0; y < h2; y++) {
        const int16_t *r = s + ((size_t)y << 1) * w;
        for (int x = 0; x < w2; x++) d[(size_t)y * w2 + x] = r[x << 1];
    }
}
static void plane_up2(int16_t *d, const int16_t *s, int w, int h)
{
    int w2 = w >> 1;
    for (int y = 0; y < h; y++) {
        const int16_t *r = s + (size_t)(y >> 1) * w2;
        for (int x = 0; x < w; x++) d[(size_t)y * w + x] = r[x >> 1];
    }
}

#define NSPAT 8

static inline void spat_preds(int a, int b, int c, int d, int aa, int uu,
                              int preds[NSPAT]) {
    preds[0] = med_pred(a, b, c);
    preds[1] = a + b - c;
    preds[2] = (a + b + 1) >> 1;
    preds[3] = a;
    preds[4] = b;
    int dh = c - a; if (dh < 0) dh = -dh;
    int dv = c - b; if (dv < 0) dv = -dv;
    int g;
    if      (dh - dv > 16) g = b;
    else if (dv - dh > 16) g = a;
    else                   g = a + b - c + ((d - b) >> 2);
    preds[5] = g;
    preds[6] = b + b - uu;   /* vertical gradient extrapolation */
    preds[7] = a + a - aa;   /* horizontal gradient extrapolation */
}

static inline int q_grad(int g) {
    int s = 1;
    if (g < 0) { s = -1; g = -g; }
    int q;
    if      (g == 0)  q = 0;
    else if (g <= 3)  q = 1;
    else if (g <= 7)  q = 2;
    else if (g <= 20) q = 3;
    else              q = 4;
    return s * q;
}

#define N_CTX 405

static inline int ctx_index(int q1, int q2, int q3, int *flip) {
    *flip = 0;
    int lead = q1; if (lead == 0) lead = q2; if (lead == 0) lead = q3;
    if (lead < 0) { *flip = 1; q1 = -q1; q2 = -q2; q3 = -q3; }
    return (q1 * 9 + (q2 + 4)) * 9 + (q3 + 4);
}

#define RESET_N 64

typedef struct {
    int32_t As[N_CTX][NSPAT];
    int32_t At[N_CTX];
    int32_t N [N_CTX];
    int32_t A_run, N_run;
} pstate_t;

/* per-tile cache of the purely-spatial per-pixel products - ctx index,
   gradient bucket, and the 8 spatial predictors.  Valid only while
   recon == NULL (lossless): with quantization the recon plane evolves
   per candidate and none of this is shareable.  ctxf < 0 = empty. */
typedef struct {
    int16_t ctxf;
    int16_t gbb;
    int32_t sp[NSPAT];
} pxcache_t;

static void pstate_init(pstate_t *s) {
    for (int i = 0; i < N_CTX; i++) {
        for (int p = 0; p < NSPAT; p++) s->As[i][p] = 4;
        s->At[i] = 4;
        s->N [i] = 1;
    }
    s->A_run = 4; s->N_run = 1;
}

static inline int compute_k(int32_t A, int32_t N) {
    int k = 0;
    while ((N << k) < A && k < 24) k++;
    return k;
}

static inline void update_ctx(pstate_t *s, int ctx,
                              const int abs_s[NSPAT], int abs_t) {
#if defined(__ARM_NEON)
    int32x4_t a0 = vld1q_s32(s->As[ctx]);
    int32x4_t a1 = vld1q_s32(s->As[ctx] + 4);
    a0 = vaddq_s32(a0, vld1q_s32(abs_s));
    a1 = vaddq_s32(a1, vld1q_s32(abs_s + 4));
    vst1q_s32(s->As[ctx], a0);
    vst1q_s32(s->As[ctx] + 4, a1);
#else
    for (int p = 0; p < NSPAT; p++) s->As[ctx][p] += abs_s[p];
#endif
    s->At[ctx] += abs_t;
    s->N[ctx]++;
    if (s->N[ctx] >= RESET_N) {
        for (int p = 0; p < NSPAT; p++) s->As[ctx][p] >>= 1;
        s->At[ctx] >>= 1;
        s->N [ctx] >>= 1;
    }
}

static inline void update_run(pstate_t *s, int run) {
    s->A_run += run; s->N_run++;
    if (s->N_run >= RESET_N) { s->A_run >>= 1; s->N_run >>= 1; }
}

/* candidate-eval undo: ladder candidates mutate the live pstate in
   place instead of evaluating on a 16KB struct clone.  A sim touches
   only the ctx rows its pixels actually visit (~tens of rows), so a
   losing candidate rolls back under a KB of state.  The winner's
   evolved state is snapshotted to bS before rollback, preserving the
   exact old clone-then-commit semantics. */
typedef struct {
    uint32_t stamp[N_CTX];            /* epoch tag per ctx (== epoch => logged) */
    uint32_t epoch;
    int32_t  row[N_CTX][NSPAT + 2];   /* saved As[ctx][*] + At + N */
    uint16_t ctx[N_CTX];
    int      n;
    int32_t  a_run, n_run;            /* scalar slots saved at begin */
} pundo_t;

static inline void pu_begin(pundo_t *u, const pstate_t *s) {
    u->epoch++;                       /* stamp-allocation - no memset */
    if (!u->epoch) {                  /* wrapped: forget all stamps */
        memset(u->stamp, 0, sizeof u->stamp);
        u->epoch = 1;
    }
    u->n = 0;
    u->a_run = s->A_run; u->n_run = s->N_run;
}

static void pu_rollback(pundo_t *u, pstate_t *s) {
    for (int i = 0; i < u->n; i++) {
        int c = u->ctx[i];
        memcpy(s->As[c], u->row[i], NSPAT * sizeof(int32_t));
        s->At[c] = u->row[i][NSPAT];
        s->N[c]  = u->row[i][NSPAT + 1];
    }
    s->A_run = u->a_run; s->N_run = u->n_run;
}


/* armed while a lite (nested) txn is open: every pstate update on
   the bound state pointer is also first-touch-logged into this second
   undo, which is what the lite rollback restores from instead of a
   full 16KB snapshot.  The pointer guard matters: emit=0 probes inside
   a nested txn evolve a private Sc copy - their updates must not log
   Sc rows into the lite undo (rollback writes them into *Sio). */
static _Thread_local pundo_t  *zpu2;
static _Thread_local const pstate_t *zpu2_s;
/* set while a depth-0 emit=0 pre-probe evaluates children: restores the
   spatial-search candidates (MV/ICOPY) so the estimate tracks the real
   emit=1 cost closely enough to veto hopeless splits without killing
   winners */
static _Thread_local int zin_pre;

static inline void update_ctx_u(pstate_t *s, pundo_t *u, pundo_t *u2,
                                int ctx,
                                const int abs_s[NSPAT], int abs_t) {
    if (u && u->stamp[ctx] != u->epoch) {
        u->stamp[ctx] = u->epoch;
        memcpy(u->row[u->n], s->As[ctx], NSPAT * sizeof(int32_t));
        u->row[u->n][NSPAT]     = s->At[ctx];
        u->row[u->n][NSPAT + 1] = s->N[ctx];
        u->ctx[u->n++] = (uint16_t)ctx;
    }
    if (u2 && u2->stamp[ctx] != u2->epoch) {
        u2->stamp[ctx] = u2->epoch;
        memcpy(u2->row[u2->n], s->As[ctx], NSPAT * sizeof(int32_t));
        u2->row[u2->n][NSPAT]     = s->At[ctx];
        u2->row[u2->n][NSPAT + 1] = s->N[ctx];
        u2->ctx[u2->n++] = (uint16_t)ctx;
    }
    update_ctx(s, ctx, abs_s, abs_t);
}

typedef struct {
    uint16_t res[N_CTX][NSPAT + 1][UNARY_LIMIT];
    uint16_t run[UNARY_LIMIT];
    uint16_t run_flag;
} plane_models_t;

static void plane_models_init(plane_models_t *m) {
    for (int c = 0; c < N_CTX; c++)
        for (int p = 0; p < NSPAT + 1; p++)
            for (int i = 0; i < UNARY_LIMIT; i++) m->res[c][p][i] = 0x8000u;
    for (int i = 0; i < UNARY_LIMIT; i++) m->run[i] = 0x8000u;
    m->run_flag = 0x8000u;
}

static void encode_plane(const int16_t *prev, int16_t *curr,
                         int w, int h, ac_enc_t *ac)
{
    pstate_t S; pstate_init(&S);
    plane_models_t M; plane_models_init(&M);
    int has_prev = (prev != NULL);

    for (int y = 0; y < h; y++) {
        int16_t *row = curr + (size_t)y * w;
        const int16_t *top = y > 0 ? curr + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? curr + (size_t)(y - 2) * w : NULL;
        const int16_t *prow = has_prev ? prev + (size_t)y * w : NULL;

        int x = 0;
        while (x < w) {

            if (has_prev) {
                int can_run = (row[x] == prow[x]);
                ac_enc_bit(ac, can_run, M.run_flag);
                bm_upd(&M.run_flag, can_run);
                if (can_run) {
                    int run = 1;
                    while (x + run < w && row[x + run] == prow[x + run]) run++;
                    int kr = compute_k(S.A_run, S.N_run);
                    sym_put(ac, (uint32_t)(run - 1), kr, M.run);
                    update_run(&S, run);
                    x += run;
                    continue;
                }
            }

            int a = x > 0 ? row[x - 1] : (top ? top[x] : 0);
            int aa = x > 1 ? row[x - 2] : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d = (top && x < w - 1) ? top[x + 1] : b;

            int q1 = q_grad(d - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);

            int spreds[NSPAT];
            spat_preds(a, b, c, d, aa, uu, spreds);
            int rs[NSPAT];
            for (int p = 0; p < NSPAT; p++) rs[p] = row[x] - spreds[p];

            int rt = 0;
            int has_t = has_prev ? 1 : 0;
            if (has_t) rt = row[x] - prow[x];

            if (flip) {
                for (int p = 0; p < NSPAT; p++) rs[p] = -rs[p];
                rt = -rt;
            }

            int mode = 0;
            int32_t best_A = S.As[ctx][0];
            for (int p = 1; p < NSPAT; p++) {
                if (S.As[ctx][p] < best_A) { best_A = S.As[ctx][p]; mode = p; }
            }
            if (has_t && S.At[ctx] < best_A) {
                best_A = S.At[ctx]; mode = NSPAT;
            }
            int32_t best_N = S.N[ctx];

            int r = (mode == NSPAT) ? rt : rs[mode];
            int k = compute_k(best_A, best_N);
            sym_put(ac, zigzag_enc(r), k, M.res[ctx][mode]);

            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) abs_s[p] = rs[p] < 0 ? -rs[p] : rs[p];
            int abs_t = has_t ? (rt < 0 ? -rt : rt) : abs_s[0];
            update_ctx(&S, ctx, abs_s, abs_t);
            x++;
        }
    }
}

static void decode_plane(const int16_t *prev, int16_t *curr,
                         int w, int h, ac_dec_t *ac)
{
    pstate_t S; pstate_init(&S);
    plane_models_t M; plane_models_init(&M);
    int has_prev = (prev != NULL);

    for (int y = 0; y < h; y++) {
        int16_t *row = curr + (size_t)y * w;
        const int16_t *top = y > 0 ? curr + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? curr + (size_t)(y - 2) * w : NULL;
        const int16_t *prow = has_prev ? prev + (size_t)y * w : NULL;

        int x = 0;
        while (x < w) {
            if (has_prev) {
                int can_run = ac_dec_bit(ac, M.run_flag);
                bm_upd(&M.run_flag, can_run);
                if (can_run) {
                    int kr = compute_k(S.A_run, S.N_run);
                    uint32_t v = sym_get(ac, kr, M.run);
                    int run = (int)v + 1;
                    update_run(&S, run);
                    for (int i = 0; i < run; i++) row[x + i] = prow[x + i];
                    x += run;
                    continue;
                }
            }

            int a = x > 0 ? row[x - 1] : (top ? top[x] : 0);
            int aa = x > 1 ? row[x - 2] : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d = (top && x < w - 1) ? top[x + 1] : b;

            int q1 = q_grad(d - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);

            int spreds[NSPAT];
            spat_preds(a, b, c, d, aa, uu, spreds);

            int has_t = has_prev ? 1 : 0;

            int mode = 0;
            int32_t best_A = S.As[ctx][0];
            for (int p = 1; p < NSPAT; p++) {
                if (S.As[ctx][p] < best_A) { best_A = S.As[ctx][p]; mode = p; }
            }
            if (has_t && S.At[ctx] < best_A) {
                best_A = S.At[ctx]; mode = NSPAT;
            }
            int32_t best_N = S.N[ctx];

            int k = compute_k(best_A, best_N);
            uint32_t u = sym_get(ac, k, M.res[ctx][mode]);
            int r = zigzag_dec(u);
            if (flip) r = -r;

            int val;
            if (mode == NSPAT) val = prow[x] + r;
            else               val = spreds[mode] + r;
            row[x] = (int16_t)val;

            int rs[NSPAT];
            for (int p = 0; p < NSPAT; p++) rs[p] = val - spreds[p];
            int rt = has_t ? (val - prow[x]) : 0;
            if (flip) {
                for (int p = 0; p < NSPAT; p++) rs[p] = -rs[p];
                rt = -rt;
            }
            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) abs_s[p] = rs[p] < 0 ? -rs[p] : rs[p];
            int abs_t = has_t ? (rt < 0 ? -rt : rt) : abs_s[0];
            update_ctx(&S, ctx, abs_s, abs_t);
            x++;
        }
    }
}

int32_t zv_encode_frame(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                        int w, int h, uint8_t *out_buf, int32_t out_cap)
{
    if (out_cap < 1) return -1;

    size_t n = (size_t)w * h;
    int16_t *pY  = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *pCo = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *pCg = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cY  = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cCo = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cCg = (int16_t*)malloc(n * sizeof(int16_t));

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);
    deinterleave_planes(curr_ycocg, cY, cCo, cCg, w, h);

    out_buf[0] = 0;
    bw_t bw; bw_init(&bw, out_buf + 1, (size_t)out_cap - 1);
    ac_enc_t ac; ac_init_enc(&ac, &bw);

    encode_plane(prev_ycocg ? pY  : NULL, cY , w, h, &ac);
    encode_plane(prev_ycocg ? pCo : NULL, cCo, w, h, &ac);
    encode_plane(prev_ycocg ? pCg : NULL, cCg, w, h, &ac);

    ac_finish_enc(&ac);
    size_t written = bw_finish(&bw);

    free(pY); free(pCo); free(pCg);
    free(cY); free(cCo); free(cCg);
    return (int64_t)written + 1;
}

int32_t zv_decode_frame(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                        int w, int h, const uint8_t *in_buf, int32_t in_size)
{
    if (in_size < 1) return -1;

    size_t n = (size_t)w * h;
    int16_t *pY  = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *pCo = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *pCg = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cY  = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cCo = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cCg = (int16_t*)malloc(n * sizeof(int16_t));

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);

    br_t br; br_init(&br, in_buf + 1, (size_t)in_size - 1);
    ac_dec_t ac; ac_init_dec(&ac, &br);

    decode_plane(prev_ycocg ? pY  : NULL, cY , w, h, &ac);
    decode_plane(prev_ycocg ? pCo : NULL, cCo, w, h, &ac);
    decode_plane(prev_ycocg ? pCg : NULL, cCg, w, h, &ac);

    interleave_planes(cY, cCo, cCg, curr_ycocg, w, h);

    free(pY); free(pCo); free(pCg);
    free(cY); free(cCo); free(cCg);
    return (int64_t)br.pos + 1;
}

int32_t zv_encode_plane_stripe(const int16_t *prev_plane,
                               int16_t *curr_plane,
                               int w, int stripe_h,
                               uint8_t *out_buf, int32_t out_cap)
{
    bw_t bw; bw_init(&bw, out_buf, (size_t)out_cap);
    ac_enc_t ac; ac_init_enc(&ac, &bw);
    encode_plane(prev_plane, curr_plane, w, stripe_h, &ac);
    ac_finish_enc(&ac);
    return (int64_t)bw_finish(&bw);
}

int32_t zv_decode_plane_stripe(const int16_t *prev_plane,
                               int16_t *curr_plane,
                               int w, int stripe_h,
                               const uint8_t *in_buf, int32_t in_size)
{
    br_t br; br_init(&br, in_buf, (size_t)in_size);
    ac_dec_t ac; ac_init_dec(&ac, &br);
    decode_plane(prev_plane, curr_plane, w, stripe_h, &ac);
    return (int64_t)br.pos;
}

/* ===================== rANS entropy layer (v2) =====================
 *
 * Same symbol semantics as the binary-AC path above: per pixel the stream is
 *   [run flag] -> [run length | residual]
 * What changes is only the coder: one rANS state op per semantic symbol
 * instead of a unary+Rice decomposition into many adaptive bits.
 *
 * rANS is a stack (LIFO), while the pixel loop is causal, so each plane is
 * encoded in two passes: pass A collects (model, value) records in raster
 * order; pass B emits them in reverse inside fixed-size segments. Every
 * segment is an independent rANS stream ({u32 len}{lane states}{renorm
 * words}), so the decoder pops semantic symbols in forward order. CDFs are
 * frozen within a segment and renormalized between segments from adaptive
 * histograms, so no tables are ever transmitted.
 */

#define RANS_L        (1u << 31)
#define RANS_SCALE    12
#define RANS_TOT      (1u << RANS_SCALE)
#define RANS_LANES    4
#define SEG_MIN       128                    /* first segment capacity     */
#define SEG_SYMS      16384                  /* max segment capacity       */
#define MDL_ALPHA     64
#define LEN_MID       62          /* 0..61 lit; 62 -> +8 bits; 63 -> +16   */
#define LEN_HI        63
#define LEN_MID_BITS  8
#define LEN_HI_BITS   16
#define NRES_MDL      (N_CTX * (NSPAT + 1))
#define NFLG_CTX      20               /* flag ctx: prev-flag(5) x4 grad */
#define NRUN_CTX      20               /* run ctx: pmatch x len-hist */
#define FLAG_MDL_IDX  NRES_MDL
#define RUN_MDL_IDX   (FLAG_MDL_IDX + NFLG_CTX)
#define HI_MDL_IDX    (RUN_MDL_IDX + NRUN_CTX) /* hi-escape bucket model */
#define CFL_MDL_IDX   (HI_MDL_IDX + 1)   /* OP_CFL residuals */
#define N_MDL         (CFL_MDL_IDX + 1)
#define MDL_PRIOR_BITS 5          /* prior shrink factor at each norm     */

/* PRED/PREDNF residual index is mode*4+gb (36 slots). OP_PREDNFX and
   OP_PREDX fold ctx into the leftover NRES space, skipping 0..35 so a
   mixed stream does not smash the two histograms together. ctx is
   0..404; 400..404 share the last row. */
static inline uint32_t pred_res_midx(int mode, int gb, int ctx, int fold)
{
    if (!fold) return (uint32_t)(mode * 4 + gb);
    if (ctx > 400) ctx = 400;
    return (uint32_t)(36 + ctx * (NSPAT + 1) + mode);
}

typedef struct {
    uint32_t cum[MDL_ALPHA + 1];  /* frozen CDF, rebuilt only at norm     */
    uint16_t f[MDL_ALPHA];        /* frozen normalized freqs (coding)     */
    uint16_t h[MDL_ALPHA];        /* adaptive histogram (per-symbol bump) */
    uint32_t tot;                 /* current sum of h                     */
    uint8_t  lut[64];             /* idx>>6 -> first candidate symbol     */
    uint32_t hits;                /* updates since last normalization     */
} mdl64_t;

/* informative static prior: residuals concentrate near u=0, decay
   geometrically, and the two escape classes keep solid mass. Both ends
   build it identically with pure integer math. */
static void mdl64_build_prior(uint16_t t[MDL_ALPHA], int id, int mclass) {
    if (id == 0) {
        for (int s = 0; s < MDL_ALPHA; s++) t[s] = RANS_TOT / MDL_ALPHA;
        return;
    }
    uint32_t raw[MDL_ALPHA], S = 0;
    if (mclass == 0) {
        /* FLAG: 0 residual, 1 temporal run, 2 left, 3 up,
           4 zero-residual (no u), 5 zero-residual run.
           4/5 need mass or RDO never emits them and they never
           learn. Unused symbols sit at 1 so 0/1 stay cheap. */
        raw[0] = 2048; raw[1] = 2048;
        raw[2] = 32;   raw[3] = 32;
        raw[4] = 256;  raw[5] = 512;
        for (int s = 6; s < MDL_ALPHA; s++) raw[s] = 1;
    } else if (mclass == 1) {
        /* RUN: run lengths usually land in the escape classes */
        for (int s = 0; s < MDL_ALPHA - 2; s++) raw[s] = 16;
        raw[MDL_ALPHA - 2] = raw[MDL_ALPHA - 1] = 2048;
    } else if (mclass == 2) {
        /* RES: residual classes peak at u=0, geometric decay */
        raw[0] = 2048;
        for (int s = 1; s < MDL_ALPHA - 2; s++) {
            raw[s] = raw[s - 1] >> 1;
            if (raw[s] < 4) raw[s] = 4;
        }
        raw[MDL_ALPHA - 2] = raw[MDL_ALPHA - 1] = 64;
    } else if (mclass == 3) {
        /* OP: RUN/PRED dominate, RAW rare */
        raw[0] = 1024; raw[1] = 512; raw[2] = 256;
        raw[3] = 1024; raw[4] = 64; raw[5] = 256;
        for (int s = 6; s < MDL_ALPHA; s++) raw[s] = 4;
    } else if (mclass == 5) {
        /* PALF: predicted-rank flag - 1 dominates by construction */
        raw[0] = 256; raw[1] = 2048;
        for (int s = 2; s < MDL_ALPHA; s++) raw[s] = 8;
    } else if (mclass == 6) {
        /* Row-dict run lengths on a 16-wide screen tile. Non-final
           runs cluster on lengths 2, 4, 6, 8 and 12 (symbols 1, 3, 5,
           7, 11). Class 2 peaks at 0, so those symbols paid the floor. */
        for (int s = 0; s < MDL_ALPHA; s++) raw[s] = 8;
        raw[1] = 1024; raw[3] = 2048; raw[5] = 1024;
        raw[7] = 2048; raw[11] = 2048;
    } else {
        /* flat over the whole alphabet (literals) */
        for (int s = 0; s < MDL_ALPHA; s++) raw[s] = 64;
    }
    for (int s = 0; s < MDL_ALPHA; s++) S += raw[s];
    uint32_t acc = 0;
    int am = 0;
    for (int s = 0; s < MDL_ALPHA; s++) {
        uint32_t v = (uint32_t)(((uint64_t)raw[s] * RANS_TOT) / S);
        if (v == 0) v = 1;
        t[s] = (uint16_t)v; acc += v;
        if (v > t[am]) am = s;
    }
    t[am] = (uint16_t)((int)t[am] + (int)RANS_TOT - (int)acc);
}

/* model index -> prior class: 0 FLAG, 1 RUN, 2 residual */
static int mdl64_class(uint32_t midx) {
    if (midx >= FLAG_MDL_IDX && midx < RUN_MDL_IDX) return 0;
    if (midx >= RUN_MDL_IDX && midx < HI_MDL_IDX) return 1;
    return 2;
}

/* coarse gradient bucket shared by the flag/run record routing:
   flat neighborhoods almost always satisfy the run bound, edges
   almost never do - both ends derive it from causal pixels. */
static inline int grad_bucket(int a, int b, int c, int d2, int qs) {
    int g = (a - c < 0 ? c - a : a - c)
          + (b - c < 0 ? c - b : b - c)
          + (d2 - b < 0 ? b - d2 : d2 - b);
    int t = qs * 32;
    return g < t ? 0 : g < t * 4 ? 1 : g < t * 16 ? 2 : 3;
}

/* run-length ctx: the running mean of past run lengths predicts the
   next run far better than gradient does - geometric buckets of the
   average, deterministic on both sides via update_run(). */
static inline int run_bucket(const pstate_t *S) {
    int k = compute_k(S->A_run, S->N_run);
    return k <= 1 ? 0 : k <= 2 ? 1 : k <= 3 ? 2 : 3;
}

/* ---- tile-program layer constants (v14) ----
   The frame is a 16x16 tile grid. Each tile emits one opcode plus
   parameters into the "program" stream; PRED/COPY tiles also emit
   per-pixel records into the residual stream (same machinery as v12).
   Both streams are ordinary segmented rANS blobs, pulled demand-driven. */
#define TILE_SZ   16
#define MV_MAX    32
#define OP_RUN    0
#define OP_COPY   1
#define OP_PAL    2
#define OP_PRED   3
#define OP_RAW    4
#define OP_ICOPY  5   /* intra block copy: reference is the current frame's
                         already-decoded region (screen-content matching) */
#define OP_NN     6   /* learned linear predictor (per-plane Q8 weights in
                         the program stream header); falls back to the
                         fixed predictor bank when no weights were sent */
#define OP_SPLIT  7   /* tile too heterogeneous for one op: decode as four
                         half-size children, each with its own op */
#define OP_MVRUN  8   /* skip mode: copy tile from prev frame at (dx,dy),
                         no residual stream (inter) */
#define OP_IRUN   9   /* same but the source is the current frame's
                         already-decoded region (intra) */
#define OP_MVRUNP 10  /* OP_MVRUN with MV taken from the last temporal MV
                         (predicted skip - zero parameter bits) */
#define OP_COPYP  11  /* OP_COPY with predicted temporal MV */
#define OP_IRUNP  12  /* OP_IRUN with predicted intra offset */
#define OP_RPT    13  /* repeat the previous leaf op for this tile too;
                         followed by a PM_REPN count covering the next
                         k-1 tiles (runs of RUN / MVRUNP / IRUNP) */
#define OP_QS     14  /* stream directive: set residual quantizer step to
                         a PM_QT record + 1 for following tiles (consumes
                         no tile; not a leaf op, leaves last_op alone) */
#define OP_NOISE  15  /* procedural fill: tile = mean + deterministic PRNG
                         noise of amplitude sigma. For content that is
                         statistically indistinguishable from noise the
                         encoder emits (mean, sigma) instead of residuals */
#define OP_PSAME  17  /* program-same: replay the leaf op + parameters
                         stored for this tile slot by the PREVIOUS frame.
                         Same replay semantics as OP_VRPT but the memory
                         is temporal, so it needs the stateful decode
                         API (v3s); streams carrying it set prog_hdr
                         bit28.  Only top-level leaf tiles are eligible;
                         split tiles store a SPLIT marker instead. */
#define PHDR_STATEFUL 0x10000000u  /* prog_hdr bit: uses v3s features */
#define PHDR_WSAME    0x08000000u  /* LLP table identical to previous */
#define PHDR_C420     0x04000000u  /* chroma coded at half resolution */
#define PHDR_FRESH    0x02000000u  /* streams cold-seeded despite carry */
#define PHDR_PLEN     0x01FFFFFFu  /* program blob length field */
#define OP_CFL    18  /* chroma-from-luma: leaf = a*luma + b affine
                         predictor (a in Q6), residual-coded.  Params ride
                         in the leaf's dx=a/dy=b slots for VRPT replay. */
#define OP_PALR   19  /* OP_PAL with the index map coded as raster-order
                         (idx,run) pairs - text/UI index maps are long
                         horizontal runs, so a 16x16 glyph leaf streams
                         ~tens of records instead of 256 literals. */
#define OP_PALX   20  /* lane-above replay of a PAL leaf INCLUDING the
                         index map: emitted only when the stored
                         descriptor is pixel-exact (pal+imap equal) -
                         one op record reconstructs the whole leaf */
#define OP_PSAMX  21  /* OP_PSAME variant: pixel-exact temporal replay,
                         for leaves whose descriptor fully determines
                         the pixels (PAL family carrying imap) */
#define OP_PALT   22  /* palette leaf whose index map is synthesized from
                         luma: entries sorted by luma centroid + k-1
                         thresholds; idx = rank(lu).  Text/UI chroma is
                         luma-determined, so the whole 256-px index map
                         costs the op + palette + thresholds.  Never
                         params-replayed: vop_match requires the index
                         map too (pixel-exact via PALX/PSAMX only). */
#define OP_PALC   23  /* palette leaf with a flag-coded index map: per
                         pixel a PM_PALF flag says idx == idx_above
                         (virtual row 0 = idx 0), else a PM_PALI literal
                         follows.  Captures the vertical stroke
                         continuity of text/UI glyphs. */

#define OP_PREDNF 24  /* OP_PRED with the flag/run machinery suppressed:
                         every pixel emits residual only - wins on dense
                         textured tiles where flag0 dominates anyway */
#define OP_NNF    25  /* OP_NN variant, same suppression */
#define OP_PREDZ  26  /* pred-only leaf: every pixel within (qs-1)/2 of the
                         spatial predictor -> zero residual stream */
#define OP_NNZ    27  /* same with the learned predictor */
#define OP_COPYNF 28  /* OP_COPY with the flag machinery suppressed */
#define OP_ICOPYNF 29 /* OP_ICOPY with the flag machinery suppressed */
#define OP_PALD   30  /* palette leaf with a med-predicted index map:
                         entries are emitted in VALUE order (a smooth
                         index space for photo/gradient palettes), each
                         pixel streams zigzag(idx - med3(left,up,up-left))
                         via PM_PALD instead of a raw index literal. */
#define OP_PREDIR 31  /* OP_PRED with the intra flag subset {0,2,3,4,5}:
                         flag machinery on, temporal copy-run disabled.
                         Exists so keyframes (and prev-less stripes) get
                         left/up/zero-res run collapsing like inter. */
#define OP_PALQ   32  /* OP_PALT whose palette values are cache indices.
                         Slot 0 is the newest color this plane has already
                         sent; symbol 8 plus a PM_PALV literal is a miss.
                         Old streams never emit it, so their priors and
                         symbol numbers stay put. */
#define OP_ROWD   33  /* Row dictionary. A tile whose rows repeat (UI
                         rules, title bars) sends each distinct row once
                         as horizontal palette-index runs, then one
                         PM_PALF flag per later row: 1 copies the pattern
                         above, 0 is followed by a PM_PALI pattern index.
                         Row 0 is pattern 0 and is not signaled. Each
                         pattern is PM_PALN(nruns-1), then index, length
                         for every run except the one that finishes the
                         row (the decoder fills what is left). Old
                         streams never emit it. No new model: the records
                         ride the existing peaked PALN/PALI/REPN/PALF
                         priors, so a cold tile is priced honestly. */
#define OP_ROWQ   34  /* OP_ROWD whose palette values are PALQ cache
                         slots. A miss is symbol 8 plus a PM_PALV literal.
                         Priced against the cache at tile start and kept
                         only when that is cheaper than literal OP_ROWD,
                         so the first copy of a color stays a literal. */
#define OP_ROWS   35  /* OP_ROWD with one shared prefix. Every distinct
                         row starts with the same stem; the stem is sent
                         once and each pattern sends only its tail. The
                         width is the cheapest shared prefix under the
                         frozen prior. Old streams never emit it. Kept
                         only when that price beats OP_ROWD and OP_ROWQ. */
#define OP_PALZ   36  /* OP_PALT whose palette values are zigzag(value)
                         on PM_PALZ. Screen chroma clusters near 0, and
                         value+128 always takes the flat class-4 escape.
                         PALT itself is unchanged, so old streams still
                         decode. Kept only when the new price wins. */
#define OP_PALU   37  /* OP_PALQ values (recent-color slot) with OP_PALZ
                         delta boundaries. The slot is cheaper than
                         re-sending a zigzag once the color is cached,
                         and the boundary stays a class-4 literal.
                         Old streams never emit it. */
#define OP_SPAN   38  /* After a temporal MV is established: one record
                         covers tiles that are each an exact copy at
                         last_tmv, or a single co-located hole between
                         those runs. Hole pixels are not signaled. The
                         tile count is PM_REPN(k-1); each motion-run
                         length is PM_RRL(len-1). Old streams never
                         emit it. No new model. */
#define OP_TX     39  /* Reversible Haar of (curr - temporal pred).
                         Square 4/8/16 leaves. Program: COPY-style MV
                         (skipped on keyframes). Residual: n-1 then n
                         zigzag Haar coeffs on CFL_MDL_IDX. Luma only.
                         Old streams never emit it. No new model. */
#define OP_TXI    40  /* Intra Haar of (curr - DC of left/top border).
                         Same residual shape as OP_TX. No MV.
                         Old streams never emit it. */
#define OP_PREDNFX 41 /* PREDNF with residual midx 36+ctx*(NSPAT+1)+mode
                         (ctx clamped to 400). Uses the unused NRES
                         slots so each neighborhood has its own residual
                         histogram. Old PREDNF keeps mode*4+gb.
                         Old streams never emit this op. */
#define OP_PREDX   42 /* PRED/PREDIR flags plus PREDNFX residual fold.
                         Intra baseline is dense fold; this keeps left/up
                         runs on tiles the flags still collapse.
                         Old streams never emit this op. */
#define OP_VRPT   16  /* vertical repeat: replay the leaf op + parameters
                         stored by the same-size rect directly above.
                         Per-pixel data (palette indexes, residuals, RAW)
                         is still streamed; the opcode and its parameters
                         (MV, palette entries, sigma) come from the stored
                         slot, so a repeated UI structure costs one
                         op record per tile row. */

/* prog-stream-local model indices */
#define PM_OP     0
#define PM_MV     1
#define PM_PALN   2
#define PM_PALV   3
#define PM_PALI   4
#define PM_RAW    5
#define PM_NNW    6   /* learned-predictor weights, one record per tap  */
#define PM_OP0    7   /* op model conditioned on previous leaf op
                         times whether the tile above emitted SPLIT */
#define N_OPOPS   16  /* leaf ops that get their own context lane */
#define N_OPCTX   (2 * N_OPOPS)
#define PM_REPN   (PM_OP0 + N_OPCTX)
#define PM_QT     (PM_REPN + 1)      /* per-plane quantizer step record */
#define PM_HI     (PM_QT + 1)        /* hi-escape bucket model (off>>6) */
#define PM_CFLA   (PM_HI + 1)        /* CFL slope, zigzag Q6 */
#define PM_CFLB   (PM_CFLA + 1)      /* CFL intercept, zigzag */
#define PM_PALF   (PM_CFLB + 1)      /* PALC predicted-rank flag */
#define PM_PALD   (PM_PALF + 1)      /* PALD index-delta model */
#define PM_PALQ   (PM_PALD + 1)      /* PALQ cache-slot / miss escape */
#define PM_PALZ   (PM_PALQ + 1)      /* OP_PALZ zigzag palette value */
#define PM_RRL    (PM_PALZ + 1)      /* row-dict run length (class 6) */
#define NPROG_MDL (PM_RRL + 1)
#define N_NNW     16  /* LLP taps: a b c d2 t med a+b-c luma bias(256),
                         aa uu (2nd-order spatial) + prev-frame cross
                         neighborhood pl pr pu pd (prev is fully decoded,
                         any offset is causal) + e: previous decoded
                         residual (error feedback) */
#define PAL_MAX   32
/* per-column leaf-op memory for OP_VRPT: three depth buckets indexed by
   x at 16/8/4 px granularity; a cell is valid when the stored rect ends
   exactly at the requesting rect's top edge */
typedef struct {
    int16_t rx, ry, rw;         /* rect that wrote this cell */
    int16_t dx, dy;             /* effective MV for the copy family */
    int16_t pal[PAL_MAX];
    uint8_t imap[TILE_SZ * TILE_SZ]; /* PAL/PALR leaves: decoded index map -
                                   part of the descriptor, so a lane/PSAME
                                   replay reconstructs the leaf verbatim */
    int npal, sig, op;          /* op==0 (OP_RUN) marks an empty cell? no:
                                   validity is rw!=0; op is the leaf op */
} vop_t;

/* ---- split-probe transactions ------------------------------------
   The SPLIT candidate emits its four children for real (emit=1) so the
   probe cost is the exact emitted cost - sibling context propagation,
   rescue sweeps and all - and a winning split commits instantly
   instead of evaluating the subtree a second time.  When the leaf
   still wins, txn_rollback restores every side effect: record counts,
   model bumps (first-touch log per level), vop cells (write log),
   scalars, predictor state and the recon rect. */
/* bump touches only h[]/tot/hits; cum/f/lut change at norm time which
   never happens inside a transaction - log just the mutable tail */
typedef struct {
    mdl64_t *p;
    uint16_t h[MDL_ALPHA];
    uint32_t tot, hits;
    int lvl;
} mdu_t;
typedef struct { vop_t   *p; vop_t  v; } vdu_t;
#define PALQ_N    8             /* recent colors remembered per plane */
#define PALQ_MISS 8             /* PM_PALQ escape: a PM_PALV literal follows */
typedef struct {
    uint64_t bm[(N_MDL + NPROG_MDL + 63) / 64];
    pstate_t S;
    int16_t rc[TILE_SZ * TILE_SZ];
    vop_t   pre_slot;
    size_t  npr, nrr;
    int     mu0, vu0, has_rc;
    int16_t rc2[TILE_SZ * TILE_SZ];   /* post-subtree recon snapshot */
    int     qs, last_op, all_run, nn_cnt;
    int     prev_copy, prev_dx, prev_dy;
    int     last_tmv[2], last_imv[2];
    int     up, mx, my;
    uint64_t h0, h0r;
    /* post-probe stash: while the transaction is open, alternative
       evaluators run against the PRE-probe snapshot (the only state
       the decoder can reconstruct if the subtree rolls back), so the
       post-subtree scalars live here until the win path restores
       them */
    int     pqs, plast_op, pall_run, pnn_cnt;
    int     pprev_copy, pprev_dx, pprev_dy;
    int     plast_tmv[2], plast_imv[2];
    int     pup, pmx, pmy;
    int16_t palq[PALQ_N];
    int     palq_n;
    vop_t   ppre_slot;
    /* vops: the tile's own-width above-cell can share a bucket with
       sub-16 children; ptab[cur_ti] is SPLIT-marked by the probe */
    vop_t   vc0, pvc0;
    vop_t   pptab;
    int     pptab_v;
    int     lite;   /* nested probe: pstate rolls back via pu2 dirty-row
                       log instead of the 16KB snapshot */
} txn_t;
#define TXN_MAXLVL 3
static _Thread_local mdu_t *mdu_l;
static _Thread_local int    mdu_n, mdu_cap;
static _Thread_local mdl64_t *dbg_pmc, *dbg_cm;
static _Thread_local vop_t   *dbg_vo;
static _Thread_local vdu_t *vdu_l;
static _Thread_local int    vdu_n, vdu_cap;
static _Thread_local txn_t *ztxn_st[TXN_MAXLVL];
static _Thread_local int    ztxn_n;
static _Thread_local mdl64_t *ztxn_cm, *ztxn_pm;
static _Thread_local long txn_cnt;
static _Thread_local uint32_t *cp_p, *cp_r;
static _Thread_local size_t cp_np, cp_nr;
static _Thread_local int cp_ok;

/* first-touch log: one saved image per model per open transaction
   level - later bumps to the same model need no new entry */
static void mdu_log(mdl64_t *m) {
    int idx;
    if (m >= ztxn_cm && m < ztxn_cm + N_MDL)
        idx = (int)(m - ztxn_cm);
    else if (m >= ztxn_pm && m < ztxn_pm + NPROG_MDL)
        idx = (int)(m - ztxn_pm) + N_MDL;
    else
        return;   /* local scratch model - not part of stream state */
    for (int L = 0; L < ztxn_n; L++) {
        uint64_t *bm = ztxn_st[L]->bm;
        if (!((bm[idx >> 6] >> (idx & 63)) & 1)) {
            bm[idx >> 6] |= 1ull << (idx & 63);
            if (mdu_n == mdu_cap) {
                mdu_cap = mdu_cap ? mdu_cap * 2 : 512;
                mdu_l = realloc(mdu_l, (size_t)mdu_cap * sizeof(mdu_t));
            }
            mdu_t *e = &mdu_l[mdu_n++];
            e->p = m; e->tot = m->tot; e->hits = m->hits; e->lvl = L;
            memcpy(e->h, m->h, sizeof e->h);
        }
    }
}

/* vop cells are written at most once per leaf, so log every write and
   restore in reverse order */
static inline void vdu_log(vop_t *p) {
    if (ztxn_n) {
        if (vdu_n == vdu_cap) {
            vdu_cap = vdu_cap ? vdu_cap * 2 : 256;
            vdu_l = realloc(vdu_l, (size_t)vdu_cap * sizeof(vdu_t));
        }
        vdu_l[vdu_n].p = p; vdu_l[vdu_n].v = *p; vdu_n++;
    }
}

/* cell index for rect (tx,tw): bucket0=16px grid, bucket1=8px,
   bucket2=4px-and-smaller; caller passes ntx = ceil(w/TILE_SZ) */
static int vcell_idx(int ntx, int tx, int tw)
{
    if (tw >= TILE_SZ) return tx >> 4;
    if (tw == 8)       return ntx + (tx >> 3);
    return 3 * ntx + (tx >> 2);
}
#define VOP_CELLS(ntx) (7 * (ntx) + 4)
#define LIT_BIAS  128           /* palette/literal: v+128 must fit 0..255 */

/* Move v to the front of the plane's recent-color cache. Encoder and
   decoder call this only after a palette leaf has actually streamed its
   values, in palette order, so the next leaf prices the same slots. */
static void palq_note(int16_t *q, int *n, int16_t v) {
    int nn = *n, f = -1;
    for (int i = 0; i < nn; i++) if (q[i] == v) { f = i; break; }
    if (f == 0) return;
    if (f < 0) {
        if (nn < PALQ_N) nn++;
        f = nn - 1;
    }
    for (int i = f; i > 0; i--) q[i] = q[i - 1];
    q[0] = v;
    *n = nn;
}
static _Thread_local int16_t *dpq;
static _Thread_local int *dpqn;

/* prog model index -> prior class (3 op, 2 geo0, 4 flat) */
static const uint8_t prog_cls[NPROG_MDL] = {3, 2, 2, 4, 2, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 4, 5, 2, 2, 2, 6};

static void mdl64_init(mdl64_t *m, const uint16_t *prior) {
    uint32_t c = 0, ht = 0;
    for (int s = 0; s < MDL_ALPHA; s++) {
        uint32_t hp = prior[s] >> MDL_PRIOR_BITS;
        if (hp == 0) hp = 1;
        m->f[s] = prior[s];
        m->h[s] = (uint16_t)hp;
        ht += hp;
        m->cum[s] = c; c += prior[s];
    }
    m->cum[MDL_ALPHA] = RANS_TOT;
    m->tot = ht; m->hits = 0;
    for (int b = 0; b < 64; b++) m->lut[b] = 0;
    for (int s = 0; s < MDL_ALPHA; s++) {
        /* bucket start (b*64) inside [cum[s], cum[s+1]) -> lut[b]=s.
           lower bound for any idx in the bucket; decode then advances. */
        int b0 = (int)((m->cum[s] + 63) >> 6);
        int b1 = (int)((m->cum[s + 1] - 1) >> 6);
        for (int b = b0; b <= b1 && b < 64; b++) m->lut[b] = (uint8_t)s;
    }
}

static void mdl64_norm(mdl64_t *m) {
    uint32_t n[MDL_ALPHA], S = 0;
    int am = 0;
    for (int s = 0; s < MDL_ALPHA; s++) {
        uint32_t v = (uint32_t)(((uint64_t)m->h[s] * RANS_TOT) / m->tot);
        if (v == 0) v = 1;
        n[s] = v; S += v;
        if (v > n[am]) am = s;
    }
    /* force the sum to exactly RANS_TOT on the largest bucket */
    n[am] = (uint32_t)((int)n[am] + (int)RANS_TOT - (int)S);
    uint32_t c = 0, ht = 0;
    for (int s = 0; s < MDL_ALPHA; s++) {
        m->f[s] = (uint16_t)n[s];
        /* shrink the carried-over prior so fresh evidence dominates
           quickly (otherwise ~4096 stale mass takes ~10 segments) */
        uint32_t hp = n[s] >> MDL_PRIOR_BITS;
        if (hp == 0) hp = 1;
        m->h[s] = (uint16_t)hp;
        ht += hp;
        m->cum[s] = c; c += n[s];
    }
    m->cum[MDL_ALPHA] = RANS_TOT;
    m->tot = ht;
    m->hits = 0;
    for (int b = 0; b < 64; b++) m->lut[b] = 0;
    for (int s = 0; s < MDL_ALPHA; s++) {
        /* bucket start (b*64) inside [cum[s], cum[s+1]) -> lut[b]=s.
           lower bound for any idx in the bucket; decode then advances. */
        int b0 = (int)((m->cum[s] + 63) >> 6);
        int b1 = (int)((m->cum[s + 1] - 1) >> 6);
        for (int b = b0; b <= b1 && b < 64; b++) m->lut[b] = (uint8_t)s;
    }
}

static void wr_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t rd_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t rd_u64le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

/* ---- encoder side: renorm words are emitted backwards ---- */

typedef struct { uint32_t *ptr; } ransw_t;

static void rans_put(uint64_t *x, uint32_t cum, uint32_t f, ransw_t *w) {
    uint64_t xmax = (((uint64_t)(RANS_L >> RANS_SCALE)) << 32) * f;
    while (*x >= xmax) { *--w->ptr = (uint32_t)*x; *x >>= 32; }
    *x = ((*x / f) << RANS_SCALE) + (*x % f) + cum;
}

static void rans_put_raw(uint64_t *x, uint32_t v, int bits, ransw_t *w) {
    uint64_t xmax = ((uint64_t)(RANS_L >> bits)) << 32;
    while (*x >= xmax) { *--w->ptr = (uint32_t)*x; *x >>= 32; }
    *x = (*x << bits) | v;
}

/* ---- decoder side: renorm words are read forwards ---- */

typedef struct {
    const uint8_t *p, *end;
    int err;
} ransr_t;

static uint32_t rans_rd32(ransr_t *r) {
    if (r->p + 4 > r->end) { fprintf(stderr,"ERR underrun p=%p end=%p\n",
                                     (void*)r->p,(void*)r->end);
                           r->err = 1; r->p = r->end; return 0; }
    const uint8_t *p = r->p; r->p += 4;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int rans_get(uint64_t *x, const mdl64_t *m, ransr_t *r) {
    uint32_t idx = (uint32_t)*x & (RANS_TOT - 1);
    int s = m->lut[idx >> 6];
    while (s < MDL_ALPHA - 1 && m->cum[s + 1] <= idx) s++;
    *x = (uint64_t)m->f[s] * (*x >> RANS_SCALE) + idx - m->cum[s];
    while (*x < RANS_L) *x = (*x << 32) | rans_rd32(r);
    return s;
}

static uint32_t rans_get_raw(uint64_t *x, int bits, ransr_t *r) {
    uint32_t v = (uint32_t)*x & ((1u << bits) - 1);
    *x >>= bits;
    while (*x < RANS_L) *x = (*x << 32) | rans_rd32(r);
    return v;
}

typedef struct {
    const uint8_t *cur;      /* next segment header position */
    const uint8_t *seg_end;
    ransr_t rr;
    uint64_t x[RANS_LANES];
    int nsym;                /* semantic symbols consumed in this segment */
    int seg_idx;             /* -1 before first segment                   */
    int seg_cap;             /* capacity of the current segment           */
    const uint8_t *in_base;
    int lanes;               /* interleaved rANS states (1,2,4)           */
    int segmode;             /* 0: u32 length, 1: bare single segment,
                                2: u8 length. Bit 3/4 of the flags byte.
                                Old streams leave both clear. */
    mdl64_t *mdls;
    int nmdl;
    const uint8_t *in_end;
    int err;
    int reslog;
    long tpull;                  /* total symbols pulled, survives segs */
    uint32_t hi_midx;            /* model index for hi-escape buckets */
} zrd_t;

static void zrd_next_seg(zrd_t *d) {
    /* full-model norm scan: with carried state a warm model may hold
       pending hits without being on any fresh touched-list */
    for (int i = 0; i < d->nmdl; i++) {
        mdl64_t *m = &d->mdls[i];
        if (m->hits >= 48 || m->hits * 8 >= m->tot) mdl64_norm(m);
    }
    /* bare: one segment fills the buffer, so the length prefix is
       redundant. short: every segment's payload fits in a byte. */
    size_t hdr = d->segmode == 1 ? 0 : (d->segmode == 2 ? 1 : 4);
    if (d->cur + hdr + (size_t)d->lanes * 8 > d->in_end) {
        d->err = 1; d->seg_cap = 0; d->seg_end = d->cur;
        d->rr.p = d->cur; d->rr.end = d->cur; return;
    }
    uint32_t len;
    if (d->segmode == 1) len = (uint32_t)(d->in_end - d->cur);
    else if (d->segmode == 2) { len = d->cur[0]; d->cur += 1; }
    else { len = rd_u32le(d->cur); d->cur += 4; }
    const uint8_t *p = d->cur;
    if (ZE(zseg)) fprintf(stderr, "SEG idx=%d len=%u nsym=%d rrpos=%ld\n",
                                d->seg_idx + 1, len, d->nsym,
                                (long)(d->rr.p - (const uint8_t *)0));
    if (len < (uint32_t)d->lanes * 8 || p + len > d->in_end) {
        d->err = 1; d->seg_cap = 0; d->seg_end = p;
        d->rr.p = p; d->rr.end = p; return;
    }
    for (int l = 0; l < d->lanes; l++) d->x[l] = rd_u64le(p + l * 8);
    d->rr.p = p + d->lanes * 8;
    d->seg_end = p + len;
    d->rr.end = d->seg_end;
    d->cur = d->seg_end;
    d->nsym = 0;
    d->seg_idx++;
    /* geometric capacity schedule: 128,256,...,16384,16384,... */
    d->seg_cap = (d->seg_idx < 7) ? (SEG_MIN << d->seg_idx) : SEG_SYMS;
    if (ZE(zmh) && !d->reslog) {
        uint64_t hh = 1469598103934665603ull;
        for (int i = 0; i < d->nmdl; i++) {
            const mdl64_t *m = &d->mdls[i];
            for (int s = 0; s < MDL_ALPHA; s++) {
                hh ^= m->f[s]; hh *= 1099511628211ull;
                hh ^= m->h[s]; hh *= 1099511628211ull;
            }
            hh ^= m->tot; hh *= 1099511628211ull;
        }
        fprintf(stderr, "MH D seg=%d hash=%016llx\n", d->seg_idx,
                (unsigned long long)hh);
    }
}

static FILE *zlogf;
static int zrd_sym(zrd_t *d, uint32_t midx) {
    if (ZEP(zlog) && !zlogf) zlogf = fopen(ZEP(zlog), "w");
    if (d->err) return 0;
    if (d->nsym >= d->seg_cap) zrd_next_seg(d);
    if (d->err) return 0;
    int lane = d->nsym & (d->lanes - 1);
    d->nsym++; d->tpull++;
    mdl64_t *m = &d->mdls[midx];
    int s = rans_get(&d->x[lane], m, &d->rr);
    if (d->rr.err) { d->err = 1;
        fprintf(stderr,"RDERR res=%d seg=%d nsym=%d tpull=%ld off=%ld segoff=%ld\n",
                d->reslog, d->seg_idx, d->nsym, d->tpull,
                (long)(d->rr.p - d->in_base), (long)(d->seg_end - d->in_base));
        return 0; }
    m->h[s]++; m->tot++; m->hits++;
    if (ZE(zrs) && d->reslog)
        fprintf(stderr, "RS D %u:%d\n", midx, s);
    if (zlogf && midx >= FLAG_MDL_IDX && midx < RUN_MDL_IDX) fprintf(zlogf, "%cF\t%d\n", d->reslog?'R':'P', s);
    return s;
}

/* decode a length-class symbol: 0..61 literal, 62 -> +8b (62..317),
 * 63 -> +16b (318..65853). Shared by residuals and run lengths. */
static uint32_t zrd_len_sym(zrd_t *d, uint32_t midx) {
    if (ZEP(zlog) && !zlogf) zlogf = fopen(ZEP(zlog), "w");
    int s = zrd_sym(d, midx);
    if (ZE(zlv)) fprintf(stderr, "LV midx=%u s=%d tp=%ld\n", midx, s, d->tpull);
    if (s < LEN_MID) { if (zlogf) fprintf(zlogf, "%c%u\t%u\n", d->reslog?'R':'P', midx, (uint32_t)s);
                       return (uint32_t)s; }
    int lane = (d->nsym - 1) & (d->lanes - 1);
    if (s == LEN_MID) {
        if (midx == d->hi_midx) {
            uint32_t u = LEN_MID + rans_get_raw(&d->x[lane], LEN_MID_BITS, &d->rr);
            if (zlogf) fprintf(zlogf, "%c%u\t%u\n", d->reslog?'R':'P', midx, u);
            return u;
        }
        mdl64_t *hm = &d->mdls[d->hi_midx];
        int hs = rans_get(&d->x[lane], hm, &d->rr);
        if (d->rr.err) { d->err = 1;
        fprintf(stderr,"RDERR res=%d seg=%d nsym=%d tpull=%ld off=%ld segoff=%ld\n",
                d->reslog, d->seg_idx, d->nsym, d->tpull,
                (long)(d->rr.p - d->in_base), (long)(d->seg_end - d->in_base));
        return 0; }
        hm->h[hs]++; hm->tot++; hm->hits++;
        uint32_t u = LEN_MID + ((uint32_t)hs << 4)
                   + rans_get_raw(&d->x[lane], 4, &d->rr);
        if (zlogf) fprintf(zlogf, "%c%u\t%u\n", d->reslog?'R':'P', midx, u);
        if (ZE(zlv)) fprintf(stderr, "LV2 midx=%u u=%u\n", midx, u); return u; }
    if (ZE(zoldhi))
        { uint32_t u = 318 + rans_get_raw(&d->x[lane], LEN_HI_BITS, &d->rr);
          if (zlogf) fprintf(zlogf, "%c%u\t%u\n", d->reslog?'R':'P', midx, u); return u; }
    if (midx == d->hi_midx)      /* nested bucket's own tail */
        { uint32_t u = 318 + rans_get_raw(&d->x[lane], LEN_HI_BITS, &d->rr);
          if (zlogf) fprintf(zlogf, "%c%u\t%u\n", d->reslog?'R':'P', midx, u); return u; }
    /* outer hi escape: bucket index len-coded on the same lane (it does
       NOT consume a lane slot - the encoder put it inline), + 6 raw lo */
    mdl64_t *hm = &d->mdls[d->hi_midx];
    if (ZE(zhi)) fprintf(stderr, "HI D midx=%u nsym=%d lane=%d\n",
                               midx, d->nsym, lane);
    int hs = rans_get(&d->x[lane], hm, &d->rr);
    if (ZE(zhi)) fprintf(stderr, "HI D2 hs=%d\n", hs);
    if (d->rr.err) { d->err = 1;
        fprintf(stderr,"RDERR res=%d seg=%d nsym=%d tpull=%ld off=%ld segoff=%ld\n",
                d->reslog, d->seg_idx, d->nsym, d->tpull,
                (long)(d->rr.p - d->in_base), (long)(d->seg_end - d->in_base));
        return 0; }
    hm->h[hs]++; hm->tot++; hm->hits++;
    uint32_t hb;
    if (hs < LEN_MID)      hb = (uint32_t)hs;
    else if (hs == LEN_MID) hb = LEN_MID + rans_get_raw(&d->x[lane], LEN_MID_BITS, &d->rr);
    else                    hb = 318 + rans_get_raw(&d->x[lane], LEN_HI_BITS, &d->rr);
    uint32_t u = 318 + (hb << 6) + rans_get_raw(&d->x[lane], 6, &d->rr);
    if (ZE(zhi2)) fprintf(stderr, "HI D3 u=%u\n", u);
    if (zlogf) fprintf(zlogf, "%c%u\t%u\n", d->reslog?'R':'P', midx, u);
    return u;
}

static int zrd_run(zrd_t *d) {
    return (int)zrd_len_sym(d, RUN_MDL_IDX) + 1;
}

/* open a segmented rANS blob: flags byte + first segment header.
   warm=1 keeps the caller's persistent model state (the arrays already
   carry the post-stream state of the previous frame) instead of
   reseeding every model from the prior selected by the theme bit. */
static int zrd_open(zrd_t *d, mdl64_t *mdls, int nmdl,
                    int prog, const uint8_t *in, int32_t in_size, int warm)
{
    if (in_size < 1) return -1;
    d->lanes = 1 << ((in[0] >> 1) & 3);
    if (d->lanes > RANS_LANES) d->lanes = RANS_LANES;
    /* bits 1-2 are the lane count. bit 3 = bare, bit 4 = u8 lengths.
       Old encoders never set either, so a carried stream still decodes. */
    d->segmode = (in[0] & 8) ? 1 : ((in[0] & 16) ? 2 : 0);
    if (!warm) {
        uint16_t priors[7][MDL_ALPHA];
        int theme = in[0] & 1;
        for (int c = 0; c < 7; c++) mdl64_build_prior(priors[c], theme, c);
        for (int i = 0; i < nmdl; i++) {
            int cls = prog ? prog_cls[i] : mdl64_class((uint32_t)i);
            mdl64_init(&mdls[i], priors[cls]);
        }
    }
    d->mdls = mdls;
    d->nmdl = nmdl; d->in_end = in + in_size; d->err = 0;
    d->in_base = in;
    d->cur = in + 1;
    d->seg_end = d->cur;   /* empty stream -> consumed = flags byte only */
    d->seg_idx = -1; d->seg_cap = 0; d->nsym = 0;
    d->rr.p = NULL; d->rr.end = in + in_size; d->rr.err = 0;
    d->reslog = 0; d->tpull = 0;
    d->hi_midx = (uint32_t)(prog ? PM_HI : HI_MDL_IDX);
    for (int l = 0; l < RANS_LANES; l++) d->x[l] = 0;
    (void)in_size;
    /* lazy: first zrd_sym pulls the first segment header, so a
       zero-record stream (all-RUN tiles) reads nothing at all */
    return 0;
}

static void dec_plane_rans(const int16_t *prev, int16_t *curr,
                           int w, int h, zrd_t *d)
{
    pstate_t S; pstate_init(&S);
    int has_prev = (prev != NULL);

    for (int y = 0; y < h; y++) {
        int16_t *row = curr + (size_t)y * w;
        const int16_t *top = y > 0 ? curr + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? curr + (size_t)(y - 2) * w : NULL;
        const int16_t *prow = has_prev ? prev + (size_t)y * w : NULL;

        int x = 0;
        while (x < w) {
            if (has_prev) {
                int can_run = zrd_sym(d, FLAG_MDL_IDX);
                if (can_run) {
                    int run = zrd_run(d);
                    update_run(&S, run);
                    for (int i = 0; i < run; i++) row[x + i] = prow[x + i];
                    x += run;
                    continue;
                }
            }

            int a = x > 0 ? row[x - 1] : (top ? top[x] : 0);
            int aa = x > 1 ? row[x - 2] : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d2 = (top && x < w - 1) ? top[x + 1] : b;

            int q1 = q_grad(d2 - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);

            int spreds[NSPAT];
            spat_preds(a, b, c, d2, aa, uu, spreds);

            int has_t = has_prev ? 1 : 0;
            int mode = 0;
            int32_t best_A = S.As[ctx][0];
            for (int p = 1; p < NSPAT; p++) {
                if (S.As[ctx][p] < best_A) { best_A = S.As[ctx][p]; mode = p; }
            }
            if (has_t && S.At[ctx] < best_A) {
                best_A = S.At[ctx]; mode = NSPAT;
            }

            uint32_t u = zrd_len_sym(d, (uint32_t)mode);
            int r = zigzag_dec(u);
            if (flip) r = -r;

            int val;
            if (mode == NSPAT) val = prow[x] + r;
            else               val = spreds[mode] + r;
            row[x] = (int16_t)val;

            int rs[NSPAT];
            for (int p = 0; p < NSPAT; p++) rs[p] = val - spreds[p];
            int rt = has_t ? (val - prow[x]) : 0;
            if (flip) {
                for (int p = 0; p < NSPAT; p++) rs[p] = -rs[p];
                rt = -rt;
            }
            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) abs_s[p] = rs[p] < 0 ? -rs[p] : rs[p];
            int abs_t = has_t ? (rt < 0 ? -rt : rt) : abs_s[0];
            update_ctx(&S, ctx, abs_s, abs_t);
            x++;
        }
    }
}

/* decode one self-contained plane stream; returns bytes consumed */
static int32_t dec_plane_stream(const int16_t *prev, int16_t *curr,
                                int w, int h,
                                const uint8_t *in, int32_t in_size)
{
    zrd_t d;
    d.mdls = (mdl64_t *)malloc(sizeof(mdl64_t) * N_MDL);
    if (!d.mdls) return -1;
    if (zrd_open(&d, d.mdls, N_MDL, 0, in, in_size, 0) < 0) {
        free(d.mdls);
        return -1;
    }
    dec_plane_rans(prev, curr, w, h, &d);
    int32_t used = d.err ? -1 : (int32_t)(d.cur - in);
    free(d.mdls);
    return used;
}

static int32_t enc_pass_b(const uint32_t *recs, size_t nrec,
                          const uint16_t *prior /* [5][MDL_ALPHA], NULL =
                                                 keep carried mdls */,
                          uint8_t *out, int32_t out_cap,
                          mdl64_t *mdls,
                          uint32_t *scr, size_t words_cap, int lanes,
                          int nmdl, int prog);

/* Drop redundant segment lengths. A single segment's end is the end of
   the blob (mode 1, flags bit 3). Several short segments store a one-byte
   length (mode 2, flags bit 4). The rANS words are unchanged, so the
   model state the next frame carries stays the same. Old blobs, which
   never set those bits, still decode with a u32 length. */
static int32_t compact_segs(uint8_t *b, int32_t n, int *mode, int bare_ok)
{
    *mode = 0;
    if (n <= 0) return n;
    int off[96], slen[96], ns = 0, pos = 0;
    while (pos + 4 <= n && ns < 96) {
        uint32_t L = rd_u32le(b + pos);
        if (L < 8 || pos + 4 + (int)L > n) return n;
        off[ns] = pos + 4;
        slen[ns] = (int)L;
        ns++;
        pos += 4 + (int)L;
    }
    if (pos != n || ns == 0) return n;
    /* Bare drops the length and treats the rest of the buffer as the
       segment. That is safe only when the decoder's slice is exactly
       this blob. A residual blob is opened with whatever bytes follow
       the program, including the next plane, so it keeps a length. */
    if (ns == 1 && bare_ok) {
        memmove(b, b + 4, (size_t)slen[0]);
        *mode = 1;
        return slen[0];
    }
    for (int i = 0; i < ns; i++) if (slen[i] > 255) return n;
    int w = 0;
    for (int i = 0; i < ns; i++) {
        memmove(b + w + 1, b + off[i], (size_t)slen[i]);
        b[w] = (uint8_t)slen[i];
        w += 1 + slen[i];
    }
    *mode = 2;
    return w;
}

/* encode one record stream into a self-contained rANS blob:
   flags[1] (theme bit | lanes<<1 | compact) + segments. Encodes with both the
   uniform and the peaked prior and keeps the smaller result.
   carry != NULL: the model array is seeded from *carry and on success
   its post-stream state is written back (persistent codec state).
   warm=1 additionally skips the dual-theme pass - the carried models
   are already trained, so a single pass suffices. */
static int32_t enc_stream_v(const uint32_t *recs, size_t nrec,
                            int nmdl, int prog,
                            uint8_t *out, int32_t out_cap,
                            mdl64_t *carry, int warm, int bare_ok)
{
    int lanes = (nrec < 16384) ? 1 : (nrec < 131072 ? 2 : RANS_LANES);
    int lane_bits = (lanes == 1) ? 0 : (lanes == 2 ? 1 : 2);
    size_t msz = sizeof(mdl64_t) * (size_t)nmdl;
    mdl64_t *mdls = (mdl64_t *)malloc(msz);
    mdl64_t *mdls2 = (carry && !warm) ? (mdl64_t *)malloc(msz) : NULL;
    size_t words_cap = (size_t)2 * SEG_SYMS + 4 * RANS_LANES + 16;
    uint32_t *scr = (uint32_t *)malloc(words_cap * sizeof(uint32_t));
    uint8_t *tmp = (uint8_t *)malloc((size_t)out_cap);
    int32_t ret = -1;
    if (mdls && scr && tmp && out_cap > 1 && (!carry || warm || mdls2)) {
        if (carry && warm) {
            memcpy(mdls, carry, msz);
            int32_t n1 = enc_pass_b(recs, nrec, NULL, out + 1,
                                    out_cap - 1, mdls, scr,
                                    words_cap, lanes, nmdl, prog);
            if (n1 >= 0) {
                int mode = 0;
                n1 = compact_segs(out + 1, n1, &mode, bare_ok);
                out[0] = (uint8_t)(1 | (lane_bits << 1) |
                                   (mode == 1 ? 8 : mode == 2 ? 16 : 0));
                memcpy(carry, mdls, msz);
                ret = n1 + 1;
            }
        } else {
            uint16_t prior[7][MDL_ALPHA];
            for (int c = 0; c < 7; c++) mdl64_build_prior(prior[c], 1, c);
            int32_t n1 = enc_pass_b(recs, nrec, &prior[0][0], out + 1,
                                    out_cap - 1, mdls, scr,
                                    words_cap, lanes, nmdl, prog);
            for (int c = 0; c < 7; c++) mdl64_build_prior(prior[c], 0, c);
            int32_t n0 = enc_pass_b(recs, nrec, &prior[0][0], tmp, out_cap,
                                    mdls2 ? mdls2 : mdls, scr, words_cap,
                                    lanes, nmdl, prog);
            int m1 = 0, m0 = 0;
            if (n1 >= 0) n1 = compact_segs(out + 1, n1, &m1, bare_ok);
            if (n0 >= 0) n0 = compact_segs(tmp, n0, &m0, bare_ok);
            if (n0 >= 0 && (n1 < 0 || n0 < n1)) {
                out[0] = (uint8_t)((lane_bits << 1) |
                                   (m0 == 1 ? 8 : m0 == 2 ? 16 : 0));
                memcpy(out + 1, tmp, (size_t)n0);
                if (carry) memcpy(carry, mdls2 ? mdls2 : mdls, msz);
                ret = n0 + 1;
            } else if (n1 >= 0) {
                out[0] = (uint8_t)(1 | (lane_bits << 1) |
                                   (m1 == 1 ? 8 : m1 == 2 ? 16 : 0));
                if (carry) memcpy(carry, mdls, msz);
                ret = n1 + 1;
            }
        }
    }
    free(mdls); free(mdls2); free(scr); free(tmp);
    return ret;
}

/* encode one plane into a self-contained rANS stream; returns bytes written */
static int32_t enc_plane_stream(const int16_t *prev, int16_t *curr,
                                int w, int h,
                                uint8_t *out, int32_t out_cap)
{
    size_t maxrec = (size_t)w * h * 2 + 16;
    uint32_t *recs = (uint32_t *)malloc(maxrec * sizeof(uint32_t));
    if (!recs) return -1;

    /* pass A: run the causal machinery, record (model << 16 | value) */
    size_t nrec = 0;
    pstate_t S; pstate_init(&S);
    int has_prev = (prev != NULL);

    for (int y = 0; y < h; y++) {
        int16_t *row = curr + (size_t)y * w;
        const int16_t *top = y > 0 ? curr + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? curr + (size_t)(y - 2) * w : NULL;
        const int16_t *prow = has_prev ? prev + (size_t)y * w : NULL;

        int x = 0;
        while (x < w) {
            if (has_prev) {
                int can_run = (row[x] == prow[x]);
                recs[nrec++] = ((uint32_t)FLAG_MDL_IDX << 16) | (uint32_t)can_run;
                if (can_run) {
                    int run = 1;
                    while (x + run < w && row[x + run] == prow[x + run]) run++;
                    recs[nrec++] = ((uint32_t)RUN_MDL_IDX << 16) | (uint32_t)(run - 1);
                    update_run(&S, run);
                    x += run;
                    continue;
                }
            }

            int a = x > 0 ? row[x - 1] : (top ? top[x] : 0);
            int aa = x > 1 ? row[x - 2] : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d2 = (top && x < w - 1) ? top[x + 1] : b;

            int q1 = q_grad(d2 - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);

            int spreds[NSPAT];
            spat_preds(a, b, c, d2, aa, uu, spreds);
            int rs[NSPAT];
            for (int p = 0; p < NSPAT; p++) rs[p] = row[x] - spreds[p];

            int rt = 0;
            int has_t = has_prev ? 1 : 0;
            if (has_t) rt = row[x] - prow[x];

            if (flip) {
                for (int p = 0; p < NSPAT; p++) rs[p] = -rs[p];
                rt = -rt;
            }

            int mode = 0;
            int32_t best_A = S.As[ctx][0];
            for (int p = 1; p < NSPAT; p++) {
                if (S.As[ctx][p] < best_A) { best_A = S.As[ctx][p]; mode = p; }
            }
            if (has_t && S.At[ctx] < best_A) {
                best_A = S.At[ctx]; mode = NSPAT;
            }
            int32_t best_N = S.N[ctx];
            (void)best_N;

            int r = (mode == NSPAT) ? rt : rs[mode];
            uint32_t u = zigzag_enc(r);
            if (u > 0xFFFFu) u = 0xFFFFu;
            recs[nrec++] = ((uint32_t)mode << 16) | u;

            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) abs_s[p] = rs[p] < 0 ? -rs[p] : rs[p];
            int abs_t = has_t ? (rt < 0 ? -rt : rt) : abs_s[0];
            update_ctx(&S, ctx, abs_s, abs_t);
            x++;
        }
    }

    /* pass B handled by the shared stream wrapper (dual-theme pick) */
    /* v2 planes are concatenated; the decoder sizes each one from the
       segment lengths, so a bare (lengthless) blob would swallow the rest. */
    int32_t r = enc_stream_v(recs, nrec, N_MDL, 0, out, out_cap, NULL, 0, 0);
    free(recs);
    return r;
}

/* emit each segment's records in reverse under a given prior.
   Segment capacities follow the geometric schedule the decoder
   derives implicitly: 128,256,...,16384,16384,...              */
static int32_t enc_pass_b(const uint32_t *recs, size_t nrec,
                          const uint16_t *prior /* [5][MDL_ALPHA], NULL =
                                                 keep carried mdls */,
                          uint8_t *out, int32_t out_cap,
                          mdl64_t *mdls,
                          uint32_t *scr, size_t words_cap, int lanes,
                          int nmdl, int prog)
{
    if (prior) {
        for (int i = 0; i < nmdl; i++) {
            int cls = prog ? prog_cls[i] : mdl64_class((uint32_t)i);
            mdl64_init(&mdls[i], &prior[cls * MDL_ALPHA]);
        }
    }
    uint8_t *op = out;
    const uint32_t hi_midx = (uint32_t)(prog ? PM_HI : HI_MDL_IDX);

    if (ZEP(zlog)) {
        if (!zlogf) zlogf = fopen(ZEP(zlog), "w");
        if (zlogf)
            for (size_t i = 0; i < nrec; i++)
                fprintf(zlogf, "%c%u\t%u\n", prog ? 'P' : 'R',
                        recs[i] >> 16, recs[i] & 0xFFFFu);
    }
    size_t base = 0, cap = SEG_MIN; int sidx = 0;
    while (base < nrec) {
        /* norm before each segment - mirrors the decoder's next_seg,
           so a carried model's pending hits resolve at the same point
           on both sides (no norm after the last segment: the decoder
           never re-enters and must keep the same state) */
        for (int i2 = 0; i2 < nmdl; i2++) {
            mdl64_t *m = &mdls[i2];
            if (m->hits >= 48 || m->hits * 8 >= m->tot) mdl64_norm(m);
        }
        if (ZE(zmh) && prog) {
            uint64_t hh = 1469598103934665603ull;
            for (int i2 = 0; i2 < nmdl; i2++) {
                const mdl64_t *m = &mdls[i2];
                for (int s = 0; s < MDL_ALPHA; s++) {
                    hh ^= m->f[s]; hh *= 1099511628211ull;
                    hh ^= m->h[s]; hh *= 1099511628211ull;
                }
                hh ^= m->tot; hh *= 1099511628211ull;
            }
            fprintf(stderr, "MH E seg=%d hash=%016llx n=%d\n", sidx,
                    (unsigned long long)hh, (int)base);
        }

        size_t end = base + cap;
        if (end > nrec) end = nrec;
        if (cap < SEG_SYMS) cap <<= 1;
        sidx++;

        uint64_t x[RANS_LANES];
        for (int l = 0; l < lanes; l++) x[l] = RANS_L;
        ransw_t wtr; wtr.ptr = scr + words_cap;

        for (size_t i = end; i-- > base;) {
            int lane = (int)((i - base) & (size_t)(lanes - 1));
            uint32_t rec = recs[i];
            uint32_t midx = rec >> 16, v = rec & 0xFFFFu;
            mdl64_t *m = &mdls[midx];
            int cls;
            if (midx >= FLAG_MDL_IDX && midx < RUN_MDL_IDX) {
                rans_put(&x[lane], m->cum[v], m->f[v], &wtr);
                cls = (int)v;
            } else {
                /* RUN and RES share the length-class shape */
                if (v <= (uint32_t)(LEN_MID - 1)) {
                    rans_put(&x[lane], m->cum[v], m->f[v], &wtr);
                    cls = (int)v;
                } else if (v <= 317) {
                    if (midx == hi_midx) {   /* the HI model's own tail
                                                stays flat - no recursion */
                        rans_put_raw(&x[lane], v - LEN_MID, LEN_MID_BITS, &wtr);
                    } else {
                        /* nested bucket: (v-62)>>4 through the HI model,
                           4 raw low bits - the mid band is dominated by
                           small offsets that flat 8b wastes */
                        uint32_t off = v - LEN_MID;
                        rans_put_raw(&x[lane], off & 15, 4, &wtr);
                        mdl64_t *hm = &mdls[hi_midx];
                        rans_put(&x[lane], hm->cum[off >> 4],
                                 hm->f[off >> 4], &wtr);
                        hm->h[off >> 4]++; hm->tot++; hm->hits++;
                    }
                    rans_put(&x[lane], m->cum[LEN_MID], m->f[LEN_MID], &wtr);
                    cls = LEN_MID;
                } else if (ZE(zoldhi)) {
                    rans_put_raw(&x[lane], v - 318, LEN_HI_BITS, &wtr);
                    rans_put(&x[lane], m->cum[LEN_HI], m->f[LEN_HI], &wtr);
                    cls = LEN_HI;
                } else {
                    /* hi escape: bucket len-coded through the HI model on
                       the same lane (pulled right after the class sym,
                       so pushed right before it), then 6 raw low bits */
                    uint32_t off = v - 318, hb = off >> 6;
                    mdl64_t *hm = &mdls[hi_midx];
                    if (ZE(zhi)) fprintf(stderr, "HI E i=%ld lane=%d v=%u hb=%u prog=%d\n",
                                               (long)(i - base), lane, v, hb, prog);
                    rans_put_raw(&x[lane], off & 63, 6, &wtr);
                    int hcls;
                    if (hb <= (uint32_t)(LEN_MID - 1)) {
                        rans_put(&x[lane], hm->cum[hb], hm->f[hb], &wtr);
                        hcls = (int)hb;
                    } else if (hb <= 317u) {
                        rans_put_raw(&x[lane], hb - LEN_MID, LEN_MID_BITS, &wtr);
                        rans_put(&x[lane], hm->cum[LEN_MID], hm->f[LEN_MID], &wtr);
                        hcls = LEN_MID;
                    } else {
                        rans_put_raw(&x[lane], hb - 318, LEN_HI_BITS, &wtr);
                        rans_put(&x[lane], hm->cum[LEN_HI], hm->f[LEN_HI], &wtr);
                        hcls = LEN_HI;
                    }
                    hm->h[hcls]++; hm->tot++; hm->hits++;
                    rans_put(&x[lane], m->cum[LEN_HI], m->f[LEN_HI], &wtr);
                    cls = LEN_HI;
                }
            }
            m->h[cls]++; m->tot++; m->hits++;
        }

        /* lane 0 must be read first -> flushed last in the backward buffer */
        for (int l = lanes - 1; l >= 0; l--) {
            *--wtr.ptr = (uint32_t)(x[l] >> 32);
            *--wtr.ptr = (uint32_t)x[l];
        }

        uint32_t nbytes = (uint32_t)((scr + words_cap - wtr.ptr) * 4);
        if ((int64_t)(op - out) + 4 + nbytes > out_cap) return -1;
        wr_u32le(op, nbytes); op += 4;
        for (const uint32_t *q = wtr.ptr; q < scr + words_cap; q++) {
            wr_u32le(op, *q); op += 4;
        }
        base = end;
    }

    return (int32_t)(op - out);
}

int32_t zv_encode_frame_v2(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                           int w, int h, uint8_t *out_buf, int32_t out_cap)
{
    if (out_cap < 1) return -1;

    size_t n = (size_t)w * h;
    int16_t *pY  = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *pCo = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *pCg = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cY  = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cCo = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cCg = (int16_t*)malloc(n * sizeof(int16_t));
    if (!pY || !pCo || !pCg || !cY || !cCo || !cCg) {
        free(pY); free(pCo); free(pCg); free(cY); free(cCo); free(cCg);
        return -1;
    }

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);
    deinterleave_planes(curr_ycocg, cY, cCo, cCg, w, h);

    out_buf[0] = 0;
    uint8_t *op = out_buf + 1;
    int32_t r;
    r = enc_plane_stream(prev_ycocg ? pY  : NULL, cY , w, h, op, out_cap - (int32_t)(op - out_buf));
    if (r < 0) goto fail; op += r;
    r = enc_plane_stream(prev_ycocg ? pCo : NULL, cCo, w, h, op, out_cap - (int32_t)(op - out_buf));
    if (r < 0) goto fail; op += r;
    r = enc_plane_stream(prev_ycocg ? pCg : NULL, cCg, w, h, op, out_cap - (int32_t)(op - out_buf));
    if (r < 0) goto fail; op += r;

    free(pY); free(pCo); free(pCg); free(cY); free(cCo); free(cCg);
    return (int32_t)(op - out_buf);
fail:
    free(pY); free(pCo); free(pCg); free(cY); free(cCo); free(cCg);
    return -1;
}

int32_t zv_decode_frame_v2(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                           int w, int h, const uint8_t *in_buf, int32_t in_size)
{
    if (in_size < 1) return -1;

    size_t n = (size_t)w * h;
    int16_t *pY  = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *pCo = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *pCg = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cY  = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cCo = (int16_t*)malloc(n * sizeof(int16_t));
    int16_t *cCg = (int16_t*)malloc(n * sizeof(int16_t));
    if (!pY || !pCo || !pCg || !cY || !cCo || !cCg) {
        free(pY); free(pCo); free(pCg); free(cY); free(cCo); free(cCg);
        return -1;
    }

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);

    const uint8_t *ip = in_buf + 1;
    int32_t left = in_size - 1, r;
    r = dec_plane_stream(prev_ycocg ? pY  : NULL, cY , w, h, ip, left);
    if (r < 0) goto fail; ip += r; left -= r;
    r = dec_plane_stream(prev_ycocg ? pCo : NULL, cCo, w, h, ip, left);
    if (r < 0) goto fail; ip += r; left -= r;
    r = dec_plane_stream(prev_ycocg ? pCg : NULL, cCg, w, h, ip, left);
    if (r < 0) goto fail; ip += r; left -= r;

    interleave_planes(cY, cCo, cCg, curr_ycocg, w, h);

    free(pY); free(pCo); free(pCg); free(cY); free(cCo); free(cCg);
    return (int32_t)(ip - in_buf);
fail:
    free(pY); free(pCo); free(pCg); free(cY); free(cCo); free(cCg);
    return -1;
}

int32_t zv_encode_plane_stripe_v2(const int16_t *prev_plane,
                                  int16_t *curr_plane,
                                  int w, int stripe_h,
                                  uint8_t *out_buf, int32_t out_cap)
{
    return enc_plane_stream(prev_plane, curr_plane, w, stripe_h,
                            out_buf, out_cap);
}

int32_t zv_decode_plane_stripe_v2(const int16_t *prev_plane,
                                  int16_t *curr_plane,
                                  int w, int stripe_h,
                                  const uint8_t *in_buf, int32_t in_size)
{
    return dec_plane_stream(prev_plane, curr_plane, w, stripe_h,
                            in_buf, in_size);
}

/* ======================= v14: tile-program layer =======================
 * Opcodes: RUN (whole tile = prev), COPY mv (PRED with shifted temporal
 * reference = integer-pel motion compensation), PAL (palette + indices),
 * PRED (existing causal machinery), RAW (literal fallback). The encoder
 * scores every legal op per tile under the current frozen CDFs and emits
 * the arg-min - encoding is program search over a tiny VM.
 */

/* fixed-point symbol cost in 1/256 bits, from a frozen CDF */
static int log2_q8(uint32_t f) {
    int e = 31 - __builtin_clz(f | 1u);
    uint32_t frac = e ? (((f - (1u << e)) << 8) >> e) : 0;
    return e * 256 + (int)frac;
}
static int msym_cost_q8(const mdl64_t *m, int s) {
    return (RANS_SCALE << 8) - log2_q8(m->f[s]);
}
static int mlensym_cost_q8(const mdl64_t *m, const mdl64_t *hm, uint32_t v) {
    int cls = v < (uint32_t)LEN_MID ? (int)v :
              (v < 318u ? LEN_MID : LEN_HI);
    int c = msym_cost_q8(m, cls);
    if (cls == LEN_MID) c += LEN_MID_BITS << 8;
    else if (cls == LEN_HI) {
        uint32_t hb = (v - 318) >> 6;
        int hc = hb < (uint32_t)LEN_MID ? (int)hb :
                 (hb < 318u ? LEN_MID : LEN_HI);
        c += msym_cost_q8(hm, hc) + (6 << 8);
        if (hc == LEN_MID) c += LEN_MID_BITS << 8;
        else if (hc == LEN_HI) c += LEN_HI_BITS << 8;
    }
    return c;
}
static void mdl_bump_raw(mdl64_t *m, mdl64_t *hm, uint32_t midx,
                         uint32_t v) {
    int isfl = midx >= FLAG_MDL_IDX && midx < RUN_MDL_IDX;
    int cls = isfl ? (int)v :
              (v < (uint32_t)LEN_MID ? (int)v :
               (v < 318u ? LEN_MID : LEN_HI));
    if (ztxn_n) mdu_log(m);   /* nested txn: outer level may roll back */
    m->h[cls]++; m->tot++; m->hits++;
    if (!isfl && cls == LEN_HI) {
        uint32_t hb = (v - 318) >> 6;
        int hc = hb < (uint32_t)LEN_MID ? (int)hb : (hb < 318u ? LEN_MID : LEN_HI);
        if (ztxn_n) mdu_log(hm);
        hm->h[hc]++; hm->tot++; hm->hits++;
    }
}
static void mdl_bump(mdl64_t *m, mdl64_t *hm, uint32_t midx, uint32_t v) {
    /* inside a split probe the bump is deferred: the emitted records
       already carry (midx,v) and the win path replays them verbatim.
       Probe evals then price against the frozen pre-probe models -
       identical to the old emit=0 probe - and rollback never touches
       the models.  Scratch models (palette eval) still bump. */
    if (ztxn_n &&
        ((m >= ztxn_cm && m < ztxn_cm + N_MDL) ||
         (m >= ztxn_pm && m < ztxn_pm + NPROG_MDL)))
        return;
    mdl_bump_raw(m, hm, midx, v);
}
static inline int rec_cost_q8(uint32_t rec, const mdl64_t *cm) {
    uint32_t midx = rec >> 16, v = rec & 0xFFFFu;
    return (midx >= FLAG_MDL_IDX && midx < RUN_MDL_IDX)
           ? msym_cost_q8(&cm[midx], (int)v)
           : mlensym_cost_q8(&cm[midx], &cm[HI_MDL_IDX], v);
}

static int recs_cost_q8(const uint32_t *recs, size_t n, const mdl64_t *cm) {
    int c = 0;
    for (size_t i = 0; i < n; i++)
        c += rec_cost_q8(recs[i], cm);
    return c;
}
static void cm_norm_sweep(mdl64_t *m, int n) {
    for (int i = 0; i < n; i++)
        if (m[i].hits >= 48 || m[i].hits * 8 >= m[i].tot) mdl64_norm(&m[i]);
}

/* ---- learned linear predictor (the seed of the NN op) ----
   Q8 fixed-point weights over causal features; weights are fitted by
   the encoder per plane and carried as the first program records. */
/* nearest-integer residual quantization: |r - qres(r)*qs| <= qs/2 */
static inline int qres(int r, int qs) {
    return qs > 1 ? (r + (r >= 0 ? qs / 2 : -(qs / 2))) / qs : r;
}

#define N_NNC 16   /* learned-predictor weight classes:
                      8 local-gradient-energy buckets x direction bit */

static inline int nn_class(int a, int b, int c, int d2) {
    int eh = c - a; if (eh < 0) eh = -eh;
    int ev = d2 - b; if (ev < 0) ev = -ev;
    int dd = b - c;  if (dd < 0) dd = -dd;
    int e = eh + ev + dd, q;
    if      (e == 0)  q = 0;
    else if (e <= 2)  q = 1;
    else if (e <= 5)  q = 2;
    else if (e <= 11) q = 3;
    else if (e <= 23) q = 4;
    else if (e <= 47) q = 5;
    else if (e <= 95) q = 6;
    else              q = 7;
    return q * 2 + (eh > ev);
}

static int llp_predict(const int16_t *Wt, int a, int b, int c, int d2,
                       int t, int lu, int aa, int uu,
                       int pl, int pr, int pu, int pd, int e) {
    const int16_t *W = Wt + nn_class(a, b, c, d2) * N_NNW;
    int64_t s = (int64_t)W[0] * a + (int64_t)W[1] * b + (int64_t)W[2] * c
              + (int64_t)W[3] * d2 + (int64_t)W[4] * t
              + (int64_t)W[5] * med_pred(a, b, c) + (int64_t)W[6] * (a + b - c)
              + (int64_t)W[7] * lu + (int64_t)W[8] * 256
              + (int64_t)W[9] * aa + (int64_t)W[10] * uu
              + (int64_t)W[11] * pl + (int64_t)W[12] * pr
              + (int64_t)W[13] * pu + (int64_t)W[14] * pd
              + (int64_t)W[15] * e + 128;
    s >>= 8;
    if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
    return (int)s;
}

/* solve the 9x9 ridge-regularized normal equations into Q8 weights */
static int solve_llp(double A[N_NNW][N_NNW + 1], int16_t *W)
{
    for (int i = 0; i < N_NNW; i++) A[i][i] += 4096.0;
    for (int col = 0; col < N_NNW; col++) {
        int piv = col;
        for (int r = col + 1; r < N_NNW; r++)
            if (fabs(A[r][col]) > fabs(A[piv][col])) piv = r;
        if (fabs(A[piv][col]) < 1e-9) return 0;
        if (piv != col)
            for (int j = col; j <= N_NNW; j++) {
                double t = A[piv][j]; A[piv][j] = A[col][j]; A[col][j] = t;
            }
        for (int r = col + 1; r < N_NNW; r++) {
            double m = A[r][col] / A[col][col];
            for (int j = col; j <= N_NNW; j++) A[r][j] -= m * A[col][j];
        }
    }
    double ws[N_NNW];
    for (int i = N_NNW - 1; i >= 0; i--) {
        double s = A[i][N_NNW];
        for (int j = i + 1; j < N_NNW; j++) s -= A[i][j] * ws[j];
        ws[i] = s / A[i][i];
    }
    for (int i = 0; i < N_NNW; i++) {
        long q = lround(ws[i] * 256.0);
        if (q > 32767) q = 32767; else if (q < -32768) q = -32768;
        W[i] = (int16_t)q;
    }
    return 1;
}

/* ridge-regularized least squares over the plane's pixels; features are
   computed with the exact tile-causal rules the decoder uses. */
static int train_llp(const int16_t *prev, const int16_t *curr,
                     int w, int h, const int16_t *luma, int16_t *Wtab,
                     int qs, double *ratio_out)
{
    int16_t WtabA[N_NNC * N_NNW], WtabB[N_NNC * N_NNW];
    int have_g = 0;
    double se_llp = 0, se_sp = 0;
    /* three passes toward self-consistency: pass 0 solves the base taps
       (e column zero), passes 1-2 replay with the causal QUANTIZED
       residual computed under the previous pass's table - including its
       e weight - so the e feature matches what the decoder feeds back */
    for (int pass = 0; pass < 3; pass++) {
    int16_t *Wdst = pass == 0 ? WtabA : (pass == 1 ? WtabB : Wtab);
    const int16_t *Wsrc = pass == 0 ? NULL : (pass == 1 ? WtabA : WtabB);
    double A[N_NNC][N_NNW][N_NNW + 1];
    double G[N_NNW][N_NNW + 1];
    long cnt[N_NNC];
    memset(A, 0, sizeof(A));
    memset(G, 0, sizeof(G));
    memset(cnt, 0, sizeof(cnt));
    for (int ty = 0; ty < h; ty += TILE_SZ) {
        int th = h - ty < TILE_SZ ? h - ty : TILE_SZ;
        for (int tx = 0; tx < w; tx += TILE_SZ) {
            int tw = w - tx < TILE_SZ ? w - tx : TILE_SZ;
            int xend = tx + tw;
            int e_prev = 0;
            /* subsample rows 2:1 - e chains live within a row so x stays
               dense; row skipping keeps every sample's features honest */
            for (int y = ty; y < ty + th; y += 2) {
                const int16_t *row = curr + (size_t)y * w;
                const int16_t *top = y > 0 ? curr + (size_t)(y - 1) * w : NULL;
            const int16_t *top2 = y > 1 ? curr + (size_t)(y - 2) * w : NULL;
                const int16_t *prow = prev ? prev + (size_t)y * w : NULL;
                for (int x = tx; x < xend; x += 2) {
                    int a = x > 0 ? row[x - 1] : (top ? top[x] : 0);
            int aa = x > 1 ? row[x - 2] : a;
                    int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
                    int c = (top && x > 0) ? top[x - 1] : a;
                    int d2;
                    if (!top)         d2 = b;
                    else if (y == ty) {
                        /* a mid-band first row (split child) sees the row
                           above only up to the containing tile's edge */
                        int rl = (ty % TILE_SZ == 0) ? w
                               : ((tx & ~(2 * tw - 1)) + 2 * tw);
                        if (rl > w) rl = w;
                        d2 = (x + 1 < rl) ? top[x + 1] : b;
                    } else            d2 = (x + 1 < xend) ? top[x + 1] : b;
                    double f[N_NNW] = {
                        (double)a, (double)b, (double)c, (double)d2,
                        prow ? (double)prow[x] : 0.0,
                        (double)med_pred(a, b, c), (double)(a + b - c),
                        luma ? (double)luma[(size_t)y * w + x] : 0.0, 256.0,
                        (double)aa, (double)uu,
                        prow ? (double)prow[x > 0 ? x - 1 : 0] : 0.0,
                        prow ? (double)prow[x + 1 < w ? x + 1 : w - 1] : 0.0,
                        (prev && y > 0) ? (double)prev[(size_t)(y - 1) * w + x] : 0.0,
                        (prev && y + 1 < h) ? (double)prev[(size_t)(y + 1) * w + x] : 0.0,
                        (double)(qs > 5 ? 0 : e_prev)
                    };
                    int cls = nn_class(a, b, c, d2);
                    cnt[cls]++;
                    /* Gram matrices are symmetric - accumulate the upper
                       triangle only and mirror once before the solve */
                    for (int i = 0; i < N_NNW; i++) {
                        double fi = f[i];
                        for (int j = i; j < N_NNW; j++) {
                            double p = fi * f[j];
                            A[cls][i][j] += p;
                            G[i][j] += p;
                        }
                        A[cls][i][N_NNW] += fi * row[x];
                        G[i][N_NNW] += fi * row[x];
                    }
                    if (Wsrc) {
                        int p0 = llp_predict(Wsrc, a, b, c, d2,
                                             prow ? prow[x] : 0,
                                             luma ? luma[(size_t)y * w + x] : 0,
                                             aa, uu,
                                             prow ? prow[x > 0 ? x - 1 : 0] : 0,
                                             prow ? prow[x + 1 < w ? x + 1 : w - 1] : 0,
                                             (prev && y > 0) ? prev[(size_t)(y - 1) * w + x] : 0,
                                             (prev && y + 1 < h) ? prev[(size_t)(y + 1) * w + x] : 0,
                                             e_prev);
                        e_prev = qres(row[x] - p0, qs) * qs;
                        if (pass == 2) {
                            int r0 = row[x] - p0;
                            se_llp += r0 < 0 ? -r0 : r0;
                            int sp = a + b - c;
                            int rs = row[x] - sp;
                            se_sp += rs < 0 ? -rs : rs;
                        }
                    }
                }
            }
        }
    }
    for (int k = 0; k < N_NNC; k++)
        for (int i = 0; i < N_NNW; i++)
            for (int j = 0; j < i; j++) A[k][i][j] = A[k][j][i];
    for (int i = 0; i < N_NNW; i++)
        for (int j = 0; j < i; j++) G[i][j] = G[j][i];
    /* solve the global system first; it doubles as the fallback for
       classes with too little data to fit reliably */
    int16_t Wg[N_NNW];
    have_g = solve_llp(G, Wg);
    for (int k = 0; k < N_NNC; k++) {
        int16_t *W = Wdst + k * N_NNW;
        if (!have_g || cnt[k] < 32 || !solve_llp(A[k], W))
            memcpy(W, have_g ? Wg : (int16_t[N_NNW]){0},
                   N_NNW * sizeof(int16_t));
    }
    }
    if (ratio_out)
        *ratio_out = se_sp > 0 ? se_llp / se_sp : 1.0;
    if (ZE(zwd) && se_sp > 0)
        fprintf(stderr, "WRATIO %.3f\n", se_llp / se_sp);
    return have_g;
}

/* run the causal per-pixel machinery over a tile; emits FLAG/RUN/RES
   records into recs and updates S exactly like the v12 pass-A loop.
   (dx,dy) shifts the temporal reference - PRED uses (0,0), COPY its MV.
   W != NULL swaps the argmin predictor for the learned linear one. */


/* Temporal sample. In-bounds motion never reaches the clamps.
   A vector that hangs off a full frame uses the same edge clamp
   the MVRUN / dec_pred_tile paths already apply, so a 1px scroll
   can keep the columns that are still on the previous frame. */
static inline int16_t prev_at(const int16_t *prev, int w, int h, int x, int y) {
    if (x < 0) x = 0; else if (x >= w) x = w - 1;
    if (y < 0) y = 0; else if (y >= h) y = h - 1;
    return prev[(size_t)y * (size_t)w + x];
}

/* In-bounds MV is a memcpy; only the overhang pixels clamp. */
static inline void copy_shift_row(int16_t *dst, const int16_t *src,
                                  int x0, int n, int dx, int w)
{
    int s0 = x0 + dx, s1 = x0 + n + dx;
    if (s0 >= 0 && s1 <= w) {
        memmove(dst + x0, src + x0 + dx, (size_t)n * sizeof(int16_t));
        return;
    }
    for (int x = x0; x < x0 + n; x++) {
        int sx = x + dx;
        if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
        dst[x] = src[sx];
    }
}

/* Consecutive pixels after `x` whose causal predictor (frozen S) is
   already exact under qs.  Returns the run length including the
   already-exact pixel at `x` (so 1 means "only this pixel").
   Does not mutate S or recon: flag 5 writes pred per pixel and
   skips update_ctx, so both ends keep S frozen across the run.
   pmatch after flag 5 is 4 (NFLG_CTX is 5×4, never 6). */
static int zres_run_len(const int16_t *row, const int16_t *top,
                        const int16_t *top2, const int16_t *prow,
                        const int16_t *prev, const int16_t *curr,
                        const int16_t *recon, const int16_t *luma,
                        const int16_t *W, const pstate_t *S,
                        int x, int y, int xend, int tx, int tw,
                        int w, int h, int dx, int dy, int qs,
                        int has_t, int ty, int inb,
                        int pred0, int a0)
{
    int zr = 1;
    int left = pred0;
    int left2 = (x > 0) ? a0 : pred0;
    for (int xi = x + 1; xi < xend; xi++) {
        int xa = left;
        int xaa = left2;
        int xb = top ? top[xi] : xa;
        int xuu = top2 ? top2[xi] : xb;
        int xc = (top && xi > 0) ? top[xi - 1] : xa;
        int xd2;
        if (!top) xd2 = xb;
        else if (y == ty) {
            int rl = (ty % TILE_SZ == 0) ? w
                   : ((tx & ~(2 * tw - 1)) + 2 * tw);
            if (rl > w) rl = w;
            xd2 = (xi + 1 < rl) ? top[xi + 1] : xb;
        } else xd2 = (xi + 1 < xend) ? top[xi + 1] : xb;
        int sx = xi + dx;
        if (!inb) {
            if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
        }
        int t = (has_t && prow) ? prow[sx] : 0;
        int lu = luma ? luma[(size_t)y * (size_t)w + xi] : 0;
        int pl = 0, pr = 0, pu = 0, pd = 0;
        if (W && has_t && prev && prev != (recon ? recon : curr)) {
            int px = xi + dx;
            if (px < 0) px = 0; else if (px >= w) px = w - 1;
            int py = y + dy;
            if (py < 0) py = 0; else if (py >= h) py = h - 1;
            pl = prev[(size_t)py * (size_t)w + (px ? px - 1 : 0)];
            pr = prev[(size_t)py * (size_t)w + (px < w - 1 ? px + 1 : w - 1)];
            pu = prev[(size_t)(py ? py - 1 : 0) * (size_t)w + px];
            pd = prev[(size_t)(py < h - 1 ? py + 1 : h - 1) * (size_t)w + px];
        }
        int pi;
        if (W) {
            pi = llp_predict(W, xa, xb, xc, xd2, t, lu, xaa, xuu,
                             pl, pr, pu, pd, 0);
        } else {
            int flip;
            int ctx = ctx_index(q_grad(xd2 - xb), q_grad(xb - xc),
                                q_grad(xc - xa), &flip);
            int mode = 0;
            int32_t best_A = S->As[ctx][0];
            for (int p = 1; p < NSPAT; p++)
                if (S->As[ctx][p] < best_A) {
                    best_A = S->As[ctx][p];
                    mode = p;
                }
            if (has_t && S->At[ctx] < best_A) pi = t;
            else {
                int sp[NSPAT];
                spat_preds(xa, xb, xc, xd2, xaa, xuu, sp);
                pi = sp[mode];
            }
        }
        if (qres(row[xi] - pi, qs) != 0) break;
        zr++;
        left2 = left;
        left = pi;
    }
    return zr;
}

static void sim_pred_tile(const int16_t *prev, const int16_t *curr,
                          int w, int h,
                          int tx, int ty, int tw, int th,
                          int dx, int dy, int has_prev,
                          pstate_t *S, uint32_t *recs, size_t *pnrec,
                          const int16_t *W, const int16_t *luma,
                          int16_t *recon, int qs, const mdl64_t *cm,
                          int lmode, int *zok, pundo_t *U, int lim,
                          int *rcq8, pxcache_t *pxc)
{
    pundo_t *pu2s = (zpu2 && S == zpu2_s) ? zpu2 : NULL;
                    /* hoisted TLS read, bound to this state */
    /* lim: caller's remaining q8 budget (best-so-far minus this op's
       fixed cost).  Accumulate the emitted records' cost inline and
       bail the moment the prefix alone can no longer win - the partial
       record list still prices >= lim, so the candidate loses exactly
       as if the sim had run to completion. */
    int rcost = 0;
    int erq = 0;   /* previous pixel's decoded residual (LLP e tap) */
    /* has_prev == 2: full-frame overhang. Clamp samples to the plane,
       matching MVRUN / dec_pred_tile when yoff==0 and h==hfull.
       Every other caller keeps the unclamped pointer, so a stripe may
       still read a negative local sy into the previous band. */
    int edge = (has_prev == 2);
    if (edge) has_prev = 1;
    int inb = !edge;
#define TSX(xx) (inb ? (xx) : ((xx) < 0 ? 0 : (xx) >= w ? w - 1 : (xx)))
    /* recon: what the decoder will actually have at this plane. Causal
       neighbors (a,b,c,d2) MUST come from it once quantization can make
       recon != curr; residuals still target the original curr. */
    const int16_t *ctx_src = recon ? recon : curr;
    size_t nr = *pnrec;
    (void)cm;
    for (int y = ty; y < ty + th; y++) {
        const int16_t *row = curr + (size_t)y * w;
        int16_t *rrow = recon ? recon + (size_t)y * w : NULL;
        const int16_t *top = y > 0 ? ctx_src + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? ctx_src + (size_t)(y - 2) * w : NULL;
        int sy = y + dy;
        if (!inb) {
            if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
        }
        const int16_t *prow = has_prev ? prev + (size_t)sy * w : NULL;
        int x = tx, xend = tx + tw;
        int pmatch = 0;         /* last flag value - runs cluster */
        int zleft = 0;          /* remaining pixels of a flag-5 run */
        while (x < xend) {
            int a = x > 0 ? (rrow ? rrow[x - 1] : row[x - 1])
                          : (top ? top[x] : 0);
            int aa = x > 1 ? (rrow ? rrow[x - 2] : row[x - 2]) : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            /* top-right is causal only inside the tile on non-first rows
               (same-band tiles decode later); the first row may use the
               whole previous band, which is already decoded. */
            int d2;
            if (!top)         d2 = b;
            else if (y == ty) {
                int rl = (ty % TILE_SZ == 0) ? w
                       : ((tx & ~(2 * tw - 1)) + 2 * tw);
                if (rl > w) rl = w;
                d2 = (x + 1 < rl) ? top[x + 1] : b;
            } else            d2 = (x + 1 < xend) ? top[x + 1] : b;
            /* route flag/run/residual through this ctx's own
               temporal-residual history: At[ctx]/N[ctx] estimates
               exactly the magnitude the flag asks about - flat-ctx
               pixels go near-free, textured stay honest.  Same on
               the decoder side, so the slot is never transmitted. */
            pxcache_t *pe = pxc ? pxc + (y - ty) * tw + (x - tx) : NULL;
            int flip, ctx, gbb;
            int spreds[NSPAT];
            if (pe && pe->ctxf >= 0) {
                ctx = pe->ctxf >> 1; flip = pe->ctxf & 1; gbb = pe->gbb;
                memcpy(spreds, pe->sp, sizeof spreds);
            } else {
                int q1 = q_grad(d2 - b);
                int q2 = q_grad(b - c);
                int q3 = q_grad(c - a);
                ctx = ctx_index(q1, q2, q3, &flip);
                gbb = grad_bucket(a, b, c, d2, qs);
                spat_preds(a, b, c, d2, aa, uu, spreds);
                if (pe) {
                    pe->ctxf = (int16_t)(ctx * 2 + flip);
                    pe->gbb = (int16_t)gbb;
                    memcpy(pe->sp, spreds, sizeof spreds);
                }
            }
            /* floor(At/N) <= T  <=>  At < (T+1)*N  for At,N >= 0 -
               the three bucket compares replace a per-pixel divide */
            int32_t atv = S->At[ctx];
            int64_t nn = S->N[ctx] > 0 ? S->N[ctx] : 1;
            int gb_a = atv < (int64_t)(qs + 1) * nn ? 0 :
                       atv < (int64_t)(qs * 4 + 1) * nn ? 1 :
                       atv < (int64_t)(qs * 16 + 1) * nn ? 2 : 3;
            int gb = gb_a + gbb;
            if (gb > 3) gb = 3;
            int fb = pmatch * 4 + gbb;

            int has_t = has_prev;

            int mode = 0;
            int32_t best_A = S->As[ctx][0];
            for (int p = 1; p < NSPAT; p++) {
                if (S->As[ctx][p] < best_A) { best_A = S->As[ctx][p]; mode = p; }
            }
            if (has_t && S->At[ctx] < best_A) {
                best_A = S->At[ctx]; mode = NSPAT;
            }

            int pred;
            if (W) {
                int t = has_t ? prow[TSX(x + dx)] : 0;
                int lu = luma ? luma[(size_t)y * w + x] : 0;
                int pl = 0, pr = 0, pu = 0, pd = 0;
                /* the cross-neighborhood taps read prev-frame pixels -
                   for ICOPY prev aliases the frame being decoded and the
                   right/down taps would hit not-yet-decoded pixels */
                if (has_t && prev != (recon ? recon : curr)) {
                    int px = x + dx;
                    if (px < 0) px = 0; else if (px >= w) px = w - 1;
                    int py = y + dy;
                    if (py < 0) py = 0; else if (py >= h) py = h - 1;
                    pl = prev[(size_t)py * w + (px ? px - 1 : 0)];
                    pr = prev[(size_t)py * w + (px < w - 1 ? px + 1 : w - 1)];
                    pu = prev[(size_t)(py ? py - 1 : 0) * w + px];
                    pd = prev[(size_t)(py < h - 1 ? py + 1 : h - 1) * w + px];
                }
                pred = llp_predict(W, a, b, c, d2, t, lu,
                                   aa, uu, pl, pr, pu, pd,
                                   qs > 5 ? 0 : erq);
                mode = NSPAT;
            } else {
                pred = (mode == NSPAT) ? prow[TSX(x + dx)] : spreds[mode];
            }
            int rq = qres(row[x] - pred, qs);
            uint32_t u0 = zigzag_enc(flip ? -rq : rq);
            if (u0 > 0xFFFFu) u0 = 0xFFFFu;

            if (zleft) {
                /* remainder of a flag-5 zero-residual run: write pred,
                   keep S frozen (decoder does the same). */
                if (rrow) rrow[x] = (int16_t)pred;
                erq = 0;
                pmatch = 4;
                zleft--;
                x++;
                continue;
            }

            int mflag = 0;
            /* lmode 3 = intra-flag mode (OP_PREDIR): flags on, but the
               temporal copy-run (mflag 1) is out of the alphabet - the
               decoder rejects it, keeping prev-less streams unambiguous.
               lmode 5 = OP_PREDX: same flag alphabets, residual fold. */
            int fold_res = (lmode == 4 || lmode == 5);
            int use_flags = (lmode == 0 && has_prev) || lmode == 3 || lmode == 5;
            int intra_fl = lmode == 3 || (lmode == 5 && !has_prev);
            if (use_flags) {
                /* flag: 0 residual, 1 temporal copy-run, 2 left run,
                   3 up run, 4 zero-residual (no u), 5 zero-residual
                   run (pred per pixel, RUN length, pmatch=4 after).
                   Copy flags 1-3 only pay when rq != 0 on inter;
                   intra cost-competes runs even on exact pred. */
                int run = 1;
                uint32_t rmidx = pred_res_midx(mode, gb, ctx, fold_res);
                if (intra_fl) {
                    /* intra flag set {0,2,3,4,5}: left/up/zero-res
                       runs compete by modeled cost against per-pixel
                       symbols, so a flat row collapses even where the
                       predictor is already exact. */
                    int lr = 0, ur = 0;
                    if (qres(row[x] - a, qs) == 0) {
                        lr = 1;
                        while (x + lr < xend &&
                               qres(row[x + lr] - a, qs) == 0) lr++;
                    }
                    if (top && qres(row[x] - b, qs) == 0) {
                        ur = 1;
                        while (x + ur < xend &&
                               qres(row[x + ur] - top[x + ur], qs) == 0) ur++;
                    }
                    int f1 = msym_cost_q8(&cm[FLAG_MDL_IDX + fb], 0)
                           + mlensym_cost_q8(&cm[rmidx],
                                             &cm[HI_MDL_IDX], u0);
                    if (rq == 0) {
                        int f4 = msym_cost_q8(&cm[FLAG_MDL_IDX + fb], 4);
                        if (f4 < f1) { f1 = f4; mflag = 4; }
                    }
                    int c2 = lr >= 2
                           ? msym_cost_q8(&cm[FLAG_MDL_IDX + fb], 2)
                           + mlensym_cost_q8(
                                 &cm[RUN_MDL_IDX + pmatch * 4 + run_bucket(S)],
                                 &cm[HI_MDL_IDX], (uint32_t)(lr - 1))
                           : 0x7FFFFFFF;
                    int c3 = ur >= 2
                           ? msym_cost_q8(&cm[FLAG_MDL_IDX + fb], 3)
                           + mlensym_cost_q8(
                                 &cm[RUN_MDL_IDX + pmatch * 4 + run_bucket(S)],
                                 &cm[HI_MDL_IDX], (uint32_t)(ur - 1))
                           : 0x7FFFFFFF;
                    int c5 = 0x7FFFFFFF;
                    int zr = 0;
                    if (rq == 0) {
                        zr = zres_run_len(row, top, top2, prow, prev,
                                          curr, recon, luma, W, S,
                                          x, y, xend, tx, tw, w, h,
                                          dx, dy, qs, has_t, ty, inb,
                                          pred, a);
                        if (zr >= 2)
                            c5 = msym_cost_q8(&cm[FLAG_MDL_IDX + fb], 5)
                               + mlensym_cost_q8(
                                     &cm[RUN_MDL_IDX + pmatch * 4 + run_bucket(S)],
                                     &cm[HI_MDL_IDX], (uint32_t)(zr - 1));
                    }
                    /* a run beats covering the same span one flag at a
                       time; between directions prefer the lower cost */
                    if (zr >= 2 && c5 <= c2 && c5 <= c3
                        && (int64_t)c5 <= (int64_t)f1 * zr) {
                        mflag = 5; run = zr;
                    } else if (c2 <= c3 && (int64_t)c2 <= (int64_t)f1 * lr) {
                        mflag = 2; run = lr;
                    } else if ((int64_t)c3 <= (int64_t)f1 * ur) {
                        mflag = 3; run = ur;
                    }
                } else if (qres(row[x] - prow[TSX(x + dx)], qs) == 0)
                    mflag = 1;
                else if (rq == 0) {
                    /* flag 4: one exact pred, no u. flag 5: a run of
                       exact pred. Both compete against flag0+u0. */
                    int zr = zres_run_len(row, top, top2, prow, prev,
                                          curr, recon, luma, W, S,
                                          x, y, xend, tx, tw, w, h,
                                          dx, dy, qs, has_t, ty, inb,
                                          pred, a);
                    int f0 = msym_cost_q8(&cm[FLAG_MDL_IDX + fb], 0)
                           + mlensym_cost_q8(&cm[rmidx],
                                             &cm[HI_MDL_IDX], u0);
                    int f4 = msym_cost_q8(&cm[FLAG_MDL_IDX + fb], 4);
                    int f5 = 0x7FFFFFFF;
                    if (zr >= 2)
                        f5 = msym_cost_q8(&cm[FLAG_MDL_IDX + fb], 5)
                           + mlensym_cost_q8(
                                 &cm[RUN_MDL_IDX + pmatch * 4 + run_bucket(S)],
                                 &cm[HI_MDL_IDX], (uint32_t)(zr - 1));
                    if (zr >= 2 && (int64_t)f5 <= (int64_t)f0 * zr
                        && (int64_t)f5 <= (int64_t)f4 * zr) {
                        mflag = 5; run = zr;
                    } else if (f4 < f0)
                        mflag = 4;
                } else {
                    int lr = 0, ur = 0;
                    if (qres(row[x] - a, qs) == 0) {
                        lr = 1;
                        while (x + lr < xend &&
                               qres(row[x + lr] - a, qs) == 0) lr++;
                    }
                    if (top && qres(row[x] - b, qs) == 0) {
                        ur = 1;
                        while (x + ur < xend &&
                               qres(row[x + ur] - top[x + ur], qs) == 0) ur++;
                    }
                    if (lr >= ur && lr) { mflag = 2; run = lr; }
                    else if (ur >= 5)   { mflag = 3; run = ur; }
                }
                recs[nr++] = ((uint32_t)(FLAG_MDL_IDX + fb) << 16) | (uint32_t)mflag;
                rcost += rec_cost_q8(recs[nr - 1], cm);
                if (rcost >= lim) { *pnrec = nr; if (rcq8) *rcq8 = rcost; return; }
                if (mflag == 4) pmatch = 4;
                else if (mflag) {
                    if (mflag == 1)
                        while (x + run < xend &&
                               qres(row[x + run] - prow[TSX(x + run + dx)], qs) == 0)
                            run++;
                    recs[nr++] = ((uint32_t)(RUN_MDL_IDX + pmatch * 4 + run_bucket(S)) << 16) | (uint32_t)(run - 1);
                    rcost += rec_cost_q8(recs[nr - 1], cm);
                    if (rcost >= lim) { *pnrec = nr; if (rcq8) *rcq8 = rcost; return; }
                    update_run(S, run);
                    if (mflag == 5) {
                        pmatch = 4;
                        if (rrow) rrow[x] = (int16_t)pred;
                        erq = 0;
                        zleft = run - 1;
                        x++;
                        continue;
                    }
                    pmatch = mflag;
                    if (rrow) {
                        if (mflag == 1)
                            for (int i = 0; i < run; i++) rrow[x + i] = prow[TSX(x + i + dx)];
                        else if (mflag == 2)
                            for (int i = 0; i < run; i++) rrow[x + i] = (int16_t)a;
                        else
                            for (int i = 0; i < run; i++) rrow[x + i] = top[x + i];
                    }
                    erq = 0;
                                x += run;
                    continue;
                }
                else pmatch = 0;
            }

            int vh = pred + rq * qs;             /* decoded value */
            uint32_t u = u0;
            if (lmode == 2) {
                /* Z leaf: the decoder writes pred verbatim - eligible only
                   while every pixel stays inside the live quantizer bound */
                int e = row[x] - pred; if (e < 0) e = -e;
                if (e > (qs - 1) >> 1) {
                    if (zok) *zok = 0;
                    *pnrec = nr;
                    if (rcq8) *rcq8 = rcost;
                    return;
                }
                vh = pred;
            } else if (mflag != 4) {
            /* PRED/PREDNF: mode*4+gb (36 slots). OP_PREDNFX (lmode 4)
               and OP_PREDX (lmode 5) fold ctx into leftover NRES. */
            recs[nr++] = (pred_res_midx(mode, gb, ctx, fold_res) << 16) | u;
            rcost += rec_cost_q8(recs[nr - 1], cm);
            if (rcost >= lim) { *pnrec = nr; if (rcq8) *rcq8 = rcost; return; }
            }
            if (ZE(zpx)) fprintf(stderr, "EPX @%d,%d px=(%d,%d) a=%d b=%d c=%d d2=%d ctx=%d mode=%d u=%d\n",
                    tx, ty, x, y, a, b, c, d2, ctx, mode, u);
            if (rrow) rrow[x] = (int16_t)vh;

            int abs_s[NSPAT];
#if defined(__ARM_NEON)
            int32x4_t vv = vdupq_n_s32(vh);
            vst1q_s32(abs_s,     vabdq_s32(vv, vld1q_s32(spreds)));
            vst1q_s32(abs_s + 4, vabdq_s32(vv, vld1q_s32(spreds + 4)));
#else
            for (int p = 0; p < NSPAT; p++) {
                int d = vh - spreds[p]; abs_s[p] = d < 0 ? -d : d;
            }
#endif
            int dt = has_t ? vh - prow[TSX(x + dx)] : 0;
            int abs_t = has_t ? (dt < 0 ? -dt : dt) : abs_s[0];
            update_ctx_u(S, U, pu2s, ctx, abs_s, abs_t);
            erq = (lmode == 2) ? 0 : rq * qs;
            x++;
        }
    }
    *pnrec = nr;
    if (rcq8) *rcq8 = rcost;
#undef TSX
}

static uint64_t smix64(uint64_t z)
{
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* content-keyed tile dictionary - encoder-side search aid only.
   Maps the hash of a 16x16 window to a few positions where that exact
   content appears.  Hits re-enter the normal candidate eval, which
   re-verifies pixels before committing, so a hash collision can only
   waste an eval, never corrupt the stream. */
#define TD_BITS 15
#define TD_SZ   (1 << TD_BITS)
#define TD_K    4
#define TD_STEP 8
typedef struct {
    uint32_t h; int8_t n, pad;
    int16_t px[TD_K], py[TD_K];
} tdent_t;
typedef struct { tdent_t *t; } tdict_t;

static uint32_t td_hash(const int16_t *p, int stride)
{
    uint64_t h = 0x243F6A8885A308D3ULL;
    for (int i = 0; i < TILE_SZ * TILE_SZ; i += 4) {
        const int16_t *r = p + (size_t)(i >> 4) * stride + (i & 15);
        uint64_t v = (uint64_t)(uint16_t)r[0]
                   | ((uint64_t)(uint16_t)r[1] << 16)
                   | ((uint64_t)(uint16_t)r[2] << 32)
                   | ((uint64_t)(uint16_t)r[3] << 48);
        h = (h ^ v) * 0x9E3779B97F4A7C15ULL;
    }
    return (uint32_t)(h ^ (h >> 32));
}

static void td_put(tdict_t *d, uint32_t h, int x, int y)
{
    size_t i = h & (TD_SZ - 1);
    for (int p = 0; p < 16; p++) {
        tdent_t *e = &d->t[(i + (size_t)p) & (TD_SZ - 1)];
        if (e->n && e->h != h) continue;
        if (!e->n) e->h = h;
        if (e->n < TD_K) {
            e->px[e->n] = (int16_t)x; e->py[e->n] = (int16_t)y; e->n++;
        }
        return;
    }
}

static const tdent_t *td_get(const tdict_t *d, uint32_t h)
{
    size_t i = h & (TD_SZ - 1);
    for (int p = 0; p < 16; p++) {
        const tdent_t *e = &d->t[(i + (size_t)p) & (TD_SZ - 1)];
        if (!e->n) return NULL;
        if (e->h == h) return e;
    }
    return NULL;
}

/* hash every TD_STEP-grid 16x16 window whose top-left lies inside the
   [bx,bx+bw) x [by,by+bh) box of base; stored positions are shifted by
   (ox,oy) so each caller keys positions in its own coordinate frame. */
static void td_build(tdict_t *d, const int16_t *base, int stride,
                     int bx, int by, int bw, int bh, int ox, int oy)
{
    memset(d->t, 0, TD_SZ * sizeof(tdent_t));
    for (int y = by; y + TILE_SZ <= by + bh; y += TD_STEP)
        for (int x = bx; x + TILE_SZ <= bx + bw; x += TD_STEP)
            td_put(d, td_hash(base + (int64_t)y * stride + x, stride),
                   x + ox, y + oy);
}

typedef struct {
    const int16_t *prev, *curr, *luma, *W;
    int w, h, ntx, has_prev;
    pstate_t S;
    uint32_t *precs, *rrecs, *tile_tmp;
    size_t npr, nrr;
    mdl64_t *pmc, *cm;
    int *mv_ax, *mv_ay;
    uint8_t *upstat;                /* per-column: tile above emitted SPLIT */
    int prev_dx, prev_dy, prev_copy;
    int last_tmv[2], last_imv[2];   /* MV predictors shared with decoder */
    int gmv[2], gmv_valid;          /* dominant frame motion (enc probe) */
    int last_op;                    /* previous leaf op: ctx for op model */
    int qs;                         /* current stream quantizer (1=lossless) */
    int qs_hi;                      /* coarsest allowed per-tile step */
    uint32_t nseed;                 /* nonzero: OP_NOISE synthesis allowed */
    int16_t *recon;                 /* decoder-view plane (NULL iff qs==1) */
    vop_t *vops;                    /* vertical-repeat table (emit state) */
    vop_t pre_slot;                 /* this slot's prev-frame descriptor,
                                       snapshotted before publish */
    vop_t *ptab;                    /* prev frame's per-tile leaf map
                                       (NULL in stateless mode) */
    int cur_ti;                     /* top-level tile index being emitted */
    int nn_cnt;                     /* OP_NN leaves emitted this plane */
    int fg_hits;                    /* fast-gate wins (diagnostic) */
    int all_run;
    int yoff;                       /* this plane slice's row offset in
                                       the full plane (stripe encodes) */
    int hfull;                      /* full-plane height - the bound that
                                       prev-frame references live under */
    tdict_t pd_, cd_;
    const tdict_t *pdict, *cdict;
    pundo_t pu;                     /* candidate-eval undo scratch */
    pundo_t pu2;                    /* lite-txn pstate rollback log */
    /* per-depth scratch stacks: enc_one_tile keeps its working buffers
       here instead of in ~60KB stack frames.  Indexed by recursion
       depth so a suspended parent's winner state survives the
       children's own evals. */
    uint16_t (*bdc_stk)[N_CTX];
    int32_t  (*bdr_stk)[NSPAT + 2];
    pxcache_t *pxc_stk;
    uint32_t *recbuf_stk;
    pstate_t *sc_stk, *bS_stk;
    /* probe capture: notxn emit=0 probe children export their winner
       bundle here; if the split wins, the emit=1 commit replays the
       bundle instead of re-running the whole candidate ladder.
       (probe children are always leaves - emit=0 gates their own
       probes - so the bundle is complete.) */
    struct cap_t *cap_stk;
    int capn_commit, capn_rep, capn_chain, capn_trans;
    int pbias[4];                 /* adaptive probe gate per depth */
    int pcap;                     /* probe-child budget: early-out when bcost exceeds */
    int minleaf;                  /* cheapest op-only record at this tile's oc */
    int qd_spread;          /* [ENC_MAXD][4] */
    struct cap_t *cap_out;          /* capture target for emit=0 child */
    struct cap_t *cap_rep;          /* replay bundle for emit=1 child */
    uint32_t pal_seen[512];         /* epoch-tagged presence table */
    int pal_epoch;
    int16_t palq[PALQ_N];       /* newest color at slot 0 */
    int palq_n;
} ectx_t;

typedef struct cap_t {
    int valid, op, dx, dy, pk, bq, sig, pred, cost;
    int bnd, ar, nr;
    int th[PAL_MAX];
    size_t nrec;
    uint32_t recs[TILE_SZ * TILE_SZ * 2 + 16];
    uint8_t idx[TILE_SZ * TILE_SZ];
    int16_t pal[PAL_MAX];
    int16_t rc[TILE_SZ * TILE_SZ];
    union {
        pstate_t bs;
        struct {
            uint16_t dc[N_CTX];
            int32_t dr[N_CTX][NSPAT + 2];
        } dl;
    } u;
} cap_t;
#define ENC_MAXD 12   /* split depth hard bound for the stacks above */

/* OP_CFL leaf: pred = (ca*luma)/64 + cb fitted per leaf.  Same causal
   loop/record shape as sim_pred_tile (run flags then residual under
   CFL_MDL_IDX) so pstate evolution mirrors the decoder exactly. */
static void sim_cfl_tile(ectx_t *E, int tx, int ty, int tw, int th,
                         int ca, int cb, pstate_t *S,
                         uint32_t *recs, size_t *pnrec, int qs,
                         pundo_t *U)
{
    pundo_t *pu2s = (zpu2 && S == zpu2_s) ? zpu2 : NULL;
    const int16_t *prev = E->prev, *curr = E->curr, *luma = E->luma;
    const int16_t *ctx_src = E->recon ? E->recon : curr;
    int w = E->w;
    size_t nr = *pnrec;
    for (int y = ty; y < ty + th; y++) {
        const int16_t *row = curr + (size_t)y * w;
        int16_t *rrow = E->recon ? E->recon + (size_t)y * w : NULL;
        const int16_t *top = y > 0 ? ctx_src + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? ctx_src + (size_t)(y - 2) * w : NULL;
        const int16_t *prow = E->has_prev ? prev + (size_t)y * w : NULL;
        int x = tx, xend = tx + tw;
        int pmatch = 0;
        while (x < xend) {
            int a = x > 0 ? (rrow ? rrow[x - 1] : row[x - 1])
                          : (top ? top[x] : 0);
            int aa = x > 1 ? (rrow ? rrow[x - 2] : row[x - 2]) : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d2;
            if (!top)         d2 = b;
            else if (y == ty) {
                int rl = (ty % TILE_SZ == 0) ? w
                       : ((tx & ~(2 * tw - 1)) + 2 * tw);
                if (rl > w) rl = w;
                d2 = (x + 1 < rl) ? top[x + 1] : b;
            } else            d2 = (x + 1 < xend) ? top[x + 1] : b;
            int q1 = q_grad(d2 - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);
            /* floor(At/N) <= T  <=>  At < (T+1)*N  for At,N >= 0 -
               the three bucket compares replace a per-pixel divide */
            int32_t atv = S->At[ctx];
            int64_t nn = S->N[ctx] > 0 ? S->N[ctx] : 1;
            int gb_a = atv < (int64_t)(qs + 1) * nn ? 0 :
                       atv < (int64_t)(qs * 4 + 1) * nn ? 1 :
                       atv < (int64_t)(qs * 16 + 1) * nn ? 2 : 3;
            int gb = gb_a + grad_bucket(a, b, c, d2, qs);
            if (gb > 3) gb = 3;
            int fb = pmatch * 4 + grad_bucket(a, b, c, d2, qs);

            if (E->has_prev) {
                int can_run = (qres(row[x] - prow[x], qs) == 0);
                recs[nr++] = ((uint32_t)(FLAG_MDL_IDX + fb) << 16) | (uint32_t)can_run;
                if (can_run) {
                    int run = 1;
                    while (x + run < xend &&
                           qres(row[x + run] - prow[x + run], qs) == 0)
                        run++;
                    recs[nr++] = ((uint32_t)(RUN_MDL_IDX + pmatch * 4 + run_bucket(S)) << 16)
                               | (uint32_t)(run - 1);
                    update_run(S, run);
                    pmatch = 1;
                    if (rrow)
                        for (int i = 0; i < run; i++)
                            rrow[x + i] = prow[x + i];
                    x += run;
                    continue;
                }
                pmatch = 0;
            }

            int spreds[NSPAT];
            spat_preds(a, b, c, d2, aa, uu, spreds);

            int lu = luma ? luma[(size_t)y * w + x] : 0;
            int pred = ((ca * lu) >> 6) + cb;
            int rq = qres(row[x] - pred, qs);
            int vh = pred + rq * qs;
            int r = flip ? -rq : rq;
            uint32_t u = zigzag_enc(r);
            if (u > 0xFFFFu) u = 0xFFFFu;
            recs[nr++] = ((uint32_t)CFL_MDL_IDX << 16) | u;
            if (rrow) rrow[x] = (int16_t)vh;

            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) {
                int d = vh - spreds[p]; abs_s[p] = d < 0 ? -d : d;
            }
            int dt = E->has_prev ? vh - prow[x] : 0;
            int abs_t = E->has_prev ? (dt < 0 ? -dt : dt) : abs_s[0];
            update_ctx_u(S, U, pu2s, ctx, abs_s, abs_t);
            x++;
        }
    }
    *pnrec = nr;
}


static void tile_cp(int16_t *dst, const int16_t *src, int w,
                    int tx, int ty, int tw, int th)
{
    for (int y = ty; y < ty + th; y++)
        memcpy(dst + (size_t)(y - ty) * tw,
               src + (size_t)y * w + tx, (size_t)tw * sizeof(int16_t));
}
static void tile_cp_back(int16_t *dst, int w, const int16_t *src,
                         int tx, int ty, int tw, int th)
{
    for (int y = ty; y < ty + th; y++)
        memcpy(dst + (size_t)y * w + tx,
               src + (size_t)(y - ty) * tw, (size_t)tw * sizeof(int16_t));
}

/* Integer lifting Haar, in-place on n (power of two).  s = a+(d>>1),
   d = b-a is exactly reversible with the matching inverse, and the
   DC stays mean-scale instead of growing with n. */
static void haar1d_fwd(int *x, int n)
{
#if defined(__ARM_NEON)
    if (n == 8) {
        int32x4x2_t u = vld2q_s32(x);
        int32x4_t d = vsubq_s32(u.val[1], u.val[0]);
        int32x4_t s = vaddq_s32(u.val[0], vshrq_n_s32(d, 1));
        vst1q_s32(x, s);
        vst1q_s32(x + 4, d);
        return;
    }
    if (n == 16) {
        int32x4x2_t u0 = vld2q_s32(x);
        int32x4x2_t u1 = vld2q_s32(x + 8);
        int32x4_t d0 = vsubq_s32(u0.val[1], u0.val[0]);
        int32x4_t d1 = vsubq_s32(u1.val[1], u1.val[0]);
        int32x4_t s0 = vaddq_s32(u0.val[0], vshrq_n_s32(d0, 1));
        int32x4_t s1 = vaddq_s32(u1.val[0], vshrq_n_s32(d1, 1));
        vst1q_s32(x, s0);
        vst1q_s32(x + 4, s1);
        vst1q_s32(x + 8, d0);
        vst1q_s32(x + 12, d1);
        return;
    }
#endif
    int tmp[TILE_SZ];
    for (int i = 0; i < n / 2; i++) {
        int a = x[2 * i], b = x[2 * i + 1];
        int d = b - a;
        tmp[i] = a + (d >> 1);
        tmp[n / 2 + i] = d;
    }
    memcpy(x, tmp, (size_t)n * sizeof(int));
}
static void haar1d_inv(int *x, int n)
{
#if defined(__ARM_NEON)
    if (n == 8) {
        int32x4_t s = vld1q_s32(x);
        int32x4_t d = vld1q_s32(x + 4);
        int32x4_t a = vsubq_s32(s, vshrq_n_s32(d, 1));
        int32x4x2_t z;
        z.val[0] = a;
        z.val[1] = vaddq_s32(a, d);
        vst2q_s32(x, z);
        return;
    }
    if (n == 16) {
        int32x4_t s0 = vld1q_s32(x);
        int32x4_t s1 = vld1q_s32(x + 4);
        int32x4_t d0 = vld1q_s32(x + 8);
        int32x4_t d1 = vld1q_s32(x + 12);
        int32x4_t a0 = vsubq_s32(s0, vshrq_n_s32(d0, 1));
        int32x4_t a1 = vsubq_s32(s1, vshrq_n_s32(d1, 1));
        int32x4x2_t z0, z1;
        z0.val[0] = a0; z0.val[1] = vaddq_s32(a0, d0);
        z1.val[0] = a1; z1.val[1] = vaddq_s32(a1, d1);
        vst2q_s32(x, z0);
        vst2q_s32(x + 8, z1);
        return;
    }
#endif
    int tmp[TILE_SZ];
    for (int i = 0; i < n / 2; i++) {
        int s = x[i], d = x[n / 2 + i];
        int a = s - (d >> 1);
        tmp[2 * i] = a;
        tmp[2 * i + 1] = a + d;
    }
    memcpy(x, tmp, (size_t)n * sizeof(int));
}
static void haar2d_fwd(int *blk, int n)
{
    int col[TILE_SZ];
    for (int y = 0; y < n; y++) haar1d_fwd(blk + y * n, n);
    for (int x = 0; x < n; x++) {
        for (int y = 0; y < n; y++) col[y] = blk[y * n + x];
        haar1d_fwd(col, n);
        for (int y = 0; y < n; y++) blk[y * n + x] = col[y];
    }
    if (n >= 4) {
        int m = n / 2;
        int ll[TILE_SZ * TILE_SZ / 4];
        for (int y = 0; y < m; y++)
            for (int x = 0; x < m; x++)
                ll[y * m + x] = blk[y * n + x];
        haar2d_fwd(ll, m);
        for (int y = 0; y < m; y++)
            for (int x = 0; x < m; x++)
                blk[y * n + x] = ll[y * m + x];
    }
}
static void haar2d_inv(int *blk, int n)
{
    if (n >= 4) {
        int m = n / 2;
        int ll[TILE_SZ * TILE_SZ / 4];
        for (int y = 0; y < m; y++)
            for (int x = 0; x < m; x++)
                ll[y * m + x] = blk[y * n + x];
        haar2d_inv(ll, m);
        for (int y = 0; y < m; y++)
            for (int x = 0; x < m; x++)
                blk[y * n + x] = ll[y * m + x];
    }
    int col[TILE_SZ];
    for (int x = 0; x < n; x++) {
        for (int y = 0; y < n; y++) col[y] = blk[y * n + x];
        haar1d_inv(col, n);
        for (int y = 0; y < n; y++) blk[y * n + x] = col[y];
    }
    for (int y = 0; y < n; y++) haar1d_inv(blk + y * n, n);
}
static void tx_zigzag(uint8_t *sc, int n)
{
    int k = 0;
    for (int s = 0; s < 2 * n - 1; s++) {
        if (s & 1) {
            int x0 = s < n ? 0 : s - n + 1;
            int x1 = s < n ? s : n - 1;
            for (int x = x0; x <= x1; x++)
                sc[k++] = (uint8_t)((s - x) * n + x);
        } else {
            int y0 = s < n ? 0 : s - n + 1;
            int y1 = s < n ? s : n - 1;
            for (int y = y0; y <= y1; y++)
                sc[k++] = (uint8_t)(y * n + (s - y));
        }
    }
}
static int tx_pred_at(const int16_t *prev, int w, int h,
                      int x, int y, int dx, int dy,
                      int y0, int y1)
{
    (void)h;
    if (!prev) return 0;
    int sx = x + dx, sy = y + dy;
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < y0) sy = y0; else if (sy >= y1) sy = y1 - 1;
    return prev[(size_t)sy * w + sx];
}

static int tx_border_at(const int16_t *p, int w, int x, int y, int y0, int y1)
{
    if (!p) return 0;
    if (y < y0) y = y0; else if (y >= y1) y = y1 - 1;
    return p[(size_t)y * w + x];
}

/* spat_mode < 0: temporal (dx,dy).  0=zero, 1=DC of left+top border,
   2=horizontal (copy left column), 3=vertical (copy top row). */
static void tx_fill_pred(int *pred, int n,
                         const int16_t *prev, const int16_t *border,
                         int w, int h, int tx, int ty,
                         int dx, int dy, int spat_mode,
                         int y0, int y1)
{
    (void)h;
    if (spat_mode < 0) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++)
                pred[y * n + x] = tx_pred_at(prev, w, h, tx + x, ty + y,
                                             dx, dy, y0, y1);
        return;
    }
    if (spat_mode == 0) {
        memset(pred, 0, (size_t)n * n * sizeof(int));
        return;
    }
    int has_l = tx > 0 && border;
    int has_t = ty > 0 && border;
    if (spat_mode == 1) {
        int s = 0, cnt = 0;
        if (has_t)
            for (int x = 0; x < n; x++) {
                s += tx_border_at(border, w, tx + x, ty - 1, y0, y1);
                cnt++;
            }
        if (has_l)
            for (int y = 0; y < n; y++) {
                s += tx_border_at(border, w, tx - 1, ty + y, y0, y1);
                cnt++;
            }
        int dc = cnt ? (s + (cnt >> 1)) / cnt : 0;
        for (int i = 0; i < n * n; i++) pred[i] = dc;
        return;
    }
    if (spat_mode == 2) {
        for (int y = 0; y < n; y++) {
            int v = has_l ? tx_border_at(border, w, tx - 1, ty + y, y0, y1)
                  : has_t ? tx_border_at(border, w, tx, ty - 1, y0, y1)
                          : 0;
            for (int x = 0; x < n; x++) pred[y * n + x] = v;
        }
        return;
    }
    for (int x = 0; x < n; x++) {
        int v = has_t ? tx_border_at(border, w, tx + x, ty - 1, y0, y1)
              : has_l ? tx_border_at(border, w, tx - 1, ty, y0, y1)
                      : 0;
        for (int y = 0; y < n; y++) pred[y * n + x] = v;
    }
}

/* Haar residual. spat_mode < 0 = temporal; 0..3 = intra modes.
   Records: [mode if intra], nnz-1, then (pos_delta, zigzag(level)). */
static int sim_tx_tile(ectx_t *E, int tx, int ty, int tw, int th,
                       int dx, int dy, int qs,
                       uint32_t *recs, size_t *pnrec, int spat_mode)
{
    int n = tw;
    int np = n * n;
    int blk[TILE_SZ * TILE_SZ];
    int pred[TILE_SZ * TILE_SZ];
    uint8_t sc[TILE_SZ * TILE_SZ];
    int y0 = -E->yoff, y1 = E->hfull - E->yoff;
    const int16_t *prev = (spat_mode < 0 && E->has_prev) ? E->prev : NULL;
    const int16_t *border = (spat_mode >= 0)
                          ? (E->recon ? E->recon : E->curr) : NULL;
    tx_fill_pred(pred, n, prev, border, E->w, E->h, tx, ty,
                 dx, dy, spat_mode, y0, y1);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            blk[y * n + x] = E->curr[(size_t)(ty + y) * E->w + tx + x]
                           - pred[y * n + x];
    haar2d_fwd(blk, n);
    tx_zigzag(sc, n);
    int stored[TILE_SZ * TILE_SZ];
    for (int i = 0; i < np; i++) {
        int q = qres(blk[sc[i]], qs);
        stored[i] = q;
        blk[sc[i]] = q * qs;
    }
    int last = 0;
    for (int i = np - 1; i > 0; i--)
        if (stored[i]) { last = i; break; }
    int nsend = last + 1;
    size_t nr = 0;
    recs[nr++] = ((uint32_t)CFL_MDL_IDX << 16) | (uint32_t)(nsend - 1);
    for (int i = 0; i < nsend; i++) {
        uint32_t u = zigzag_enc(stored[i]);
        if (u > 0xFFFFu) u = 0xFFFFu;
        recs[nr++] = ((uint32_t)CFL_MDL_IDX << 16) | u;
    }
    haar2d_inv(blk, n);
    int lim = qs > 1 ? (E->qs_hi - 1) / 2 : 0;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int v = pred[y * n + x] + blk[y * n + x];
            if (qs > 1) {
                int d = v - E->curr[(size_t)(ty + y) * E->w + tx + x];
                if (d < 0) d = -d;
                if (d > lim) return 0;
            }
            if (E->recon)
                E->recon[(size_t)(ty + y) * E->w + tx + x] = (int16_t)v;
        }
    *pnrec = nr;
    return 1;
}

static int dec_tx_tile(const int16_t *prev, int16_t *curr,
                       int w, int h, int tx, int ty, int tw, int th,
                       int dx, int dy, int has_prev, zrd_t *d, int qs,
                       int yoff, int hfull, int intra)
{
    (void)h;
    if (tw != th || (tw != 4 && tw != 8 && tw != 16) || !d) return -1;
    int n = tw;
    int np = n * n;
    int blk[TILE_SZ * TILE_SZ];
    int pred[TILE_SZ * TILE_SZ];
    uint8_t sc[TILE_SZ * TILE_SZ];
    memset(blk, 0, (size_t)np * sizeof(int));
    tx_zigzag(sc, n);
    int spat_mode = intra ? 1 : -1;
    uint32_t ns = zrd_len_sym(d, CFL_MDL_IDX);
    if (d->err || ns >= (uint32_t)np) return -1;
    int nsend = (int)ns + 1;
    for (int i = 0; i < nsend; i++) {
        uint32_t u = zrd_len_sym(d, CFL_MDL_IDX);
        if (d->err) return -1;
        blk[sc[i]] = zigzag_dec(u) * qs;
    }
    haar2d_inv(blk, n);
    int y0 = -yoff, y1 = hfull - yoff;
    const int16_t *pref = (!intra && has_prev) ? prev : NULL;
    const int16_t *border = intra ? curr : NULL;
    tx_fill_pred(pred, n, pref, border, w, h, tx, ty,
                 dx, dy, spat_mode, y0, y1);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            curr[(size_t)(ty + y) * w + tx + x] =
                (int16_t)(pred[y * n + x] + blk[y * n + x]);
    return 0;
}

static int mv_full_hang(const ectx_t *E, int tx, int ty, int tw, int th,
                        int dx, int dy);

static int resim_op(ectx_t *E, int op, int dx, int dy,
                    int tx, int ty, int tw, int th,
                    const pstate_t *seed, pstate_t *St,
                    uint32_t *tmp, size_t *nt, int q)
{
    if (op == OP_CFL) {
        *St = *seed;
        *nt = 0;
        sim_cfl_tile(E, tx, ty, tw, th, dx, dy, St, tmp, nt, q, NULL);
        return 1;
    }
    if (op == OP_TX || op == OP_TXI) {
        if (tw != th || (tw != 4 && tw != 8 && tw != 16)) return 0;
        *St = *seed;
        *nt = 0;
        if (!sim_tx_tile(E, tx, ty, tw, th,
                         op == OP_TXI ? 0 : dx,
                         op == OP_TXI ? 0 : dy, q, tmp, nt,
                         op == OP_TXI ? 1 : -1))
            return 0;
        return 1;
    }
    const int16_t *ref = (op == OP_ICOPY || op == OP_ICOPYNF)
                       ? (E->recon ? E->recon : E->curr) : E->prev;
    int lm = (op == OP_PREDNF || op == OP_NNF ||
              op == OP_COPYNF || op == OP_ICOPYNF) ? 1
           : (op == OP_PREDZ || op == OP_NNZ) ? 2
           : op == OP_PREDIR ? 3
           : op == OP_PREDNFX ? 4
           : op == OP_PREDX ? 5 : 0;
    int hp = (op == OP_PRED || op == OP_NN || op == OP_PREDNF ||
              op == OP_NNF || op == OP_PREDZ || op == OP_NNZ ||
              op == OP_PREDIR || op == OP_PREDNFX ||
              op == OP_PREDX) ? E->has_prev : 1;
    /* A full-frame overhang COPY was priced with the edge clamp.
       Re-simulating it (OP_QS) must clamp the same way, or the
       residual stream will not match dec_pred_tile. */
    if (hp == 1 &&
        op != OP_ICOPY && op != OP_ICOPYNF &&
        op != OP_IRUN && op != OP_IRUNP &&
        mv_full_hang(E, tx, ty, tw, th, dx, dy))
        hp = 2;
    /* decoder passes Wp only to COPY/ICOPY/NN leaves - OP_PRED and the
       default case run the fixed bank.  The resim must route residual
       records through the same midx space the decoder will read. */
    const int16_t *Wr = (op == OP_PRED || op == OP_PREDNF ||
                         op == OP_PREDZ || op == OP_PREDIR ||
                         op == OP_PREDNFX || op == OP_PREDX) ? NULL : E->W;
    *St = *seed;
    *nt = 0;
    int zk = 1;
    sim_pred_tile(ref, E->curr, E->w, E->h, tx, ty, tw, th,
                  dx, dy, hp, St, tmp, nt, Wr, E->luma, E->recon, q,
                  E->cm, lm, &zk, NULL, INT_MAX, NULL, NULL);
    if (!zk) return 0;   /* Z leaf lost eligibility at this qs */
    return 1;
}

static void sim_noise_tile(ectx_t *E,
                           int tx, int ty, int tw, int th,
                           pstate_t *S, int sig, uint32_t seed)
{
    const int16_t *ctx_src = E->recon ? E->recon : E->curr;
    const int16_t *prev = E->prev, *curr = E->curr;
    int w = E->w;
    int erq = 0;
    uint64_t ctr = seed;
    for (int y = ty; y < ty + th; y++) {
        const int16_t *row = curr + (size_t)y * w;
        int16_t *rrow = E->recon ? E->recon + (size_t)y * w : NULL;
        const int16_t *top = y > 0 ? ctx_src + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? ctx_src + (size_t)(y - 2) * w : NULL;
        const int16_t *prow = E->has_prev ? prev + (size_t)y * w : NULL;
        for (int x = tx; x < tx + tw; x++) {
            int a = x > 0 ? (rrow ? rrow[x - 1] : row[x - 1])
                          : (top ? top[x] : 0);
            int aa = x > 1 ? (rrow ? rrow[x - 2] : row[x - 2]) : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d2;
            if (!top)         d2 = b;
            else if (y == ty) {
                int rl = (ty % TILE_SZ == 0) ? w
                       : ((tx & ~(2 * tw - 1)) + 2 * tw);
                if (rl > w) rl = w;
                d2 = (x + 1 < rl) ? top[x + 1] : b;
            } else            d2 = (x + 1 < tx + tw) ? top[x + 1] : b;

            int q1 = q_grad(d2 - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);

            int spreds[NSPAT];
            spat_preds(a, b, c, d2, aa, uu, spreds);
            int has_t = E->has_prev;
            int mode = 0;
            int32_t best_A = S->As[ctx][0];
            for (int p = 1; p < NSPAT; p++) {
                if (S->As[ctx][p] < best_A) { best_A = S->As[ctx][p]; mode = p; }
            }
            if (has_t && S->At[ctx] < best_A) {
                best_A = S->At[ctx]; mode = NSPAT;
            }

            uint64_t z = smix64(ctr++);
            int64_t n4 = (int64_t)(z & 0xFFFF) + (int64_t)((z >> 16) & 0xFFFF)
                       + (int64_t)((z >> 32) & 0xFFFF) + (int64_t)(z >> 48);
            int nz = (int)((n4 - 131070) * sig / 37838);

            int val;
            if (E->W) {
                int pl = 0, pr = 0, pu = 0, pd = 0;
                if (has_t) {
                    pl = prow[x ? x - 1 : 0];
                    pr = prow[x + 1 < w ? x + 1 : w - 1];
                    pu = prev[(size_t)(y ? y - 1 : 0) * w + x];
                    pd = prev[(size_t)(y < E->h - 1 ? y + 1 : E->h - 1) * w + x];
                }
                val = llp_predict(E->W, a, b, c, d2,
                                  has_t ? prow[x] : 0,
                                  E->luma ? E->luma[(size_t)y * w + x] : 0,
                                  aa, uu, pl, pr, pu, pd, erq) + nz;
                erq = nz;
            }
            else if (mode == NSPAT) val = prow[x] + nz;
            else               val = spreds[mode] + nz;
            if (rrow) rrow[x] = (int16_t)val;

            int rs[NSPAT];
            for (int p = 0; p < NSPAT; p++) rs[p] = val - spreds[p];
            int rt = has_t ? (val - prow[x]) : 0;
            if (flip) {
                for (int p = 0; p < NSPAT; p++) rs[p] = -rs[p];
                rt = -rt;
            }
            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) abs_s[p] = rs[p] < 0 ? -rs[p] : rs[p];
            int abs_t = has_t ? (rt < 0 ? -rt : rt) : abs_s[0];
            update_ctx(S, ctx, abs_s, abs_t);
        }
    }
}

/* Evaluate every op for region (tx,ty,tw,th) and commit the cheapest.
   emit=0 is estimate-only: same candidate sims on cloned predictor state,
   no stream writes or model updates, returns the q8 bit cost. OP_SPLIT
   recurses one level into quarter-size children. */
/* row-parity SAD: sums rows where (y & 1) == parity.  even+odd == full
   SAD, so a caller can screen on the even half (a lower bound) and pay
   the odd half only for survivors. */
static int tile_sad_par(const int16_t *a, const int16_t *b, int w,
                        int tw, int th, int parity, int cap)
{
    int s = 0;
#if defined(__ARM_NEON)
    for (int y = parity; y < th; y += 2) {
        const int16_t *pa = a + (size_t)y * w;
        const int16_t *pb = b + (size_t)y * w;
        int32x4_t acc = vdupq_n_s32(0);
        int x = 0;
        for (; x + 8 <= tw; x += 8) {
            int16x8_t va = vld1q_s16(pa + x);
            int16x8_t vb = vld1q_s16(pb + x);
            acc = vabal_s16(acc, vget_low_s16(va), vget_low_s16(vb));
            acc = vabal_s16(acc, vget_high_s16(va), vget_high_s16(vb));
        }
        s += vaddvq_s32(acc);
        for (; x < tw; x++) {
            int d = pa[x] - pb[x];
            s += d < 0 ? -d : d;
        }
        if (s > cap) return s;
    }
    return s;
#else
    for (int y = parity; y < th; y += 2) {
        for (int x = 0; x < tw; x++) {
            int d = a[(size_t)y * w + x] - b[(size_t)y * w + x];
            s += d < 0 ? -d : d;
        }
        if (s > cap) return s;
    }
    return s;
#endif
}

static int tile_sad(const int16_t *a, const int16_t *b, int w,
                    int tw, int th, int cap)
{
    /* cap: bail with the partial sum once it exceeds cap - callers that
       keep a running top-K bound only need "not better than cap" from
       losers.  0x7FFFFFFF = always scan the whole tile. */
    int s = 0;
#if defined(__ARM_NEON)
    for (int y = 0; y < th; y++) {
        const int16_t *pa = a + (size_t)y * w;
        const int16_t *pb = b + (size_t)y * w;
        int32x4_t acc = vdupq_n_s32(0);
        int x = 0;
        for (; x + 8 <= tw; x += 8) {
            int16x8_t va = vld1q_s16(pa + x);
            int16x8_t vb = vld1q_s16(pb + x);
            acc = vabal_s16(acc, vget_low_s16(va), vget_low_s16(vb));
            acc = vabal_s16(acc, vget_high_s16(va), vget_high_s16(vb));
        }
        s += vaddvq_s32(acc);
        for (; x < tw; x++) {
            int d = pa[x] - pb[x];
            s += d < 0 ? -d : d;
        }
        if (s > cap) return s;
    }
    return s;
#else
    for (int y = 0; y < th; y++) {
        for (int x = 0; x < tw; x++) {
            int d = a[(size_t)y * w + x] - b[(size_t)y * w + x];
            s += d < 0 ? -d : d;
        }
        if (s > cap) return s;
    }
    return s;
#endif
}

/* max |a-b| over the tile - the acceptance metric for bounded
   exact-copy ops (RUN/MVRUN/IRUN): recon equals the source, so any
   src within the leaf's own error bound is free */
static int tile_maxd(const int16_t *a, const int16_t *b, int w,
                     int tw, int th)
{
    int m = 0;
#if defined(__ARM_NEON)
    int16x8_t mx = vdupq_n_s16(0);
    for (int y = 0; y < th; y++) {
        const int16_t *pa = a + (size_t)y * w;
        const int16_t *pb = b + (size_t)y * w;
        int x = 0;
        for (; x + 8 <= tw; x += 8) {
            int16x8_t va = vld1q_s16(pa + x);
            int16x8_t vb = vld1q_s16(pb + x);
            mx = vmaxq_s16(mx, vabdq_s16(va, vb));
        }
        for (; x < tw; x++) {
            int d = pa[x] - pb[x];
            if (d < 0) d = -d;
            if (d > m) m = d;
        }
    }
    m = vmaxvq_s16(mx) > m ? vmaxvq_s16(mx) : m;
    return m;
#else
    for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++) {
            int d = a[(size_t)y * w + x] - b[(size_t)y * w + x];
            if (d < 0) d = -d;
            if (d > m) m = d;
        }
    return m;
#endif
}

/* Reference rect fully inside the window an unclamped prev pointer
   may index. A stripe's local sy may be negative down to -yoff. */
static int mv_contained(const ectx_t *E, int tx, int ty, int tw, int th,
                        int dx, int dy)
{
    return tx + dx >= 0 && tx + tw - 1 + dx < E->w &&
           ty + dy >= -E->yoff &&
           ty + th - 1 + dy < E->hfull - E->yoff;
}

/* Full frame only: the rect overlaps the plane but is not contained.
   Callers sample it with prev_at. Stripe planes never take this path. */
static int mv_full_hang(const ectx_t *E, int tx, int ty, int tw, int th,
                        int dx, int dy)
{
    if (E->yoff != 0 || E->h != E->hfull) return 0;
    if (tx + dx >= E->w || tx + tw - 1 + dx < 0) return 0;
    if (ty + dy >= E->h || ty + th - 1 + dy < 0) return 0;
    return !mv_contained(E, tx, ty, tw, th, dx, dy);
}

static int tile_sad_clamp(const int16_t *curr, const int16_t *prev,
                          int w, int h, int tx, int ty, int tw, int th,
                          int dx, int dy, int cap)
{
    int s = 0;
    for (int y = 0; y < th; y++) {
        const int16_t *pa = curr + (size_t)(ty + y) * w + tx;
        int sy = ty + y + dy;
        for (int x = 0; x < tw; x++) {
            int d = pa[x] - prev_at(prev, w, h, tx + x + dx, sy);
            if (d < 0) d = -d;
            s += d;
            if (s > cap) return s;
        }
    }
    return s;
}

static int tile_maxd_clamp(const int16_t *curr, const int16_t *prev,
                           int w, int h, int tx, int ty, int tw, int th,
                           int dx, int dy)
{
    int m = 0;
    for (int y = 0; y < th; y++) {
        const int16_t *pa = curr + (size_t)(ty + y) * w + tx;
        int sy = ty + y + dy;
        for (int x = 0; x < tw; x++) {
            int d = pa[x] - prev_at(prev, w, h, tx + x + dx, sy);
            if (d < 0) d = -d;
            if (d > m) m = d;
        }
    }
    return m;
}

#define N_MVC 80
static const int mvc[N_MVC][2] = { {1,0}, {-1,0}, {0,1}, {0,-1},
    {2,0}, {-2,0}, {0,2}, {0,-2}, {3,0}, {-3,0}, {0,3}, {0,-3},
    {4,0}, {-4,0}, {0,4}, {0,-4}, {8,0}, {-8,0}, {0,8}, {0,-8}, {16,0}, {-16,0}, {0,16}, {0,-16}, {32,0}, {-32,0}, {0,32}, {0,-32}, {64,0}, {-64,0}, {0,64}, {0,-64}, {128,0}, {-128,0}, {0,128}, {0,-128}, {4,-4}, {4,4}, {8,-8}, {8,8}, {16,-16}, {16,16}, {32,-32}, {32,32}, {64,-64}, {64,64}, {128,-128}, {128,128},
    /* camera/natural handshake lives off the axis-aligned lattice.
       A 16x16 SAD is cheap; these 32 vectors catch the (2,1)/(3,2)
       pans that used to fall through to residual PREDNF. */
    {1,1},{-1,1},{1,-1},{-1,-1},
    {2,1},{1,2},{-2,1},{-1,2},{2,-1},{1,-2},{-2,-1},{-1,-2},
    {2,2},{-2,2},{2,-2},{-2,-2},
    {3,1},{1,3},{-3,1},{-1,3},{3,-1},{1,-3},{-3,-1},{-1,-3},
    {3,2},{2,3},{-3,2},{-2,3},{3,-2},{2,-3},{-3,-2},{-2,-3} };

static int zopc[43];

/* Cold cost of one implicit-last run list over n samples. Matches the
   symbols EMIT_RUNS writes inside rowd_body. */
static int row_runs_cost(ectx_t *E, const uint8_t *row, int n)
{
    int nruns = 0, ri[TILE_SZ], rx[TILE_SZ];
    if (n < 1 || n > TILE_SZ) return 0x3fffffff;
    for (int x = 0; x < n; ) {
        int r = 1;
        while (x + r < n && row[x + r] == row[x]) r++;
        ri[nruns] = row[x];
        rx[nruns] = r;
        nruns++;
        x += r;
    }
    int c = mlensym_cost_q8(&E->pmc[PM_PALN], &E->pmc[PM_HI],
                            (uint32_t)(nruns - 1));
    for (int i = 0; i < nruns; i++) {
        c += mlensym_cost_q8(&E->pmc[PM_PALI], &E->pmc[PM_HI],
                             (uint32_t)ri[i]);
        if (i + 1 < nruns)
            c += mlensym_cost_q8(&E->pmc[PM_RRL], &E->pmc[PM_HI],
                                 (uint32_t)(rx[i] - 1));
    }
    return c;
}

/* Longest-paying shared prefix of every pattern. 0 means none. */
static int pick_stem_w(ectx_t *E, uint8_t pat[][TILE_SZ], int nr, int tw)
{
    int best_w = 0, best = 0x7fffffff;
    if (nr < 1 || tw < 2) return 0;
    for (int w = 1; w < tw; w++) {
        int share = 1;
        for (int p = 1; p < nr; p++)
            if (memcmp(pat[p], pat[0], (size_t)w)) { share = 0; break; }
        if (!share) continue;
        int sc = mlensym_cost_q8(&E->pmc[PM_PALN], &E->pmc[PM_HI],
                                 (uint32_t)(w - 1));
        sc += row_runs_cost(E, pat[0], w);
        for (int p = 0; p < nr; p++)
            sc += row_runs_cost(E, pat[p] + w, tw - w);
        if (sc < best) { best = sc; best_w = w; }
    }
    return best_w;
}

/* The v3sq lossless sibling sets this so its duel stays on first-seen
   palette order. Sorting that sibling changed which frames won the
   duel and grew the near-lossless file. Direct lossless leaves it 0. */
static int rowpal_nosort;

/* Price (do_emit=0) or write (do_emit=1) an OP_ROWD / OP_ROWQ / OP_ROWS
   body. The opcode itself is counted here but only the caller emits it,
   on the shared commit path. Frozen CDFs: f[] does not move inside a
   segment, so a scratch-warmed price would undercut the bits the stream
   pays. use_stem selects OP_ROWS and ignores the cache. */
static int rowd_body(ectx_t *E, int oc, int do_emit, int use_cache,
                     int use_stem, const uint8_t *imap, int tw, int th,
                     int k, const int16_t *pal, int budget)
{
    uint8_t pat[TILE_SZ][TILE_SZ];
    uint8_t rid[TILE_SZ];
    int nr = 0;
    if (k < 1 || k > PAL_MAX || tw < 1 || th < 1 ||
        tw > TILE_SZ || th > TILE_SZ)
        return budget;
    for (int y = 0; y < th; y++) {
        int fnd = -1;
        for (int p = 0; p < nr; p++)
            if (!memcmp(pat[p], imap + (size_t)y * (size_t)tw, (size_t)tw)) {
                fnd = p;
                break;
            }
        if (fnd < 0) {
            memcpy(pat[nr], imap + (size_t)y * (size_t)tw, (size_t)tw);
            fnd = nr++;
        }
        rid[y] = (uint8_t)fnd;
    }
    /* Lossless planes only (recon is NULL iff the plane opened at
       qs==1). A luma delta like 40,81,69 becomes 40,69,81 and stays
       under the class-4 literal cap. A lossy plane can still drop a
       tile to qs==1 via OP_QS; sorting those tiles moved the shared
       model and grew the near-lossless file, so recon!=NULL keeps
       first-seen order. The decoder reads whichever order was sent.
       spal outlives pal. */
    int16_t spal[PAL_MAX];
    if (!E->recon && !rowpal_nosort) {
        uint8_t order[PAL_MAX], rmap[PAL_MAX];
        for (int i = 0; i < k; i++) order[i] = (uint8_t)i;
        for (int a = 1; a < k; a++) {
            uint8_t oi = order[a];
            int16_t v = pal[oi];
            int b = a;
            while (b > 0 && pal[order[b - 1]] > v) {
                order[b] = order[b - 1];
                b--;
            }
            order[b] = oi;
        }
        for (int j = 0; j < k; j++) {
            spal[j] = pal[order[j]];
            rmap[order[j]] = (uint8_t)j;
        }
        for (int p = 0; p < nr; p++)
            for (int x = 0; x < tw; x++)
                pat[p][x] = rmap[pat[p][x]];
        pal = spal;
    }
    int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                            use_stem ? OP_ROWS :
                            (use_cache ? OP_ROWQ : OP_ROWD));
    if (!do_emit && c >= budget) return c;
#define RDADD(M, V) do { \
        uint32_t _v = (uint32_t)(V); \
        c += mlensym_cost_q8(&E->pmc[(M)], &E->pmc[PM_HI], _v); \
        if (do_emit) { \
            E->precs[E->npr++] = ((uint32_t)(M) << 16) | _v; \
            mdl_bump(&E->pmc[(M)], &E->pmc[PM_HI], (uint32_t)(M), _v); \
        } else if (c >= budget) return c; \
    } while (0)
    RDADD(PM_PALN, k - 1);
    /* Luma palette values are a delta chain (pred starts at 0). YCoCg
       luma lives in about 0..128, so a neighbor delta zigzags to <318
       and never takes the 16-bit escape; a delta of 30 or less stays
       under the class-4 literal cap and drops the 8 raw bits that
       value+128 always pays. Chroma keeps value+128 so its PM_PALV
       escape, shared with PALT, is not pulled off symbol 62. */
    int pred = 0;
    for (int i = 0; i < k; i++) {
        if (!use_cache) {
            RDADD(PM_PALV, E->luma ? pal[i] + LIT_BIAS
                                   : (int)zigzag_enc(pal[i] - pred));
            pred = pal[i];
        } else {
            int slot = -1;
            for (int s = 0; s < E->palq_n; s++)
                if (E->palq[s] == pal[i]) { slot = s; break; }
            if (slot >= 0)
                RDADD(PM_PALQ, slot);
            else {
                RDADD(PM_PALQ, PALQ_MISS);
                RDADD(PM_PALV, E->luma ? pal[i] + LIT_BIAS
                                       : (int)zigzag_enc(pal[i] - pred));
            }
            pred = pal[i];
        }
    }
    RDADD(PM_PALN, nr - 1);
    /* The run that finishes a span has no length symbol: the decoder
       fills whatever is left. OP_ROWS first sends the shared stem, then
       each pattern's tail. A width that is not a prefix of every pattern
       is skipped, and the opcode is kept only when the caller sees a
       lower price than literal OP_ROWD. */
#define EMIT_RUNS(ROW, N) do { \
        const uint8_t *_row = (ROW); \
        int _n = (N); \
        int nruns = 0, ri[TILE_SZ], rx[TILE_SZ]; \
        for (int x = 0; x < _n; ) { \
            int r = 1; \
            while (x + r < _n && _row[x + r] == _row[x]) r++; \
            ri[nruns] = _row[x]; \
            rx[nruns] = r; \
            nruns++; \
            x += r; \
        } \
        RDADD(PM_PALN, nruns - 1); \
        for (int i = 0; i < nruns; i++) { \
            RDADD(PM_PALI, ri[i]); \
            if (i + 1 < nruns) \
                RDADD(PM_RRL, rx[i] - 1); \
        } \
    } while (0)
    if (use_stem) {
        int stem_w = pick_stem_w(E, pat, nr, tw);
        if (stem_w < 1) return budget;
        RDADD(PM_PALN, stem_w - 1);
        EMIT_RUNS(pat[0], stem_w);
        for (int p = 0; p < nr; p++)
            EMIT_RUNS(pat[p] + stem_w, tw - stem_w);
    } else {
        for (int p = 0; p < nr; p++)
            EMIT_RUNS(pat[p], tw);
    }
#undef EMIT_RUNS
    /* Vertical runs of the row id. A copy-above flag was one PM_PALF
       per row; these lengths sit on the same peaks as the horizontal
       runs, so one PM_RRL replaces a whole band. Run 0 is pattern 0
       and its index is omitted. The run that finishes the tile has
       no length. */
    {
        int nruns = 0, ri[TILE_SZ], rl[TILE_SZ];
        for (int y = 0; y < th; ) {
            int r = 1;
            while (y + r < th && rid[y + r] == rid[y]) r++;
            ri[nruns] = rid[y];
            rl[nruns] = r;
            nruns++;
            y += r;
        }
        RDADD(PM_PALN, nruns - 1);
        for (int i = 0; i < nruns; i++) {
            if (i > 0)
                RDADD(PM_PALI, ri[i]);
            if (i + 1 < nruns)
                RDADD(PM_RRL, rl[i] - 1);
        }
    }
#undef RDADD
    return c;
}

/* does stored VRPT cell e replay leaf op+params (op, dx, dy, npal,
   pal, sig) exactly?  Predicted-MV ops compare the effective MV the
   encoder settled on - replay resolves through the same chain. */
static int vop_match(const vop_t *e, int op, int dx, int dy,
                     int npal, const int16_t *pal,
                     const uint8_t *imap, int npix, int sig)
{
    if (e->op != op) return 0;
    switch (op) {
    case OP_COPY: case OP_MVRUN: case OP_ICOPY: case OP_IRUN:
    case OP_COPYP: case OP_MVRUNP: case OP_IRUNP:
    case OP_COPYNF: case OP_ICOPYNF:
    case OP_CFL: case OP_TX:
        return e->dx == dx && e->dy == dy;
    case OP_TXI:
        return 1;
    case OP_PAL: case OP_PALR: case OP_PALC: case OP_PALD:
        return e->npal == npal &&
               !memcmp(e->pal, pal, (size_t)npal * sizeof(int16_t));
    case OP_PALT: case OP_PALQ: case OP_PALZ: case OP_PALU:
        return e->npal == npal &&
               !memcmp(e->pal, pal, (size_t)npal * sizeof(int16_t)) &&
               !memcmp(e->imap, imap, (size_t)npix);
    case OP_NOISE:
        return e->sig == sig;
    case OP_ROWD: case OP_ROWQ: case OP_ROWS:
    case OP_SPAN:
        /* The pixels live entirely in this leaf's program records.
           A params replay would desync, so never report a match. */
        return 0;
    default:
        return 1;
    }
}

/* program records OP_VRPT saves: the opcode itself plus the leaf's
   parameter records (MV pair, palette header/entries, noise sigma).
   Per-pixel data - palette indexes, residuals, RAW - still streams. */
static int mvfam(int op)      /* 1 = temporal (last_tmv), 2 = intra */
{
    if (op == OP_COPY || op == OP_MVRUN ||
        op == OP_COPYP || op == OP_MVRUNP || op == OP_COPYNF ||
        op == OP_TX) return 1;
    if (op == OP_ICOPY || op == OP_IRUN || op == OP_IRUNP ||
        op == OP_ICOPYNF) return 2;
    return 0;
}

/* MV predictor for delta-coded streams: a top-level leaf whose
   previous-frame slot stored a same-family descriptor deltas against
   THAT mv (the program patch view); otherwise deltas against the leaf
   descriptor last published in this column+width lane (the spatial
   "above" reference - the decoder sees the same lane state because it
   only writes the cell after the leaf has fully decoded). */
static int mvpred(const ectx_t *E, int op, int c, int depth, int tx, int tw)
{
    int f = mvfam(op);
    if (!f || !E->vops) return 0;
    if (E->ptab && depth == 0) {
        const vop_t *pe = &E->pre_slot;
        if (mvfam(pe->op) == f) return c ? pe->dy : pe->dx;
    }
    const vop_t *vc = &E->vops[vcell_idx(E->ntx, tx, tw)];
    return mvfam(vc->op) == f ? (c ? vc->dy : vc->dx) : 0;
}

static int leaf_pcost(ectx_t *E, const pstate_t *S, int op, int dx, int dy, int npal,
                      const int16_t *pal, int sig, int depth,
                      int tx, int tw)
{
    int c = 0;
    if (op == OP_COPY || op == OP_MVRUN || op == OP_ICOPY ||
        op == OP_IRUN || op == OP_COPYNF || op == OP_ICOPYNF ||
        (op == OP_TX && E->has_prev))
        c = mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dx - mvpred(E, op, 0, depth, tx, tw)))
          + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dy - mvpred(E, op, 1, depth, tx, tw)));
    else if (op == OP_PAL || op == OP_PALR || op == OP_PALT ||
             op == OP_PALC || op == OP_PALD) {
        c = mlensym_cost_q8(&E->pmc[PM_PALN], &E->pmc[PM_HI], (uint32_t)(npal - 1));
        for (int i = 0; i < npal; i++)
            c += mlensym_cost_q8(&E->pmc[PM_PALV], &E->pmc[PM_HI],
                                 (uint32_t)(pal[i] + LIT_BIAS));
    } else if (op == OP_PALZ) {
        c = mlensym_cost_q8(&E->pmc[PM_PALN], &E->pmc[PM_HI], (uint32_t)(npal - 1));
        for (int i = 0; i < npal; i++)
            c += mlensym_cost_q8(&E->pmc[PM_PALZ], &E->pmc[PM_HI],
                                 zigzag_enc(pal[i]));
    } else if (op == OP_PALU) {
        c = mlensym_cost_q8(&E->pmc[PM_PALN], &E->pmc[PM_HI], (uint32_t)(npal - 1));
        for (int i = 0; i < npal; i++) {
            int slot = -1;
            for (int s = 0; s < E->palq_n; s++)
                if (E->palq[s] == pal[i]) { slot = s; break; }
            if (slot >= 0)
                c += mlensym_cost_q8(&E->pmc[PM_PALQ], &E->pmc[PM_HI],
                                     (uint32_t)slot);
            else
                c += mlensym_cost_q8(&E->pmc[PM_PALQ], &E->pmc[PM_HI],
                                     PALQ_MISS)
                   + mlensym_cost_q8(&E->pmc[PM_PALV], &E->pmc[PM_HI],
                                     (uint32_t)(pal[i] + LIT_BIAS));
        }
    } else if (op == OP_CFL)
        c = mlensym_cost_q8(&E->pmc[PM_CFLA], &E->pmc[PM_HI], zigzag_enc(dx))
          + mlensym_cost_q8(&E->pmc[PM_CFLB], &E->pmc[PM_HI], zigzag_enc(dy));
    else if (op == OP_NOISE)
        c = mlensym_cost_q8(&E->pmc[PM_QT], &E->pmc[PM_HI], (uint32_t)sig);
    return c;
}

/* save every side effect a subtree emit can produce; the recon rect is
   snapshotted only when a decoder view exists (lossy mode) */
static uint64_t txn_hash(const ectx_t *E, const pstate_t *Sio);
static uint64_t txn_hash_rc(const ectx_t *E, int tx, int ty, int tw, int th);
static _Atomic long txn_bad;

static void txn_begin(ectx_t *E, pstate_t *Sio, int tx, int ty,
                      int tw, int th, txn_t *t, int lite) {
    memset(t->bm, 0, sizeof t->bm);
    t->lite = lite;
    if (lite) {
        /* nested probe: no 16KB snapshot - every pstate write during
           the subtree is first-touch-logged into pu2 instead */
        pu_begin(&E->pu2, Sio);
        zpu2 = &E->pu2; zpu2_s = Sio;
    } else
        t->S = *Sio;
    t->npr = E->npr; t->nrr = E->nrr;
    t->mu0 = mdu_n; t->vu0 = vdu_n;
    t->qs = E->qs; t->last_op = E->last_op;
    t->all_run = E->all_run; t->nn_cnt = E->nn_cnt;
    t->prev_copy = E->prev_copy;
    t->prev_dx = E->prev_dx; t->prev_dy = E->prev_dy;
    t->last_tmv[0] = E->last_tmv[0]; t->last_tmv[1] = E->last_tmv[1];
    t->last_imv[0] = E->last_imv[0]; t->last_imv[1] = E->last_imv[1];
    t->palq_n = E->palq_n;
    memcpy(t->palq, E->palq, sizeof t->palq);
    int col = tx >> 4;
    t->up = E->upstat[col]; t->mx = E->mv_ax[col]; t->my = E->mv_ay[col];
    t->pre_slot = E->pre_slot;
    t->pvc0 = t->vc0 = E->vops ? E->vops[vcell_idx(E->ntx, tx, tw)]
                             : (vop_t){0};
    t->pptab_v = 0;
    ztxn_cm = E->cm; ztxn_pm = E->pmc;
    if ((t->has_rc = E->recon != NULL))
        for (int y = 0; y < th; y++)
            memcpy(t->rc + y * tw,
                   E->recon + (size_t)(ty + y) * E->w + tx,
                   (size_t)tw * sizeof(int16_t));
    if (ZE(ztxv)) {
        t->h0 = txn_hash(E, Sio);
        t->h0r = txn_hash_rc(E, tx, ty, tw, th);
        if (!dbg_pmc) {
            dbg_pmc = malloc(sizeof(mdl64_t) * NPROG_MDL);
            dbg_cm  = malloc(sizeof(mdl64_t) * N_MDL);
            dbg_vo  = malloc(sizeof(vop_t) * VOP_CELLS(E->ntx));
        }
        memcpy(dbg_pmc, E->pmc, sizeof(mdl64_t) * NPROG_MDL);
        memcpy(dbg_cm, E->cm, sizeof(mdl64_t) * N_MDL);
        memcpy(dbg_vo, E->vops, sizeof(vop_t) * VOP_CELLS(E->ntx));
    } else t->h0 = 0;
    ztxn_st[ztxn_n++] = t;
}

static void txn_rollback(ectx_t *E, pstate_t *Sio, txn_t *t,
                         int tx, int ty, int tw, int th) {
    int lvl = --ztxn_n;
    for (int i = mdu_n - 1; i >= t->mu0; i--)
        if (mdu_l[i].lvl == lvl) {
            mdl64_t *m = mdu_l[i].p;
            memcpy(m->h, mdu_l[i].h, sizeof m->h);
            m->tot = mdu_l[i].tot; m->hits = mdu_l[i].hits;
        }
    mdu_n = t->mu0;
    for (int i = vdu_n - 1; i >= t->vu0; i--)
        *vdu_l[i].p = vdu_l[i].v;
    vdu_n = t->vu0;
    E->npr = t->npr; E->nrr = t->nrr;
    if (t->lite) {
        pu_rollback(&E->pu2, Sio);
        zpu2 = NULL;
    } else
        *Sio = t->S;
    E->qs = t->qs; E->last_op = t->last_op;
    E->all_run = t->all_run; E->nn_cnt = t->nn_cnt;
    E->prev_copy = t->prev_copy;
    E->prev_dx = t->prev_dx; E->prev_dy = t->prev_dy;
    E->last_tmv[0] = t->last_tmv[0]; E->last_tmv[1] = t->last_tmv[1];
    E->last_imv[0] = t->last_imv[0]; E->last_imv[1] = t->last_imv[1];
    E->palq_n = t->palq_n;
    memcpy(E->palq, t->palq, sizeof E->palq);
    int col = tx >> 4;
    E->upstat[col] = t->up; E->mv_ax[col] = t->mx; E->mv_ay[col] = t->my;
    E->pre_slot = t->pre_slot;
    if (t->has_rc)
        for (int y = 0; y < th; y++)
            memcpy(E->recon + (size_t)(ty + y) * E->w + tx,
                   t->rc + y * tw, (size_t)tw * sizeof(int16_t));
    if (t->h0) {
        if (txn_hash_rc(E, tx, ty, tw, th) != t->h0r) {
            fprintf(stderr, "TXN-LEAK-RC @%d,%d %dx%d\n", tx, ty, tw, th);
            txn_bad++;
        }
        uint64_t h1 = txn_hash(E, Sio);
        if (h1 != t->h0) {
            /* bisect: rehash components one at a time */
            fprintf(stderr, "TXN-LEAK @%d,%d %dx%d ", tx, ty, tw, th);
            if (!t->lite && memcmp(Sio, &t->S, sizeof *Sio)) fprintf(stderr, "[Sio]");
            if (t->lite) fprintf(stderr, "[lite]");
            if (E->npr != t->npr || E->nrr != t->nrr) fprintf(stderr, "[npr/nrr]");
            if (E->qs != t->qs || E->last_op != t->last_op) fprintf(stderr, "[qs/lo]");
            if (E->all_run != t->all_run || E->nn_cnt != t->nn_cnt) fprintf(stderr, "[run/nn]");
            if (E->prev_copy != t->prev_copy || E->prev_dx != t->prev_dx ||
                E->prev_dy != t->prev_dy) fprintf(stderr, "[prev]");
            if (E->last_tmv[0] != t->last_tmv[0] || E->last_tmv[1] != t->last_tmv[1] ||
                E->last_imv[0] != t->last_imv[0] || E->last_imv[1] != t->last_imv[1]) fprintf(stderr, "[mv]");
            int col = tx >> 4;
            if (E->upstat[col] != t->up) fprintf(stderr, "[up]");
            if (E->mv_ax[col] != t->mx || E->mv_ay[col] != t->my) fprintf(stderr, "[mvax]");
            if (memcmp(&E->pre_slot, &t->pre_slot, sizeof E->pre_slot)) fprintf(stderr, "[slot]");
            for (int i = 0; i < NPROG_MDL; i++)
                if (memcmp(&E->pmc[i], &dbg_pmc[i], sizeof(mdl64_t))) {
                    fprintf(stderr, "[pmc%d]", i); break;
                }
            for (int i = 0; i < N_MDL; i++)
                if (memcmp(&E->cm[i], &dbg_cm[i], sizeof(mdl64_t))) {
                    fprintf(stderr, "[cm%d]", i); break;
                }
            for (int i = 0; i < VOP_CELLS(E->ntx); i++)
                if (memcmp(&E->vops[i], &dbg_vo[i], sizeof(vop_t))) {
                    fprintf(stderr, "[vo%d]", i); break;
                }
            if (t->has_rc && E->recon) {
                for (int y = 0; y < th; y++)
                    if (memcmp(E->recon + (size_t)(ty + y) * E->w + tx,
                               t->rc + y * tw, (size_t)tw * sizeof(int16_t))) {
                        fprintf(stderr, "[recon]"); break;
                    }
            }
            fprintf(stderr, "\n");
            txn_bad++;
        }
    }
}

/* debug: hash mutable state before/after rollback to find leaks */
static uint64_t txn_hash(const ectx_t *E, const pstate_t *Sio) {
    const uint64_t F = 0x100000001b3ull;
    uint64_t h = 1469598103934665603ull;
    const uint8_t *p;
    p = (const uint8_t *)Sio;
    for (size_t i = 0; i < sizeof *Sio; i++) { h ^= p[i]; h *= F; }
    p = (const uint8_t *)E->pmc;
    for (size_t i = 0; i < sizeof(mdl64_t) * NPROG_MDL; i++) { h ^= p[i]; h *= F; }
    p = (const uint8_t *)E->cm;
    for (size_t i = 0; i < sizeof(mdl64_t) * N_MDL; i++) { h ^= p[i]; h *= F; }
    h ^= (uint64_t)E->npr; h *= F; h ^= (uint64_t)E->nrr; h *= F;
    h ^= (uint64_t)E->qs; h *= F; h ^= (uint64_t)E->last_op; h *= F;
    h ^= (uint64_t)E->all_run; h *= F; h ^= (uint64_t)E->nn_cnt; h *= F;
    h ^= (uint64_t)E->prev_copy; h *= F;
    h ^= (uint64_t)E->prev_dx; h *= F; h ^= (uint64_t)E->prev_dy; h *= F;
    h ^= (uint64_t)E->last_tmv[0]; h *= F; h ^= (uint64_t)E->last_tmv[1]; h *= F;
    h ^= (uint64_t)E->last_imv[0]; h *= F; h ^= (uint64_t)E->last_imv[1]; h *= F;
    h ^= (uint64_t)E->palq_n; h *= F;
    p = (const uint8_t *)E->palq;
    for (int i = 0; i < PALQ_N * (int)sizeof(int16_t); i++) { h ^= p[i]; h *= F; }
    p = (const uint8_t *)&E->pre_slot;
    for (size_t i = 0; i < sizeof E->pre_slot; i++) { h ^= p[i]; h *= F; }
    {
        int ntx = (E->w + TILE_SZ - 1) / TILE_SZ;
        p = (const uint8_t *)E->upstat;
        for (int i = 0; i < ntx; i++) { h ^= p[i]; h *= F; }
        p = (const uint8_t *)E->mv_ax;
        for (int i = 0; i < ntx * (int)sizeof(int); i++) { h ^= p[i]; h *= F; }
        p = (const uint8_t *)E->mv_ay;
        for (int i = 0; i < ntx * (int)sizeof(int); i++) { h ^= p[i]; h *= F; }
        if (E->ptab) {
            int nty = (E->h + TILE_SZ - 1) / TILE_SZ;
            p = (const uint8_t *)E->ptab;
            for (size_t i = 0; i < sizeof(vop_t) * (size_t)(ntx * nty); i++) { h ^= p[i]; h *= F; }
        }
    }
    if (E->vops) {
        int ntx = (E->w + TILE_SZ - 1) / TILE_SZ;
        p = (const uint8_t *)E->vops;
        for (size_t i = 0; i < sizeof(vop_t) * (size_t)VOP_CELLS(ntx); i++) { h ^= p[i]; h *= F; }
    }
    return h;
}

static uint64_t txn_hash_rc(const ectx_t *E, int tx, int ty,
                            int tw, int th) {
    const uint64_t F = 0x100000001b3ull;
    uint64_t h = 1469598103934665603ull;
    if (E->recon)
        for (int y = ty; y < ty + th; y++) {
            const uint8_t *p = (const uint8_t *)(E->recon + (size_t)y * E->w + tx);
            for (int i = 0; i < tw * (int)sizeof(int16_t); i++) { h ^= p[i]; h *= F; }
        }
    return h;
}

static _Atomic long zemit_n[2];
_Atomic long pcw[160], pcl[160];
static int enc_one_tile(ectx_t *E, int tx, int ty, int tw, int th,
                        int depth, pstate_t *Sio, int emit)
{
    if (ZE(zd2)) { extern _Atomic long zcalls_e;
                   atomic_fetch_add(&zcalls_e,1);
                   atomic_fetch_add(&zemit_n[emit & 1], 1); }
    if (depth == 0 && E->ptab) {
        E->pre_slot = E->ptab[E->cur_ti];
        if (ZE(zps2)) fprintf(stderr, "SLOT ti=%d pre.op=%d pre.dx=%d pre.rw=%d\n",
                E->cur_ti, E->pre_slot.op, E->pre_slot.dx, E->pre_slot.rw);
    }
    int npix = tw * th;
    const int oc = PM_OP0 + (E->last_op << 1)
                 + (depth == 0 && !ZE(znoup) ? E->upstat[tx >> 4] : 0);

            /* RUN: tile identical to co-located E->prev -> one symbol wins */
            if (E->has_prev) {
                /* bounded RUN: recon is prev verbatim, so any tile whose
                   drift stays inside the live step's own error bound is
                   free - at qs=1 the bound is zero and this is exact */
                int lim = (E->qs - 1) >> 1;
                int same = 1;
                for (int y = ty; y < ty + th && same; y++)
                    for (int x = tx; x < tx + tw; x++) {
                        int d = E->curr[(size_t)y * E->w + x]
                              - E->prev[(size_t)y * E->w + x];
                        if (d < 0) d = -d;
                        if (d > lim) { same = 0; break; }
                    }
                if (same) {
                    if (E->recon)
                        for (int y = ty; y < ty + th; y++)
                            memcpy(E->recon + (size_t)y * E->w + tx,
                                   E->prev + (size_t)y * E->w + tx,
                                   (size_t)tw * sizeof(int16_t));
                    if (emit) {
                        if (ZE(zd2)) fprintf(stderr,
                            "E ti=%d op=%d lo=%d d%d\n",
                            E->cur_ti, OP_RUN, E->last_op, depth);
                        E->precs[E->npr++] = ((uint32_t)oc << 16) | OP_RUN;
                        mdl_bump(&E->pmc[oc], &E->pmc[PM_HI], oc, OP_RUN);
                        E->last_op = OP_RUN;
                        if (depth == 0)
                            E->upstat[tx >> 4] = 0;
                        E->mv_ax[tx / TILE_SZ] = 0x7FFFFFFF;
                        E->prev_copy = 0;
                        /* mirror the decoder: every leaf updates the
                           vertical-repeat cell and (top level) the
                           persistent cross-frame slot */
                        vop_t *ve = &E->vops[vcell_idx(E->ntx, tx, tw)];
                        vdu_log(ve);
                        ve->rx = (int16_t)tx; ve->ry = (int16_t)ty;
                        ve->rw = (int16_t)tw; ve->op = OP_RUN;
                        ve->dx = 0; ve->dy = 0; ve->npal = 0; ve->sig = 0;
                        if (E->ptab && depth == 0) {
                            vdu_log(&E->ptab[E->cur_ti]);
                            E->ptab[E->cur_ti] = *ve;
                        }
                    }
                    if (!emit && E->cap_out) {
                        /* deterministic early path: reproduces identically
                           at commit - mark transparent so the replay
                           prefix is not broken by a leaf that was never
                           evaluated */
                        E->cap_out->valid = 1; E->cap_out->op = -1;
                    }
                    if (!emit) {
                        /* probe-sibling fidelity: a real RUN commit leaves
                           these scalars behind - mirror them so the next
                           probe child sees the post-state */
                        E->last_op = OP_RUN;
                        E->mv_ax[tx / TILE_SZ] = 0x7FFFFFFF;
                        E->prev_copy = 0;
                    }
                    return mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_RUN);
                }
            }
            E->all_run = 0;   /* probes advance it too -
                                 sibling fidelity, restored at probe end */

            /* ---- candidate evaluation ---- */
            struct timespec ztt;
            if (ZE(zt)) clock_gettime(CLOCK_MONOTONIC, &ztt);
            txn_t txn; int txn_open = 0;
            pstate_t *Sreal = Sio;
            int bop = OP_PRED, bdx = 0, bdy = 0, bpk = 0, bpred = 1;
            int bth[PAL_MAX];
            int bq = E->qs, bsig = 0;
            int bcost;
            /* SAD of the best temporal COPY candidate; INT_MAX if none.
               Rescue and the off-lattice diamond both key off this so
               grain tiles (huge SAD, no exact MV) skip the window
               sweep, while a near-miss handshake still refines. */
            int copy_sad0 = 0x7FFFFFFF, sad_bx = 0, sad_by = 0;
            size_t bnr = 0;
            int dslot = depth < ENC_MAXD ? depth : ENC_MAXD - 1;
            pstate_t *bSp = &E->bS_stk[dslot];
            /* bS delta: when the winning sim ran under pu, its post
               state is fully described by pu's dirty-row list - capture
               just those rows instead of the whole 16KB struct */
            int bnd = -1;                        /* <0: bS is a full copy */
            uint16_t *bdc = E->bdc_stk[dslot];
            int32_t (*bdr)[NSPAT + 2] = E->bdr_stk + (size_t)dslot * N_CTX;
            int32_t bAr = 0, bNr = 0;
            /* ping-pong record scratch: the winner's buffer becomes
               brecs, the old one becomes the next candidate's scratch */
            uint32_t *recbuf = E->recbuf_stk
                             + (size_t)dslot * (TILE_SZ * TILE_SZ * 4 + 32);
            uint32_t *wcur = recbuf,
                     *brecs = recbuf + TILE_SZ * TILE_SZ * 2 + 16;
            /* spatial-product cache: ctx index, gradient bucket and the
               8 spatial predictors are identical across every candidate
               of this tile while recon == NULL (pure-lossless path) */
            pxcache_t *pxc = E->pxc_stk + (size_t)dslot * TILE_SZ * TILE_SZ;
            pxcache_t *pxp = NULL;
            if (!E->recon) {
                pxp = pxc;
                for (int i = 0; i < TILE_SZ * TILE_SZ; i++)
                    pxc[i].ctxf = -1;
            }
            int rcv = 0;
            E->minleaf = 0;
            if (emit && E->pcap != INT32_MAX) {
                /* floor of every op-only leaf record at this op-ctx -
                   any child outcome costs at least this, so a probe
                   budget below it means certain probe death */
                static const uint32_t opo[9] = {
                    OP_RUN, OP_MVRUNP, OP_COPYP, OP_IRUNP, OP_PREDZ,
                    OP_NNZ, OP_VRPT, OP_PSAME, OP_PSAMX
                };
                int m = INT32_MAX;
                for (int i = 0; i < 9; i++) {
                    int c = mlensym_cost_q8(&E->pmc[oc],
                                            &E->pmc[PM_HI], opo[i]);
                    if (c < m) m = c;
                }
                E->minleaf = m;
            }
#define BS_SAVE(PU) do { \
    pundo_t *_u = (PU); \
    bnd = _u->n; bAr = Sio->A_run; bNr = Sio->N_run; \
    for (int _i = 0; _i < bnd; _i++) { \
        int _c = _u->ctx[_i]; bdc[_i] = (uint16_t)_c; \
        memcpy(bdr[_i], Sio->As[_c], NSPAT * sizeof(int32_t)); \
        bdr[_i][NSPAT] = Sio->At[_c]; \
        bdr[_i][NSPAT + 1] = Sio->N[_c]; \
    } } while (0)
            uint8_t bidx[TILE_SZ * TILE_SZ];
            int16_t bpal[PAL_MAX];
            int16_t brecon[TILE_SZ * TILE_SZ];
            int capn = 0;

            if (emit && E->cap_rep && !ZE(znrep)) {
                /* probe replay: this leaf was fully decided during the
                   emit=0 probe under an identical entry state - restore
                   the bundle and jump straight to emission. */
                cap_t *cpk = E->cap_rep;
                bop = cpk->op; bdx = cpk->dx; bdy = cpk->dy;
                bpk = cpk->pk; bq = cpk->bq; bsig = cpk->sig;
                bpred = cpk->pred; bcost = cpk->cost; bnr = cpk->nrec;
                memcpy(bth, cpk->th, sizeof bth);
                memcpy(bpal, cpk->pal, sizeof bpal);
                memcpy(bidx, cpk->idx, sizeof bidx);
                memcpy(brecon, cpk->rc, sizeof brecon);
                bnd = cpk->bnd; bAr = cpk->ar; bNr = cpk->nr;
                if (bnd >= 0) {
                    memcpy(bdc, cpk->u.dl.dc,
                           (size_t)bnd * sizeof(uint16_t));
                    memcpy(bdr, cpk->u.dl.dr,
                           (size_t)bnd * sizeof(*bdr));
                } else {
                    *bSp = cpk->u.bs;
                }
                /* predicted-MV ops carry no MV record - the decoder
                   resolves it from its own live state.  The probe-time
                   check was against probe scalars; siblings committed
                   since may have moved last_tmv/last_imv, so demote to
                   the explicit-MV variant when the prediction drifted. */
                if (bop == OP_MVRUNP &&
                    (bdx != E->last_tmv[0] || bdy != E->last_tmv[1]))
                    bop = OP_MVRUN;
                else if (bop == OP_COPYP &&
                         (bdx != E->last_tmv[0] || bdy != E->last_tmv[1]))
                    bop = OP_COPY;
                else if (bop == OP_IRUNP &&
                         (bdx != E->last_imv[0] || bdy != E->last_imv[1]))
                    bop = OP_IRUN;
                brecs = cpk->recs;
                cp_ok = 0;
                goto cap_commit;
            }

            /* fast skip: the predicted temporal MV already exact? Commit
               MVRUNP immediately - nothing beats op-only coding, and it
               keeps the RPT chain alive for uniform motion.  Runs before
               the pred/palette gauntlet: a hit skips the whole ladder. */
            if (E->has_prev) {
                int dx = E->last_tmv[0], dy = E->last_tmv[1];
                int hit = 0;
                if (mv_contained(E, tx, ty, tw, th, dx, dy))
                    hit = tile_maxd(E->curr + (size_t)ty * E->w + tx,
                              E->prev + (size_t)(ty + dy) * E->w + tx + dx,
                              E->w, tw, th) <= (E->qs - 1) / 2;
                else if (mv_full_hang(E, tx, ty, tw, th, dx, dy))
                    hit = tile_maxd_clamp(E->curr, E->prev, E->w, E->h,
                              tx, ty, tw, th, dx, dy) <= (E->qs - 1) / 2;
                if (hit) {
                    bcost = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_MVRUNP);
                    bop = OP_MVRUNP; bdx = dx; bdy = dy; bpred = 0;
                    ZT_ACC(0, ztt);
                    goto alt_done;
                }
            }
            if (E->has_prev && E->prev_copy) {
                /* scroll continuation: the previous COPY leaf's vector
                   often stays exact on the next tile - one maxd probe
                   replaces the whole ladder.  MVRUNP fires first, so
                   reaching here means the predicted MV already missed. */
                int dx = E->prev_dx, dy = E->prev_dy;
                int hit = 0;
                if (mv_contained(E, tx, ty, tw, th, dx, dy))
                    hit = tile_maxd(E->curr + (size_t)ty * E->w + tx,
                              E->prev + (size_t)(ty + dy) * E->w + tx + dx,
                              E->w, tw, th) <= (E->qs - 1) / 2;
                else if (mv_full_hang(E, tx, ty, tw, th, dx, dy))
                    hit = tile_maxd_clamp(E->curr, E->prev, E->w, E->h,
                              tx, ty, tw, th, dx, dy) <= (E->qs - 1) / 2;
                if (hit) {
                    int ip = (dx == E->last_tmv[0] &&
                              dy == E->last_tmv[1]);
                    bcost = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                ip ? OP_MVRUNP : OP_MVRUN)
                          + (ip ? 0 :
                             mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                 zigzag_enc(dx - mvpred(E, OP_MVRUN, 0, depth, tx, tw)))
                           + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                 zigzag_enc(dy - mvpred(E, OP_MVRUN, 1, depth, tx, tw))));
                    bop = ip ? OP_MVRUNP : OP_MVRUN;
                    bdx = dx; bdy = dy; bpred = 0;
                    ZT_ACC(0, ztt);
                    goto alt_done;
                }
            }
            ZT_ACC(0, ztt);


            {   /* Inter baseline: PRED (flags, 36-slot residual).
                   Intra baseline: PREDNFX (dense fold). Starting intra
                   at PRED trained slots 0-35 first and starved fold. */
                int base_lm = E->has_prev ? 0 : 4;
                int base_op = E->has_prev ? OP_PRED : OP_PREDNFX;
                pundo_t *pu = &E->pu;
                size_t nt = 0;
                pu_begin(pu, Sio);
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              NULL, E->luma, E->recon, E->qs, E->cm, base_lm, NULL, pu, INT_MAX, &rcv, pxp);
                bcost = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], base_op)
                      + rcv;
                bop = base_op;
                { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                /* emit=1 bail is exact only when the remaining probe
                   budget is under the cheapest possible leaf emission
                   (an op-only record - RUN/MVRUNP/VRPT/... at this oc):
                   nothing can fit, the parent probe is certainly dead */
                if (bcost > E->pcap &&
                    (!emit || E->pcap <= E->minleaf)) {
                    pu_rollback(pu, Sio);
                    return bcost;
                }
                if (E->recon)
                    tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                bnr = nt; BS_SAVE(pu); pu_rollback(pu, Sio);
            }

            if ((emit || ZE(zrl0)) && E->has_prev && !ZE(znpx)) {
                /* PREDNFX first so fold models train on dense inter
                   tiles; PREDNF (36-slot dense) is the fallback. */
                pundo_t *pu = &E->pu;
                size_t nt = 0;
                int c;
                if (bcost > tw * th * 160) {
                pu_begin(pu, Sio);
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              NULL, E->luma, E->recon, E->qs, E->cm, 4, NULL, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_PREDNFX), &rcv, pxp);
                c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PREDNFX)
                  + rcv;
                if (c < bcost) {
                    bcost = c; bop = OP_PREDNFX;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
                }
                /* PREDX: flags plus fold. Catches tiles PRED won on
                   runs but whose residuals still sat in 36 slots. */
                if (bcost > tw * th * 80) {
                pu_begin(pu, Sio);
                nt = 0;
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              NULL, E->luma, E->recon, E->qs, E->cm, 5, NULL, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_PREDX), &rcv, pxp);
                c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PREDX)
                  + rcv;
                if (c < bcost) {
                    bcost = c; bop = OP_PREDX;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
                }
                /* 36-slot dense is the fallback only when fold lost. */
                if (bop != OP_PREDNFX && bop != OP_PREDX) {
                pu_begin(pu, Sio);
                nt = 0;
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              NULL, E->luma, E->recon, E->qs, E->cm, 1, NULL, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_PREDNF), &rcv, pxp);
                c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PREDNF)
                      + rcv;
                if (c < bcost) {
                    bcost = c; bop = OP_PREDNF;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
                }
            } else if (emit || ZE(zrl0)) {
                /* Intra 36-slot PRED fallback only on blowout tiles.
                   Shared 36 models beat 3600-way fold on near-raw
                   noise (test7 random strips). Camera texture stays
                   on the fold baseline so those models actually train.
                   1024 q8/px = 4 bpp. */
                pundo_t *pu = &E->pu;
                size_t nt = 0;
                int c;
                if (bcost > tw * th * 1024) {
                pu_begin(pu, Sio);
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              NULL, E->luma, E->recon, E->qs, E->cm, 0, NULL, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_PRED), &rcv, pxp);
                c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PRED)
                      + rcv;
                if (c < bcost) {
                    bcost = c; bop = OP_PRED;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
                }
                /* PREDIR: intra flag subset {0,2,3,4,5} on 36-slot
                   residual. PREDX is the same alphabet on the fold. */
                pu_begin(pu, Sio);
                nt = 0;
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              NULL, E->luma, E->recon, E->qs, E->cm, 3, NULL, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_PREDIR), &rcv, pxp);
                c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PREDIR)
                      + rcv;
                if (ZE(zrs) && E->cur_ti < 6)
                    fprintf(stderr, "PIR ti=%d pir=%d base=%d bop=%d nt=%d\n",
                            E->cur_ti, c, bcost, bop, (int)nt);
                if (c < bcost) {
                    bcost = c; bop = OP_PREDIR;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
                /* PREDX on intra: same flags as PREDIR, fold residual.
                   Skip cheap tiles; PAL/ROW already win those later. */
                if (bcost > tw * th * 80) {
                pu_begin(pu, Sio);
                nt = 0;
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              NULL, E->luma, E->recon, E->qs, E->cm, 5, NULL, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_PREDX), &rcv, pxp);
                c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PREDX)
                  + rcv;
                if (c < bcost) {
                    bcost = c; bop = OP_PREDX;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
                }
            }

            if ((emit || ZE(zrl0)) && E->W && !ZE(znpx)) {   /* NN: same machinery, learned predictor */

                pundo_t *pu = &E->pu;
                size_t nt = 0;
                pu_begin(pu, Sio);
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              E->W, E->luma, E->recon, E->qs, E->cm, 0, NULL, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_NN), &rcv, pxp);
                int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_NN)
                      + rcv;
                if (c < bcost) {
                    bcost = c; bop = OP_NN;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
            }

            if ((emit || ZE(zrl0)) && E->W && E->has_prev) {   /* NNF: learned pred, no flags */
                pundo_t *pu = &E->pu;
                size_t nt = 0;
                pu_begin(pu, Sio);
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              E->W, E->luma, E->recon, E->qs, E->cm, 1, NULL, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_NNF), &rcv, pxp);
                int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_NNF)
                      + rcv;
                if (c < bcost) {
                    bcost = c; bop = OP_NNF;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
            }

            {   /* PREDZ: leaf within quantizer bound of the spatial
                   predictor -> a bare op code, no residual stream */
                pundo_t *pu = &E->pu;
                size_t nt = 0; int zok = 1;
                pu_begin(pu, Sio);
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              NULL, E->luma, E->recon, E->qs, E->cm, 2, &zok, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_PREDZ), NULL, pxp);
                if (zok) {
                    int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                            OP_PREDZ);
                    if (c < bcost) {
                        bcost = c; bop = OP_PREDZ; bnr = 0; BS_SAVE(pu);
                        if (E->recon)
                            tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                        bpred = 1;
                    }
                }
                pu_rollback(pu, Sio);
            }

            if ((emit || ZE(zrl0)) && E->W && !ZE(znpx)) {   /* NNZ: residual-free learned-pred leaf */
                pundo_t *pu = &E->pu;
                size_t nt = 0; int zok = 1;
                pu_begin(pu, Sio);
                sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                              0, 0, E->has_prev, Sio, wcur, &nt,
                              E->W, E->luma, E->recon, E->qs, E->cm, 2, &zok, pu,
                              bcost - mlensym_cost_q8(&E->pmc[oc],
                                  &E->pmc[PM_HI], OP_NNZ), NULL, pxp);
                if (zok) {
                    int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                            OP_NNZ);
                    if (c < bcost) {
                        bcost = c; bop = OP_NNZ; bnr = 0; BS_SAVE(pu);
                        if (E->recon)
                            tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                        bpred = 1;
                    }
                }
                pu_rollback(pu, Sio);
            }

            /* cheap-leaf gate: when the pred family already priced the
               tile near the floor, palette/copy/motion candidates cannot
               recover the gap - skip them (stream still legal, just the
               ladder subset).  ZSK = q8 threshold, 0 disables. */
            if (ZE_.zsk > 0 && bcost <= ZE_.zsk * 256)
                goto cheap_done;

            /* CFL: chroma ~ a*luma + b per-leaf affine.  Only legal when
               a luma plane exists (chroma jobs); the frame-level API always
               encodes luma losslessly so orig == recon on both sides. */
            if ((emit || ZE(zrl0)) && E->luma) {
                int64_t sl = 0, sc2 = 0, sll = 0, slc = 0;
                for (int y = ty; y < ty + th; y++)
                    for (int x = tx; x < tx + tw; x++) {
                        int l = E->luma[(size_t)y * E->w + x];
                        int cv = E->curr[(size_t)y * E->w + x];
                        sl += l; sc2 += cv;
                        sll += (int64_t)l * l; slc += (int64_t)l * cv;
                    }
                double np = (double)npix;
                double var = sll - sl * (double)sl / np;
                double cov = slc - sl * (double)sc2 / np;
                int ca = var > 1.0 ? (int)(cov * 64.0 / var
                                       + (cov >= 0 ? 0.5 : -0.5)) : 0;
                if (ca > 255) ca = 255; else if (ca < -256) ca = -256;
                double braw = (sc2 - ca * (double)sl / 64.0) / np;
                int cb = (int)(braw + (braw >= 0 ? 0.5 : -0.5));
                if (cb > 8191) cb = 8191; else if (cb < -8192) cb = -8192;
                pundo_t *pu = &E->pu;
                size_t nt = 0;
                pu_begin(pu, Sio);
                sim_cfl_tile(E, tx, ty, tw, th, ca, cb, Sio,
                             wcur, &nt, E->qs, pu);
                int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_CFL)
                      + mlensym_cost_q8(&E->pmc[PM_CFLA], &E->pmc[PM_HI],
                                        zigzag_enc(ca))
                      + mlensym_cost_q8(&E->pmc[PM_CFLB], &E->pmc[PM_HI],
                                        zigzag_enc(cb))
                      + recs_cost_q8(wcur, nt, E->cm);
                if (c < bcost) {
                    bcost = c; bop = OP_CFL; bdx = ca; bdy = cb;
                    { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                    if (E->recon)
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                    bnr = nt; BS_SAVE(pu); bpred = 1;
                }
                pu_rollback(pu, Sio);
            }

            if (bcost > E->pcap && (!emit || E->pcap <= E->minleaf))
                return bcost;

            /* PAL: tile with <= PAL_MAX distinct in-range values.
               Acceptance window is v = px+LIT_BIAS in [0,511]: values
               stream len-coded so extremes cost a few extra bits only
               for the palette entries that need them (pure-white luma
               and saturated chroma both sit at the old cap's edge). */
            if ((emit || ZE(zrl0)) && !ZE(znpal)) {
                /* epoch-tagged presence table: avoids a 512B memset per
                   leaf - stamp_hi(v)==pal_epoch means seen, low byte the
                   palette index */
                uint32_t *seen = E->pal_seen;
                E->pal_epoch++;
                if (!E->pal_epoch) {
                    memset(seen, 0, 512 * sizeof(uint32_t));
                    E->pal_epoch = 1;
                }
                int k = 0, ok = 1;
                uint8_t imap[TILE_SZ * TILE_SZ];
                int16_t pal[PAL_MAX];
                for (int y = ty; y < ty + th && ok; y++) {
                    for (int x = tx; x < tx + tw; x++) {
                        int v = E->curr[(size_t)y * E->w + x] + LIT_BIAS;
                        if (v < 0 || v > 511) { ok = 0; break; }
                        if (seen[v] >> 8 != (uint32_t)E->pal_epoch) {
                            if (k == PAL_MAX) { ok = 0; break; }
                            seen[v] = ((uint32_t)E->pal_epoch << 8)
                                    | (uint32_t)(k + 1);
                            pal[k] = (int16_t)(v - LIT_BIAS);
                            k++;
                        }
                        imap[(y - ty) * tw + (x - tx)] = (uint8_t)((seen[v] & 255) - 1);
                    }
                }
                /* palette pricing is a separate pass now: the index map
                   must be complete for the PALR/PALT/PALD siblings even
                   when PAL itself is already over budget */
                int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PAL);
                if (ok && k > 0) {
                    for (int i = 0; i < k && c < bcost; i++)
                        c += mlensym_cost_q8(&E->pmc[PM_PALV], &E->pmc[PM_HI],
                                             (uint32_t)(pal[i] + LIT_BIAS));
                    for (int i = 0; i < npix && c < bcost; i++)
                        c += mlensym_cost_q8(&E->pmc[PM_PALI], &E->pmc[PM_HI],
                                             (uint32_t)imap[i]);
                    c += mlensym_cost_q8(&E->pmc[PM_PALN], &E->pmc[PM_HI], (uint32_t)(k - 1));
                    if (c < bcost) {
                        bcost = c; bop = OP_PAL; bpk = k; bpred = 0;
                        memcpy(bidx, imap, (size_t)npix);
                        memcpy(bpal, pal, (size_t)k * sizeof(int16_t));
                    }
                    /* PALR: same palette, index map as (idx,run) pairs */
                    int rc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PALR)
                           + mlensym_cost_q8(&E->pmc[PM_PALN], &E->pmc[PM_HI],
                                            (uint32_t)(k - 1));
                    for (int i = 0; i < k; i++)
                        rc += mlensym_cost_q8(&E->pmc[PM_PALV], &E->pmc[PM_HI],
                                              (uint32_t)(pal[i] + LIT_BIAS));
                    for (int i = 0; i < npix && rc < bcost; ) {
                        int r = 1;
                        while (i + r < npix && imap[i + r] == imap[i]) r++;
                        rc += mlensym_cost_q8(&E->pmc[PM_PALI], &E->pmc[PM_HI],
                                              (uint32_t)imap[i])
                            + mlensym_cost_q8(&E->pmc[PM_REPN], &E->pmc[PM_HI],
                                              (uint32_t)(r - 1));
                        i += r;
                    }
                    if (rc < bcost) {
                        bcost = rc; bop = OP_PALR; bpk = k; bpred = 0;
                        memcpy(bidx, imap, (size_t)npix);
                        memcpy(bpal, pal, (size_t)k * sizeof(int16_t));
                    }
                    /* ROWD: same palette, but each distinct row is sent
                       once and later rows are a copy-above flag. Wins
                       on UI tiles where PREDIR re-prices a row every
                       time a glyph edge breaks the up-run. */
                    {
                        int rdc = rowd_body(E, oc, 0, 0, 0, imap, tw, th, k,
                                            pal, bcost);
                        if (rdc < bcost) {
                            bcost = rdc; bop = OP_ROWD; bpk = k; bpred = 0;
                            memcpy(bidx, imap, (size_t)npix);
                            memcpy(bpal, pal, (size_t)k * sizeof(int16_t));
                        }
                        int rqc = rowd_body(E, oc, 0, 1, 0, imap, tw, th, k,
                                            pal, bcost);
                        if (rqc < bcost) {
                            bcost = rqc; bop = OP_ROWQ; bpk = k; bpred = 0;
                            memcpy(bidx, imap, (size_t)npix);
                            memcpy(bpal, pal, (size_t)k * sizeof(int16_t));
                        }
                        int rsc = rowd_body(E, oc, 0, 0, 1, imap, tw, th, k,
                                            pal, bcost);
                        if (rsc < bcost) {
                            bcost = rsc; bop = OP_ROWS; bpk = k; bpred = 0;
                            memcpy(bidx, imap, (size_t)npix);
                            memcpy(bpal, pal, (size_t)k * sizeof(int16_t));
                        }
                    }
                    /* PALT: index map synthesized from luma.  When the
                       per-entry luma ranges are strictly separable the
                       palette is emitted in luma order plus k-1
                       boundaries; the decoder derives idx = rank(lu)
                       and the entire index map costs zero records.
                       This is the chroma analogue of OP_CFL for
                       palettized content (text/UI). */
                    if ((emit || ZE(zrl0)) && E->luma && !ZE(znpt)) {
                        int mn[PAL_MAX], mx[PAL_MAX], ord[PAL_MAX],
                            cnt[PAL_MAX];
                        double ms[PAL_MAX];
                        for (int j = 0; j < k; j++) {
                            mn[j] = 32767; mx[j] = -32768;
                            ord[j] = j; cnt[j] = 0; ms[j] = 0.0;
                        }
                        for (int i = 0; i < npix; i++) {
                            int lu = E->luma[(size_t)(ty + i / tw) * E->w
                                             + tx + i % tw];
                            int j = imap[i];
                            if (lu < mn[j]) mn[j] = lu;
                            if (lu > mx[j]) mx[j] = lu;
                            ms[j] += lu; cnt[j]++;
                        }
                        for (int j = 0; j < k; j++)
                            if (cnt[j]) ms[j] /= cnt[j]; else ms[j] = mn[j];
                        for (int a = 1; a < k; a++)
                            for (int b = a;
                                 b > 0 && mn[ord[b - 1]] > mn[ord[b]]; b--) {
                                int t = ord[b - 1];
                                ord[b - 1] = ord[b]; ord[b] = t;
                            }
                        int okk = 1;
                        for (int j = 0; j + 1 < k && okk; j++)
                            if (mx[ord[j]] >= mn[ord[j + 1]]) okk = 0;
                        if (okk) {
                            int rank[PAL_MAX];
                            int rt = mlensym_cost_q8(&E->pmc[oc],
                                    &E->pmc[PM_HI], OP_PALT)
                                   + mlensym_cost_q8(&E->pmc[PM_PALN],
                                    &E->pmc[PM_HI], (uint32_t)(k - 1));
                            for (int j = 0; j < k; j++) {
                                rank[ord[j]] = j;
                                rt += mlensym_cost_q8(&E->pmc[PM_PALV],
                                      &E->pmc[PM_HI],
                                      (uint32_t)(pal[ord[j]] + LIT_BIAS));
                                if (j + 1 < k)
                                    rt += mlensym_cost_q8(&E->pmc[PM_CFLB],
                                          &E->pmc[PM_HI],
                                          zigzag_enc(mn[ord[j + 1]]));
                            }
                            if (rt < bcost) {
                                bcost = rt; bop = OP_PALT; bpk = k;
                                bpred = 0;
                                for (int j = 0; j < k; j++) {
                                    bpal[j] = pal[ord[j]];
                                    if (j + 1 < k) bth[j] = mn[ord[j + 1]];
                                }
                                for (int i = 0; i < npix; i++)
                                    bidx[i] = (uint8_t)rank[imap[i]];
                            }
                            /* PALQ: same luma-ranked index map, but each
                               palette value is a recent-color slot when
                               this plane has already sent it. A cold
                               miss (symbol 8 + literal) loses to PALT,
                               so the first copy of a color stays PALT
                               and fills the cache. */
                            int qc = mlensym_cost_q8(&E->pmc[oc],
                                        &E->pmc[PM_HI], OP_PALQ)
                                   + mlensym_cost_q8(&E->pmc[PM_PALN],
                                        &E->pmc[PM_HI], (uint32_t)(k - 1));
                            for (int j = 0; j < k && qc < bcost; j++) {
                                int v = pal[ord[j]];
                                int slot = -1;
                                for (int s = 0; s < E->palq_n; s++)
                                    if (E->palq[s] == v) { slot = s; break; }
                                if (slot >= 0)
                                    qc += mlensym_cost_q8(&E->pmc[PM_PALQ],
                                          &E->pmc[PM_HI], (uint32_t)slot);
                                else
                                    qc += mlensym_cost_q8(&E->pmc[PM_PALQ],
                                          &E->pmc[PM_HI], PALQ_MISS)
                                       + mlensym_cost_q8(&E->pmc[PM_PALV],
                                          &E->pmc[PM_HI],
                                          (uint32_t)(v + LIT_BIAS));
                                if (j + 1 < k)
                                    qc += mlensym_cost_q8(&E->pmc[PM_CFLB],
                                          &E->pmc[PM_HI],
                                          zigzag_enc(mn[ord[j + 1]]));
                            }
                            if (qc < bcost) {
                                bcost = qc; bop = OP_PALQ; bpk = k;
                                bpred = 0;
                                for (int j = 0; j < k; j++) {
                                    bpal[j] = pal[ord[j]];
                                    if (j + 1 < k) bth[j] = mn[ord[j + 1]];
                                }
                                for (int i = 0; i < npix; i++)
                                    bidx[i] = (uint8_t)rank[imap[i]];
                            }
                            /* PALZ: same luma-ranked map, values are
                               zigzag(value) on a model peaked at 0. The
                               decoder accepts any boundary strictly
                               between two luma groups, so place it as
                               high as a class-4 literal delta allows.
                               A gap wider than 30 then does not pay the
                               8 raw bits of the next group's min luma. */
                            int zc = mlensym_cost_q8(&E->pmc[oc],
                                        &E->pmc[PM_HI], OP_PALZ)
                                   + mlensym_cost_q8(&E->pmc[PM_PALN],
                                        &E->pmc[PM_HI], (uint32_t)(k - 1));
                            int zpred = 0;
                            int zthr[PAL_MAX];
                            for (int j = 0; j < k && zc < bcost; j++) {
                                zc += mlensym_cost_q8(&E->pmc[PM_PALZ],
                                      &E->pmc[PM_HI],
                                      zigzag_enc(pal[ord[j]]));
                                if (j + 1 < k) {
                                    int lo = mx[ord[j]] + 1;
                                    int hi = mn[ord[j + 1]];
                                    int thr = hi;
                                    if (hi - zpred > 30 && lo - zpred <= 30)
                                        thr = zpred + 30;
                                    zthr[j] = thr;
                                    zc += mlensym_cost_q8(&E->pmc[PM_CFLB],
                                          &E->pmc[PM_HI],
                                          zigzag_enc(thr - zpred));
                                    zpred = thr;
                                }
                            }
                            if (zc < bcost) {
                                bcost = zc; bop = OP_PALZ; bpk = k;
                                bpred = 0;
                                for (int j = 0; j < k; j++) {
                                    bpal[j] = pal[ord[j]];
                                    if (j + 1 < k) bth[j] = zthr[j];
                                }
                                for (int i = 0; i < npix; i++)
                                    bidx[i] = (uint8_t)rank[imap[i]];
                            }
                            /* PALU: cached values, same literal-bounded
                               delta boundaries as PALZ. A hit is a short
                               class-2 slot; PALZ would send the zigzag
                               again. Misses stay on PALZ. */
                            int uc = mlensym_cost_q8(&E->pmc[oc],
                                        &E->pmc[PM_HI], OP_PALU)
                                   + mlensym_cost_q8(&E->pmc[PM_PALN],
                                        &E->pmc[PM_HI], (uint32_t)(k - 1));
                            int upred = 0;
                            int uthr[PAL_MAX];
                            for (int j = 0; j < k && uc < bcost; j++) {
                                int v = pal[ord[j]];
                                int slot = -1;
                                for (int s = 0; s < E->palq_n; s++)
                                    if (E->palq[s] == v) { slot = s; break; }
                                if (slot >= 0)
                                    uc += mlensym_cost_q8(&E->pmc[PM_PALQ],
                                          &E->pmc[PM_HI], (uint32_t)slot);
                                else
                                    uc += mlensym_cost_q8(&E->pmc[PM_PALQ],
                                          &E->pmc[PM_HI], PALQ_MISS)
                                       + mlensym_cost_q8(&E->pmc[PM_PALV],
                                          &E->pmc[PM_HI],
                                          (uint32_t)(v + LIT_BIAS));
                                if (j + 1 < k) {
                                    int lo = mx[ord[j]] + 1;
                                    int hi = mn[ord[j + 1]];
                                    int thr = hi;
                                    if (hi - upred > 30 && lo - upred <= 30)
                                        thr = upred + 30;
                                    uthr[j] = thr;
                                    uc += mlensym_cost_q8(&E->pmc[PM_CFLB],
                                          &E->pmc[PM_HI],
                                          zigzag_enc(thr - upred));
                                    upred = thr;
                                }
                            }
                            if (uc < bcost) {
                                bcost = uc; bop = OP_PALU; bpk = k;
                                bpred = 0;
                                for (int j = 0; j < k; j++) {
                                    bpal[j] = pal[ord[j]];
                                    if (j + 1 < k) bth[j] = uthr[j];
                                }
                                for (int i = 0; i < npix; i++)
                                    bidx[i] = (uint8_t)rank[imap[i]];
                            }
                        }
                        /* PALC: flag-coded index map - per pixel a
                           PM_PALF flag says idx == idx_above (virtual
                           row 0 = idx 0); misses pull a PM_PALI literal.
                           Flag model sim is scratch-warmed so the cold
                           PM_PALF is priced as the decode will pay. */
                        if (emit || ZE(zrl0)) {
                            mdl64_t fm;
                            memcpy(&fm, &E->pmc[PM_PALF], sizeof fm);
                            int cc = mlensym_cost_q8(&E->pmc[oc],
                                    &E->pmc[PM_HI], OP_PALC)
                                   + mlensym_cost_q8(&E->pmc[PM_PALN],
                                    &E->pmc[PM_HI], (uint32_t)(k - 1));
                            for (int j = 0; j < k; j++)
                                cc += mlensym_cost_q8(&E->pmc[PM_PALV],
                                      &E->pmc[PM_HI],
                                      (uint32_t)(pal[j] + LIT_BIAS));
                            for (int i = 0; i < npix && cc < bcost; i++) {
                                int pu = i >= tw ? imap[i - tw] : 0;
                                int pl = (i % tw) ? imap[i - 1] : pu;
                                int c = imap[i] == pl ? 0
                                      : imap[i] == pu ? 1 : 2;
                                cc += mlensym_cost_q8(&fm, &E->pmc[PM_HI],
                                                      (uint32_t)c);
                                mdl_bump(&fm, &E->pmc[PM_HI], PM_PALF,
                                         (uint32_t)c);
                                if (c == 2)
                                    cc += mlensym_cost_q8(&E->pmc[PM_PALI],
                                          &E->pmc[PM_HI],
                                          (uint32_t)imap[i]);
                            }
                            if (ZE(zpc) && bcost > 30000)
                                fprintf(stderr, "PC %d,%d k=%d cc=%d bc=%d\n",
                                        tx, ty, k, cc, bcost);
                            if (cc < bcost) {
                                bcost = cc; bop = OP_PALC; bpk = k;
                                bpred = 0;
                                memcpy(bidx, imap, (size_t)npix);
                                memcpy(bpal, pal,
                                       (size_t)k * sizeof(int16_t));
                            }
                        }
                        /* PALD: palette re-sorted by VALUE so adjacent
                           indices are near colours - the index map then
                           streams as med3 residuals instead of literals.
                           Wins on smooth/photo palettes where literal
                           indexes carry ~4-6 bits each. */
                        if (emit || ZE(zrl0)) {
                            int ord[PAL_MAX], rank[PAL_MAX];
                            for (int j = 0; j < k; j++) ord[j] = j;
                            for (int a2 = 1; a2 < k; a2++) {
                                int t = ord[a2], j2 = a2;
                                while (j2 > 0 &&
                                       pal[ord[j2 - 1]] > pal[t]) {
                                    ord[j2] = ord[j2 - 1]; j2--;
                                }
                                ord[j2] = t;
                            }
                            for (int j = 0; j < k; j++)
                                rank[ord[j]] = j;
                            uint8_t smap[TILE_SZ * TILE_SZ];
                            int16_t spal[PAL_MAX];
                            for (int j = 0; j < k; j++)
                                spal[j] = pal[ord[j]];
                            for (int i = 0; i < npix; i++)
                                smap[i] = (uint8_t)rank[imap[i]];
                            mdl64_t dm;
                            memcpy(&dm, &E->pmc[PM_PALD], sizeof dm);
                            int cc = mlensym_cost_q8(&E->pmc[oc],
                                    &E->pmc[PM_HI], OP_PALD)
                                   + mlensym_cost_q8(&E->pmc[PM_PALN],
                                    &E->pmc[PM_HI], (uint32_t)(k - 1));
                            for (int j = 0; j < k; j++)
                                cc += mlensym_cost_q8(&E->pmc[PM_PALV],
                                      &E->pmc[PM_HI],
                                      (uint32_t)(spal[j] + LIT_BIAS));
                            for (int i = 0; i < npix && cc < bcost; i++) {
                                int a = (i % tw) ? smap[i - 1] : 0;
                                int b = (i >= tw) ? smap[i - tw] : a;
                                int c2 = (i >= tw && (i % tw))
                                       ? smap[i - tw - 1] : a;
                                int p0 = a + b - c2;
                                int pa = p0 > a ? p0 - a : a - p0;
                                int pb = p0 > b ? p0 - b : b - p0;
                                int pc = p0 > c2 ? p0 - c2 : c2 - p0;
                                int pr = pa <= pb && pa <= pc ? a
                                       : pb <= pc ? b : c2;
                                uint32_t v = zigzag_enc(smap[i] - pr);
                                cc += mlensym_cost_q8(&dm, &E->pmc[PM_HI],
                                                      v);
                                mdl_bump(&dm, &E->pmc[PM_HI], PM_PALD, v);
                            }
                            if (cc < bcost) {
                                bcost = cc; bop = OP_PALD; bpk = k;
                                bpred = 0;
                                memcpy(bidx, smap, (size_t)npix);
                                memcpy(bpal, spal,
                                       (size_t)k * sizeof(int16_t));
                            }
                        }
                    }
                }
            }

            if (bcost > E->pcap && (!emit || E->pcap <= E->minleaf))
                return bcost;

            /* RAW: literals, bounded fallback */
            if (emit || ZE(zrl0)) {
                int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_RAW), ok = 1;
                for (int y = ty; y < ty + th && ok; y++) {
                    for (int x = tx; x < tx + tw; x++) {
                        int v = E->curr[(size_t)y * E->w + x] + LIT_BIAS;
                        if (v < 0 || v > 511) { ok = 0; break; }
                        c += mlensym_cost_q8(&E->pmc[PM_RAW], &E->pmc[PM_HI], (uint32_t)v);
                    }
                    if (c >= bcost) { ok = 0; break; }
                }
                if (ok && c < bcost) { bcost = c; bop = OP_RAW; bpred = 0; }
            }

            ZT_ACC(1, ztt);
            /* COPY: SAD screen over the MV set + neighbor-MV predictors,
               full sim for top 3 */
            if ((emit || ZE(zrl0) || zin_pre) && E->has_prev && !ZE(znmv)) {
                int cmv[N_MVC + 64][2], nc = 0;
                for (int i = 0; i < N_MVC; i++) {
                    cmv[nc][0] = mvc[i][0]; cmv[nc][1] = mvc[i][1]; nc++;
                }
                if (E->gmv_valid) {
                    cmv[nc][0] = E->gmv[0]; cmv[nc][1] = E->gmv[1]; nc++;
                    cmv[nc][0] = E->gmv[0]; cmv[nc][1] = E->gmv[1] + E->h; nc++;
                    cmv[nc][0] = E->gmv[0]; cmv[nc][1] = E->gmv[1] - E->h; nc++;
                    cmv[nc][0] = E->gmv[0] + E->w; cmv[nc][1] = E->gmv[1]; nc++;
                    cmv[nc][0] = E->gmv[0] - E->w; cmv[nc][1] = E->gmv[1]; nc++;
                }
                if (E->prev_copy) {
                    cmv[nc][0] = E->prev_dx; cmv[nc][1] = E->prev_dy; nc++;
                    /* wrap-around companions: content scrolling off one
                       edge re-enters from the opposite side of the same
                       reference frame */
                    cmv[nc][0] = E->prev_dx; cmv[nc][1] = E->prev_dy + E->h; nc++;
                    cmv[nc][0] = E->prev_dx; cmv[nc][1] = E->prev_dy - E->h; nc++;
                    cmv[nc][0] = E->prev_dx + E->w; cmv[nc][1] = E->prev_dy; nc++;
                    cmv[nc][0] = E->prev_dx - E->w; cmv[nc][1] = E->prev_dy; nc++;
                }
                if (E->mv_ax[tx / TILE_SZ] != 0x7FFFFFFF) {
                    cmv[nc][0] = E->mv_ax[tx / TILE_SZ];
                    cmv[nc][1] = E->mv_ay[tx / TILE_SZ]; nc++;
                    cmv[nc][0] = E->mv_ax[tx / TILE_SZ];
                    cmv[nc][1] = E->mv_ay[tx / TILE_SZ] + E->h; nc++;
                    cmv[nc][0] = E->mv_ax[tx / TILE_SZ];
                    cmv[nc][1] = E->mv_ay[tx / TILE_SZ] - E->h; nc++;
                }
                /* whole-frame tile dictionary: repeated content found
                   anywhere in the previous frame becomes an exact-MV
                   candidate.  Only full tiles hash (partial-edge tiles
                   keep the geometric probes). */
                if (E->pdict && tw == TILE_SZ && th == TILE_SZ) {
                    const tdent_t *e = td_get(E->pdict,
                        td_hash(E->curr + (size_t)ty * E->w + tx, E->w));
                    if (e)
                        for (int q = 0; q < e->n && nc < N_MVC + 63; q++) {
                            cmv[nc][0] = e->px[q] - tx;
                            cmv[nc][1] = e->py[q] - E->yoff - ty;
                            nc++;
                        }
                }
                /* follow the live temporal MV with a small hex: camera
                   handshake walks (last_tmv + {±1,±2}²) more often than
                   it jumps back onto the origin-centred lattice. */
                if (E->last_tmv[0] || E->last_tmv[1]) {
                    static const int hx[16][2] = {
                        {0,0},
                        {1,1},{-1,1},{1,-1},{-1,-1},
                        {2,1},{1,2},{-2,1},{-1,2},
                        {2,-1},{1,-2},{-2,-1},{-1,-2},
                        {2,2},{-2,2},{2,-2}
                    };
                    int bx = E->last_tmv[0], by = E->last_tmv[1];
                    for (int i = 0; i < 16 && nc < N_MVC + 63; i++) {
                        cmv[nc][0] = bx + hx[i][0];
                        cmv[nc][1] = by + hx[i][1];
                        nc++;
                    }
                }
                /* dedupe: gmv/prev_copy/mv_ax/dict overlap constantly */
                {   int w2 = 0;
                    for (int i = 0; i < nc; i++) {
                        int dup = 0;
                        for (int j = 0; j < w2; j++)
                            if (cmv[j][0] == cmv[i][0] &&
                                cmv[j][1] == cmv[i][1]) { dup = 1; break; }
                        if (!dup) { cmv[w2][0] = cmv[i][0];
                                    cmv[w2][1] = cmv[i][1]; w2++; }
                    }
                    nc = w2;
                }
                int sad[N_MVC + 64];
                const int16_t *ct2 = E->curr + (size_t)ty * E->w + tx;
                /* top-4 SADs feed the sim stage: the extra slot is the
                   off-axis handshake the 32 new lattice vectors add,
                   without evicting a better large-MV from the old top-3. */
                int b3[4] = { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF };
                int nb3 = 0;
                for (int i = 0; i < nc; i++) {
                    int dx = cmv[i][0], dy = cmv[i][1];
                    int hang = 0;
                    if (!mv_contained(E, tx, ty, tw, th, dx, dy)) {
                        /* A full-frame rect that still overlaps is legal:
                           the decoder already clamps MVRUN to the edge.
                           Anything else, including every stripe hang,
                           stays rejected. */
                        if (!mv_full_hang(E, tx, ty, tw, th, dx, dy)) {
                            sad[i] = 0x7FFFFFFF;
                            continue;
                        }
                        hang = 1;
                    }
                    int s;
                    if (hang) {
                        s = tile_sad_clamp(E->curr, E->prev, E->w, E->h,
                                           tx, ty, tw, th, dx, dy, b3[3]);
                    } else {
                        /* even-row screen first (a strict lower bound);
                           odd rows get summed only while the partial
                           still fits inside the running top-4 bound */
                        const int16_t *rp = E->prev
                                + (size_t)(ty + dy) * E->w + tx + dx;
                        s = tile_sad_par(ct2, rp, E->w, tw, th,
                                         0, b3[3]);
                        if (s <= b3[3])
                            s += tile_sad_par(ct2, rp, E->w, tw, th,
                                              1, b3[3] - s);
                    }
                    sad[i] = s;
                    if (s <= b3[3]) {
                        int k = nb3 < 4 ? nb3++ : 3;
                        while (k > 0 && b3[k - 1] > s) {
                            b3[k] = b3[k - 1]; k--;
                        }
                        b3[k] = s;
                    }
                }
                copy_sad0 = b3[0];
                if (copy_sad0 < 0x7FFFFFFF) {
                    for (int i = 0; i < nc; i++)
                        if (sad[i] == copy_sad0) {
                            sad_bx = cmv[i][0]; sad_by = cmv[i][1];
                            break;
                        }
                }
                for (int t = 0; t < 4; t++) {
                    int bi = -1;
                    for (int i = 0; i < nc; i++)
                        if (sad[i] < 0x7FFFFFFF && (bi < 0 || sad[i] < sad[bi]))
                            bi = i;
                    if (bi < 0) break;
                    int dx = cmv[bi][0], dy = cmv[bi][1];
                    sad[bi] = 0x7FFFFFFF;
                    int is_p = (dx == E->last_tmv[0] &&
                                dy == E->last_tmv[1]);
                    int hang = mv_full_hang(E, tx, ty, tw, th, dx, dy);
                    /* bounded skip: recon is the source verbatim, so a
                       match within the leaf's own error bound is free -
                       at qs=1 the bound is zero (exact match) */
                    int md = hang
                        ? tile_maxd_clamp(E->curr, E->prev, E->w, E->h,
                                          tx, ty, tw, th, dx, dy)
                        : tile_maxd(E->curr + (size_t)ty * E->w + tx,
                                    E->prev + (size_t)(ty + dy) * E->w + tx + dx,
                                    E->w, tw, th);
                    if (md <= (E->qs - 1) / 2) {
                        int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                    is_p ? OP_MVRUNP : OP_MVRUN)
                              + (is_p ? 0 :
                                 mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dx - mvpred(E, OP_MVRUN, 0, depth, tx, tw)))
                               + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dy - mvpred(E, OP_MVRUN, 1, depth, tx, tw))));
                        if (c < bcost) {
                            bcost = c; bop = is_p ? OP_MVRUNP : OP_MVRUN;
                            bdx = dx; bdy = dy; bpred = 0;
                        }
                        continue;
                    }
                    pundo_t *pu = &E->pu;
                    size_t nt = 0;
                    pu_begin(pu, Sio);
                    sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                                  dx, dy, hang ? 2 : 1, Sio, wcur, &nt,
                                  E->W, E->luma, E->recon, E->qs, E->cm, 0, NULL, pu,
                                  bcost - mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                      is_p ? OP_COPYP : OP_COPY)
                                      - (is_p ? 0 :
                                         mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                             zigzag_enc(dx - mvpred(E, OP_COPY, 0, depth, tx, tw)))
                                       + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                             zigzag_enc(dy - mvpred(E, OP_COPY, 1, depth, tx, tw)))), &rcv, pxp);
                    int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                is_p ? OP_COPYP : OP_COPY)
                          + (is_p ? 0 :
                             mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dx - mvpred(E, OP_COPY, 0, depth, tx, tw)))
                           + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dy - mvpred(E, OP_COPY, 1, depth, tx, tw))))
                          + rcv;
                    if (c < bcost) {
                        bcost = c; bop = is_p ? OP_COPYP : OP_COPY;
                        bdx = dx; bdy = dy;
                        { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                        if (E->recon)
                            tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                        bnr = nt; BS_SAVE(pu); bpred = 1;
                    }
                    pu_rollback(pu, Sio);
                }

                /* local refinement: unit-step diamond around the winning
                   MV, gated by SAD before the expensive full simulation. */
                int keep_ref = (bop == OP_COPY || bop == OP_COPYP) ? 1 : 0;
                while (keep_ref) {
                    int bx = bdx, by = bdy, improved = 0;
                    int s0;
                    if (mv_full_hang(E, tx, ty, tw, th, bx, by))
                        s0 = tile_sad_clamp(E->curr, E->prev, E->w, E->h,
                                            tx, ty, tw, th, bx, by, 0x7FFFFFFF);
                    else
                        s0 = tile_sad(E->curr + (size_t)ty * E->w + tx,
                                      E->prev + (size_t)(ty + by) * E->w + tx + bx,
                                      E->w, tw, th, 0x7FFFFFFF);
                    for (int ry = -1; ry <= 1; ry++) {
                    for (int rx = -1; rx <= 1; rx++) {
                        int dx, dy, hang = 0;
                        if (!rx && !ry) continue;
                        dx = bx + rx; dy = by + ry;
                        const int16_t *np = NULL;
                        if (mv_contained(E, tx, ty, tw, th, dx, dy))
                            np = E->prev + (size_t)(ty + dy) * E->w + tx + dx;
                        else if (mv_full_hang(E, tx, ty, tw, th, dx, dy))
                            hang = 1;
                        else
                            continue;
                        int ns;
                        if (hang)
                            ns = tile_sad_clamp(E->curr, E->prev, E->w, E->h,
                                                tx, ty, tw, th, dx, dy, s0 - 1);
                        else {
                            ns = tile_sad_par(E->curr + (size_t)ty * E->w + tx,
                                              np, E->w, tw, th, 0, s0 - 1);
                            if (ns <= s0 - 1)
                                ns += tile_sad_par(E->curr + (size_t)ty * E->w + tx,
                                                   np, E->w, tw, th, 1, s0 - 1 - ns);
                        }
                        if (ns >= s0) continue;
                        int ip = (dx == E->last_tmv[0] &&
                                  dy == E->last_tmv[1]);
                        int md = hang
                            ? tile_maxd_clamp(E->curr, E->prev, E->w, E->h,
                                              tx, ty, tw, th, dx, dy)
                            : tile_maxd(E->curr + (size_t)ty * E->w + tx,
                                        np, E->w, tw, th);
                        if (md <= (E->qs - 1) / 2) {
                            /* bounded match: upgrade to skip mode */
                            int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                        ip ? OP_MVRUNP : OP_MVRUN)
                                  + (ip ? 0 :
                                     mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                                     zigzag_enc(dx - mvpred(E, OP_MVRUN, 0, depth, tx, tw)))
                                   + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                                     zigzag_enc(dy - mvpred(E, OP_MVRUN, 1, depth, tx, tw))));
                            if (c < bcost) {
                                bcost = c; bdx = dx; bdy = dy;
                                bop = ip ? OP_MVRUNP : OP_MVRUN;
                                bpred = 0; improved = 1;
                            }
                            continue;
                        }
                        pundo_t *pu = &E->pu;
                        size_t nt = 0;
                        pu_begin(pu, Sio);
                        sim_pred_tile(E->prev, E->curr, E->w, E->h, tx, ty, tw, th,
                                      dx, dy, hang ? 2 : 1, Sio, wcur, &nt,
                                      E->W, E->luma, E->recon, E->qs, E->cm, 0, NULL, pu,
                                      bcost - mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                          ip ? OP_COPYP : OP_COPY)
                                          - (ip ? 0 :
                                             mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                                 zigzag_enc(dx - mvpred(E, OP_COPY, 0, depth, tx, tw)))
                                           + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                                 zigzag_enc(dy - mvpred(E, OP_COPY, 1, depth, tx, tw)))), &rcv, pxp);
                        int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                    ip ? OP_COPYP : OP_COPY)
                              + (ip ? 0 :
                                 mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dx - mvpred(E, OP_COPY, 0, depth, tx, tw)))
                               + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dy - mvpred(E, OP_COPY, 1, depth, tx, tw))))
                              + rcv;
                        if (c < bcost) {
                            bcost = c; bdx = dx; bdy = dy;
                            bop = ip ? OP_COPYP : OP_COPY;
                            { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                            if (E->recon)
                                tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                            bnr = nt; BS_SAVE(pu); improved = 1;
                        }
                        pu_rollback(pu, Sio);
                    }
                    }
                    if (improved) keep_ref = 1;
                    else break;
                }
            }

            ZT_ACC(2, ztt);
            /* ICOPY: copy from this frame's already-decoded region.
               Causality: every source pixel's tile must precede the
               current tile in raster order - it suffices that the
               bottom-right source pixel's tile does. */
            if ((emit || ZE(zrl0) || zin_pre) && !ZE(znic)) {
                /* two candidate classes: row-causal offsets (dy<0, or
                   dy==0 && dx<0) read only pixels the decoder has
                   already written - legal per-pixel even inside this
                   tile - while other offsets need the whole source rect
                   to precede this tile in raster order.  In-tile reads
                   are simulated against the original, so under
                   quantization (recon != source) they are only safe
                   when the source rect lies fully above the tile. */
                static const int imvc0[][2] = {
                    {0,-1},{0,-2},{0,-3},{0,-4},{0,-8},
                    {-1,0},{-2,0},{-4,0},{-8,0},
                    {-1,-1},{1,-1},{-2,-2},{2,-2},{-4,-4},{4,-4},
                    {-16,0},{-32,0},{-48,0},{-64,0},{-96,0},{-128,0},
                    {0,-16},{0,-32},{0,-48},{0,-64},
                    {-16,-16},{16,-16},{32,-16},{-32,-16},
                    {-16,-32},{16,-32},{0,-48},{48,-16},{-48,-16},
                };
                int cur_ti = (ty / TILE_SZ) * E->ntx + tx / TILE_SZ;
                int imvc[40][2];
                int isad[40];
                int nic = (int)(sizeof(imvc0) / sizeof(imvc0[0]));
                for (int i = 0; i < nic; i++) {
                    imvc[i][0] = imvc0[i][0]; imvc[i][1] = imvc0[i][1];
                }
                /* causal tile dictionary: this exact tile seen earlier in
                   the stripe becomes an IRUN/ICOPY candidate - repeated
                   glyphs anywhere in the coded region collapse to an MV */
                if (0 && E->cdict && tw == TILE_SZ && th == TILE_SZ &&
                    depth == 0) {
                    const tdent_t *e = td_get(E->cdict,
                        td_hash(E->curr + (size_t)ty * E->w + tx, E->w));
                    if (e)
                        for (int q = 0; q < e->n && nic < 40; q++) {
                            imvc[nic][0] = e->px[q] - tx;
                            imvc[nic][1] = e->py[q] - ty;
                            nic++;
                        }
                }
                const int16_t *isrc = E->recon ? E->recon : E->curr;
                const int16_t *ict = E->curr + (size_t)ty * E->w + tx;
                int i2[2] = { 0x7FFFFFFF, 0x7FFFFFFF };
                int ni2 = 0;
                for (int i = 0; i < nic; i++) {
                    int dx0 = imvc[i][0], dy0 = imvc[i][1];
                    int sx = tx + dx0, sy = ty + dy0;
                    isad[i] = 0x7FFFFFFF;
                    if (sx < 0 || sy < 0 || sx + tw > E->w || sy + th > E->h)
                        continue;
                    int causal;
                    if (dy0 < 0 || (dy0 == 0 && dx0 < 0)) {
                        /* decoded-before-this-leaf territory: rows above
                           the tile band, tiles left of us in the band, or
                           any leftward/upward read (dx<=0 lands on cols
                           <= leaf right edge, which DFS decodes first).
                           sy+th<=ty alone is NOT enough: above-leaf rows
                           at rightward cols sit in sibling subtrees the
                           quadtree raster decodes after this leaf. */
                        int tiy0 = ty & ~(TILE_SZ - 1);
                        int tix0 = tx & ~(TILE_SZ - 1);
                        causal = dx0 <= 0 || sy + th <= tiy0 ||
                                 sx + tw <= tix0 ||
                                 (!E->recon && tx + tw >= E->w);
                    } else {
                        int sti = ((sy + th - 1) / TILE_SZ) * E->ntx
                                + (sx + tw - 1) / TILE_SZ;
                        causal = sti < cur_ti;
                    }
                    if (!causal) continue;
                    /* lossy: recon mirrors the decoder only where leaves
                       have committed.  A source rect overlapping this
                       leaf reads stale bytes while the decoder's
                       sequential copy feeds back its own fresh writes -
                       misprediction desyncs the stream.  Lossless is
                       safe: SAD==0 already forces the self-consistent
                       fixed point that the copy reproduces. */
                    if (E->recon && sx < tx + tw && sx + tw > tx &&
                        sy < ty + th && sy + th > ty)
                        continue;
                    const int16_t *ip = isrc + (size_t)sy * E->w + sx;
                    int s = tile_sad_par(ict, ip, E->w, tw, th,
                                         0, i2[1]);
                    if (s <= i2[1])
                        s += tile_sad_par(ict, ip, E->w, tw, th,
                                          1, i2[1] - s);
                    isad[i] = s;
                    if (s <= i2[1]) {
                        int k = ni2 < 2 ? ni2++ : 1;
                        if (k > 0 && i2[0] > s) { i2[1] = i2[0]; k = 0; }
                        i2[k] = s;
                    }
                }
                for (int t = 0; t < 2; t++) {
                    int bi = -1;
                    for (int i = 0; i < nic; i++)
                        if (isad[i] < 0x7FFFFFFF && (bi < 0 || isad[i] < isad[bi]))
                            bi = i;
                    if (bi < 0) break;
                    int dx = imvc[bi][0], dy = imvc[bi][1];
                    isad[bi] = 0x7FFFFFFF;
                    int is_p = (dx == E->last_imv[0] &&
                                dy == E->last_imv[1]);
                    if (tile_maxd(E->curr + (size_t)ty * E->w + tx,
                                  (E->recon ? E->recon : E->curr)
                                    + (size_t)(ty + dy) * E->w + tx + dx,
                                  E->w, tw, th) <= (E->qs - 1) / 2) {
                        int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                    is_p ? OP_IRUNP : OP_IRUN)
                              + (is_p ? 0 :
                                 mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dx - mvpred(E, OP_IRUN, 0, depth, tx, tw)))
                               + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dy - mvpred(E, OP_IRUN, 1, depth, tx, tw))));
                        if (c < bcost) {
                            bcost = c; bop = is_p ? OP_IRUNP : OP_IRUN;
                            bdx = dx; bdy = dy; bpred = 0;
                        }
                        continue;
                    }
                    pundo_t *pu = &E->pu;
                    size_t nt = 0;
                    pu_begin(pu, Sio);
                    sim_pred_tile(E->recon ? E->recon : E->curr, E->curr,
                                  E->w, E->h, tx, ty, tw, th,
                                  dx, dy, 1, Sio, wcur, &nt,
                                  E->W, E->luma, E->recon, E->qs, E->cm, 0, NULL, pu,
                                  bcost - mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                      OP_ICOPY)
                                      - mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                          zigzag_enc(dx - mvpred(E, OP_ICOPY, 0, depth, tx, tw)))
                                      - mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                          zigzag_enc(dy - mvpred(E, OP_ICOPY, 1, depth, tx, tw))), &rcv, pxp);
                    int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_ICOPY)
                          + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dx - mvpred(E, OP_ICOPY, 0, depth, tx, tw)))
                          + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(dy - mvpred(E, OP_ICOPY, 1, depth, tx, tw)))
                          + rcv;
                    if (c < bcost) {
                        bcost = c; bop = OP_ICOPY; bdx = dx; bdy = dy;
                        { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                        if (E->recon)
                            tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                        bnr = nt; BS_SAVE(pu); bpred = 1;
                    }
                    pu_rollback(pu, Sio);
                }
            }

            /* SPLIT: four half-size children, each choosing freely.
               One level only - deeper recursion rarely pays its op bits. */
            /* only worth 4 extra evals when no cheap op fits (mixed
               content); wins observed only above ~0.35 bit/px, gate at 0.5 */
            if ((emit || ZE(zrl0)) &&
                (bop == OP_COPY || bop == OP_COPYP)) {
                int sad0 = -1;
                if (mv_contained(E, tx, ty, tw, th, bdx, bdy))
                    sad0 = tile_sad(E->curr + (size_t)ty * E->w + tx,
                                    E->prev + (size_t)(ty + bdy) * E->w + tx + bdx,
                                    E->w, tw, th, 0x7FFFFFFF);
                else if (mv_full_hang(E, tx, ty, tw, th, bdx, bdy))
                    sad0 = tile_sad_clamp(E->curr, E->prev, E->w, E->h,
                                          tx, ty, tw, th, bdx, bdy, 0x7FFFFFFF);
                if (sad0 == 0) {
                int ip = (bdx == E->last_tmv[0] && bdy == E->last_tmv[1]);
                int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                            ip ? OP_MVRUNP : OP_MVRUN)
                      + (ip ? 0 :
                         mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(bdx - mvpred(E, OP_MVRUN, 0, depth, tx, tw)))
                       + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI], zigzag_enc(bdy - mvpred(E, OP_MVRUN, 1, depth, tx, tw))));
                if (c < bcost) {
                    bcost = c; bop = ip ? OP_MVRUNP : OP_MVRUN; bpred = 0;
                }
                }
            }

            /* flag-free residual-copy variants: rerun the winning MV
               with the per-pixel flag machinery suppressed. */
            if ((emit || ZE(zrl0)) &&
                (bop == OP_COPY || bop == OP_ICOPY)) {
                int isic = (bop == OP_ICOPY);
                const int16_t *ref = isic ? (E->recon ? E->recon : E->curr)
                                          : E->prev;
                if (isic || E->has_prev) {
                    int hp = (!isic && mv_full_hang(E, tx, ty, tw, th,
                                                    bdx, bdy)) ? 2 : 1;
                    pundo_t *pu = &E->pu; size_t nt = 0;
                    pu_begin(pu, Sio);
                    sim_pred_tile(ref, E->curr, E->w, E->h, tx, ty, tw, th,
                                  bdx, bdy, hp, Sio, wcur, &nt,
                                  E->W, E->luma, E->recon, E->qs, E->cm,
                                  1, NULL, pu,
                                  bcost - mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                      (uint32_t)(isic ? OP_ICOPYNF : OP_COPYNF))
                                      - leaf_pcost(E, Sio,
                                          isic ? OP_ICOPYNF : OP_COPYNF,
                                          bdx, bdy, 0, NULL, 0, depth, tx, tw), &rcv, pxp);
                    int nfc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                  (uint32_t)(isic ? OP_ICOPYNF : OP_COPYNF))
                            + rcv
                            + leaf_pcost(E, Sio, isic ? OP_ICOPYNF : OP_COPYNF,
                                         bdx, bdy, 0, NULL, 0, depth, tx, tw);
                    if (nfc < bcost) {
                        bcost = nfc; bop = isic ? OP_ICOPYNF : OP_COPYNF;
                        { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                        if (E->recon)
                            tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                        BS_SAVE(pu); bnr = nt; bpred = 1;
                    }
                    pu_rollback(pu, Sio);
                }
            }

            /* OP_TX / OP_TXI: reversible Haar of a predicted residual.
               Luma only (chroma keeps CFL_MDL for CfL).  Square 4/8/16.
               Skip exact-copy / palette winners. */
            if ((emit || ZE(zrl0)) && !E->luma &&
                tw == th && (tw == 4 || tw == 8 || tw == 16) &&
                bpred &&
                bop != OP_MVRUN && bop != OP_MVRUNP &&
                bop != OP_IRUN && bop != OP_IRUNP &&
                bop != OP_PREDZ && bop != OP_NNZ && bop != OP_RUN &&
                bop != OP_PAL && bop != OP_PALR && bop != OP_PALT &&
                bop != OP_PALQ && bop != OP_PALZ && bop != OP_PALU &&
                bop != OP_PALC && bop != OP_PALD &&
                bop != OP_ROWD && bop != OP_ROWQ && bop != OP_ROWS &&
                bop != OP_RAW && bop != OP_NOISE &&
                bcost > tw * th * 64 &&
                bcost < (int64_t)tw * th * 1024) {
                int cands[4][2], nc = 0;
                cands[nc][0] = 0; cands[nc][1] = 0; nc++;
                if (E->has_prev) {
                    if (E->last_tmv[0] || E->last_tmv[1]) {
                        cands[nc][0] = E->last_tmv[0];
                        cands[nc][1] = E->last_tmv[1]; nc++;
                    }
                    if (copy_sad0 < 0x7FFFFFFF &&
                        (sad_bx || sad_by) &&
                        (sad_bx != E->last_tmv[0] ||
                         sad_by != E->last_tmv[1])) {
                        cands[nc][0] = sad_bx;
                        cands[nc][1] = sad_by; nc++;
                    }
                    if ((bop == OP_COPY || bop == OP_COPYP ||
                         bop == OP_COPYNF) && nc < 4 &&
                        (bdx != cands[0][0] || bdy != cands[0][1]) &&
                        (bdx != E->last_tmv[0] || bdy != E->last_tmv[1])) {
                        cands[nc][0] = bdx; cands[nc][1] = bdy; nc++;
                    }
                }
                for (int ci = 0; ci < nc; ci++) {
                    int dx = cands[ci][0], dy = cands[ci][1];
                    pundo_t *pu = &E->pu;
                    size_t nt = 0;
                    pu_begin(pu, Sio);
                    if (!sim_tx_tile(E, tx, ty, tw, th, dx, dy, E->qs,
                                     wcur, &nt, -1)) {
                        pu_rollback(pu, Sio);
                        continue;
                    }
                    int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                            OP_TX)
                          + leaf_pcost(E, Sio, OP_TX, dx, dy, 0, NULL, 0,
                                       depth, tx, tw)
                          + recs_cost_q8(wcur, nt, E->cm);
                    if (c < bcost) {
                        bcost = c; bop = OP_TX; bdx = dx; bdy = dy;
                        { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                        if (E->recon)
                            tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                        bnr = nt; BS_SAVE(pu); bpred = 1;
                    }
                    pu_rollback(pu, Sio);
                }
                /* OP_TXI: Haar of (curr - DC of left/top border). */
                {
                    pundo_t *pu = &E->pu;
                    size_t nt = 0;
                    pu_begin(pu, Sio);
                    if (sim_tx_tile(E, tx, ty, tw, th, 0, 0, E->qs,
                                    wcur, &nt, 1)) {
                        int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                                OP_TXI)
                              + recs_cost_q8(wcur, nt, E->cm);
                        if (c < bcost) {
                            bcost = c; bop = OP_TXI; bdx = 0; bdy = 0;
                            { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                            if (E->recon)
                                tile_cp(brecon, E->recon, E->w,
                                        tx, ty, tw, th);
                            bnr = nt; BS_SAVE(pu); bpred = 1;
                        }
                    }
                    pu_rollback(pu, Sio);
                }
            }

            cheap_done:;

            /* vertical/temporal repeat: if the rect directly above or
               the previous frame's same slot stored this exact leaf,
               replaying it costs one op record - fold the saving into
               bcost so the SPLIT decision is honest */
            if (bop != OP_SPLIT) {
                int vc = -1;
                vop_t *ve0 = &E->vops[vcell_idx(E->ntx, tx, tw)];
                if (ve0->rw == tw && ve0->rx == tx &&
                    ve0->ry + ve0->rw == ty &&
                    vop_match(ve0, bop, bdx, bdy, bpk, bpal, bidx, npix, bsig))
                    vc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_VRPT);
                if (depth == 0 && E->ptab) {
                    vop_t *pe0 = &E->ptab[E->cur_ti];
                    if (pe0->rw == tw && pe0->rx == tx &&
                        pe0->op != OP_SPLIT &&
                        vop_match(pe0, bop, bdx, bdy, bpk, bpal, bidx, npix, bsig)) {
                        int vp = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PSAME);
                        if (vc < 0 || vp < vc) vc = vp;
                    }
                }
                if (vc >= 0) {
                    int nc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], (uint32_t)bop)
                           + leaf_pcost(E, Sio, bop, bdx, bdy, bpk, bpal, bsig, depth, tx, tw);
                    if (vc < nc) bcost += vc - nc;
                }
            }

            ZT_ACC(3, ztt);
            /* deeper probes cost more (children encode twice under the
               notxn fallback) and win less often - scale the gate by
               depth: ZSPG sets the depth-0 threshold, +ZDSTEP per level */
            /* 128 q8 = 0.5 bit/px, the gate the probe was tuned for.
               400 q8 (1.56 bpp) left mixed screen tiles unsplit: on the
               fixed UI scoreboard ZSPG=128 beat 400 by hundreds of bytes
               and beat both 200 and 64. */
            int spgd = (int)(ZEP(zspg) ? atol(ZEP(zspg)) : 128)
                     + depth * (ZE(zdstep) ? ZE(zdstep) : 0);
            /* leaf-op prognosis: winners that code (near-)exact block
               transforms never get outbid by four leaves - skip the
               probe outright (measured ~0% win on real content) */
            int probe_ok = bop != OP_PREDZ && bop != OP_NNZ &&
                           bop != OP_IRUNP && bop != OP_IRUN &&
                           bop != OP_MVRUN && bop != OP_MVRUNP &&
                           bop != OP_COPYNF &&
                           bop != OP_PALD && bop != OP_PAL &&
                           bop != OP_CFL && bop != OP_TX &&
                           bop != OP_TXI;
            /* NNF/PREDNF at >=2 bpp: children inherit the same dense
               residual and never recover the split op.  Face-grain
               burned ~135k losing probes here; live camera at ~1 bpp
               still probes (and does win splits). */
            if ((bop == OP_NNF || bop == OP_PREDNF || bop == OP_PREDNFX) &&
                bcost > tw * th * 512)
                probe_ok = 0;
            /* solid tile: four corners + centre match, split cannot win */
            if (probe_ok && tw >= 8 && th >= 8) {
                int16_t c0 = E->curr[(size_t)ty * E->w + tx];
                if (E->curr[(size_t)ty * E->w + tx + tw - 1] == c0 &&
                    E->curr[(size_t)(ty + th - 1) * E->w + tx] == c0 &&
                    E->curr[(size_t)(ty + th - 1) * E->w + tx + tw - 1] == c0 &&
                    E->curr[(size_t)(ty + th / 2) * E->w + tx + tw / 2] == c0)
                    probe_ok = 0;
            }
            int qds_v = -1;
            if (ZE(zpoi) && depth <= 2 && tw >= 8 && th >= 8) {
                int qm[4] = {0,0,0,0};
                for (int yy = 0; yy < th; yy += 2)
                    for (int xx = 0; xx < tw; xx += 2)
                        qm[(xx >= tw/2) + 2*(yy >= th/2)] +=
                            E->curr[(size_t)(ty + yy) * E->w + tx + xx];
                int a0 = (tw/2)*(th/2)/4, mm[4] = {qm[0]/a0, qm[1]/a0,
                                                 qm[2]/a0, qm[3]/a0};
                int lo = 1<<30, hi = -(1<<30);
                for (int i = 0; i < 4; i++) {
                    if (mm[i] < lo) lo = mm[i];
                    if (mm[i] > hi) hi = mm[i];
                }
                qds_v = hi - lo;
            }
            int pd = depth < 4 ? depth : 3;
            int poi = -1;
            if (depth == 0 && emit && E->ptab) {
                vop_t *pe_ = &E->ptab[E->cur_ti];
                poi = (pe_->rw == tw && pe_->rx == tx)
                    ? (pe_->op & 31) : 33;
                /* pixel-identical to prev frame at this tile? */
                if (poi != 33 && E->prev) {
                    int same = 1;
                    for (int yy = ty; yy < ty + th && same; yy++)
                        if (memcmp(E->curr + (size_t)yy * E->w + tx,
                                   E->prev + (size_t)yy * E->w + tx,
                                   (size_t)tw * sizeof(int16_t)))
                            same = 0;
                    if (!same) poi += 64;      /* changed pixels */
                    else       poi += 96;      /* identical pixels */
                }
            }
            /* prev-PREDNF leaf: dense texture where a split wins ~17%
               of the time but still burns full probes - scale the
               threshold so only blowout wins pay for a probe
               (ZAP24 restores the plain threshold) */
            int spg_eff = spgd;
            if (poi == 24 && !ZE(zap24)) spg_eff *= 3;
            /* NNF (learned, no flags) at camera density is a chronic
               split loser; PREDNF still wins ~17% on live footage so
               it keeps the plain threshold. */
            if (bop == OP_NNF && !ZE(zap24))
                spg_eff *= 3;
            if (probe_ok || ZE(znopflt)) {
            int dlim = ZE(zdmax) >= 0 ? ZE(zdmax) : 1;
            /* photo/camera 8x8 in the mid-bpp band: one more split to
               4x4.  Grain (NNF >=2 bpp) is already probe_ok=0; flat
               8x8 stays at depth 1. */
            if (dlim < 2 && depth == 1 && tw >= 8 && th >= 8 &&
                bcost > tw * th * 192 &&
                bcost < (int64_t)tw * th * 512)
                dlim = 2;
            int want = depth < dlim &&
                tw >= 8 && th >= 8 &&
                (emit || ZE(zedp)) &&
                (ztxn_n == 0 || !ZE(znxp)) &&
                bcost > tw * th * (spg_eff + (ZE(znopb) ? 0
                                : E->pbias[pd]));
            /* saturated leaf: at ~ZHIB/2 bits per pixel the residual
               stream is already near raw - children inherit the same
               density and the split op overhead can never be recovered
               (measured: ~3% probe wins beyond 4.5bpp).  The skip still
               feeds pbias like a loss, else suppressing chronic losers
               starves the adaptive threshold and lets mid-density
               probes multiply. */
            if (want && ZE_.zhib &&
                bcost > (int64_t)tw * th * ZE_.zhib * 128) {
                want = 0;
                if (!ZE(znopb) && E->pbias[pd] < 96) E->pbias[pd] += 1;
            }
            if (want) {
                int sc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_SPLIT);
                if (emit && !ZE(zntx) && ztxn_n < (ZE(zntxl) ? 2 : 1)) {
                    /* two-level probe: the emit=0 (reduced-ladder) probe
                       inflates subtree cost slightly, so any split that
                       loses even under that inflated estimate cannot win
                       the real probe either - veto it for ~1/3 the price
                       of a full transactional probe */
                    if (ZE(zpre) >= 0) {
                        pstate_t *Scp = &E->sc_stk[
                            depth < ENC_MAXD ? depth : ENC_MAXD - 1];
                        *Scp = *Sio;
                        int sc0 = sc;
                        struct cap_t *cap_sv = E->cap_out;
                        int pb_sv[4], qs_sv = E->qs,
                            lop_sv = E->last_op, arun_sv = E->all_run,
                            nnc_sv = E->nn_cnt, pcopy_sv = E->prev_copy,
                            pdx_sv = E->prev_dx, pdy_sv = E->prev_dy,
                            tmv_sv0 = E->last_tmv[0],
                            tmv_sv1 = E->last_tmv[1],
                            imv_sv0 = E->last_imv[0],
                            imv_sv1 = E->last_imv[1],
                            qds_sv = E->qd_spread,
                            cti_sv = E->cur_ti;
                        vop_t psl_sv = E->pre_slot;
                        memcpy(pb_sv, E->pbias, sizeof pb_sv);
                        E->cap_out = NULL;
                        zin_pre++;
                        for (int i = 0; i < 4; i++) {
                            E->pcap = ZE(znopc) ? INT32_MAX : bcost - sc0;
                            sc0 += enc_one_tile(
                                E, tx + (i & 1) * (tw / 2),
                                ty + (i >> 1) * (th / 2),
                                (i & 1) ? tw - tw / 2 : tw / 2,
                                (i >> 1) ? th - th / 2 : th / 2,
                                depth + 1, Scp, 0);
                            if (sc0 >= bcost) break;
                        }
                        zin_pre--;
                        E->pcap = INT32_MAX;
                        E->cap_out = cap_sv;
                        memcpy(E->pbias, pb_sv, sizeof pb_sv);
                        E->qs = qs_sv; E->last_op = lop_sv;
                        E->all_run = arun_sv; E->nn_cnt = nnc_sv;
                        E->prev_copy = pcopy_sv; E->prev_dx = pdx_sv;
                        E->prev_dy = pdy_sv;
                        E->last_tmv[0] = tmv_sv0; E->last_tmv[1] = tmv_sv1;
                        E->last_imv[0] = imv_sv0; E->last_imv[1] = imv_sv1;
                        E->pre_slot = psl_sv; E->qd_spread = qds_sv;
                        E->cur_ti = cti_sv;
                        int zpre = ZE(zpre) >= 0 ? ZE(zpre) : 25;
                        if (ZE(zprelog))
                            fprintf(stderr, "PRE d%d sc0=%d bc=%d\n",
                                    depth, sc0, bcost);
                        if ((int64_t)sc0 * 100 >=
                            (int64_t)bcost * (100 + zpre)) {
                            if (!ZE(znopb) && E->pbias[pd] < 96)
                                E->pbias[pd] += 1;
                            goto probe_done;
                        }
                    }
                    long txi = ++txn_cnt;
                    if (ZE(ztxk) >= 0 && txi > ZE(ztxk)) goto notxn;
                    if (ZE(ztxb) >= 0 && E->cur_ti > ZE(ztxb)) goto notxn;
                    if (ZE(ztxw))
                        fprintf(stderr, "TXP %x #%ld @%d,%d ti=%d\n",
                                (unsigned)((uintptr_t)E & 0xffff),
                                txi, tx, ty, E->cur_ti);
                    /* emit the subtree for real inside a transaction:
                       the probe cost is then the exact emitted cost and
                       a winning split has no second evaluation pass */
                    txn_begin(E, Sio, tx, ty, tw, th, &txn,
                              ztxn_n > 0 || ZE(zlite0));
                    if (ZE(zd2)) fprintf(stderr,
                        "E %x ti=%d op=%d bop=%d lo=%d up=%d d%d @%d,%d %dx%d\n",
                        (unsigned)((uintptr_t)E & 0xffff),
                        E->cur_ti, OP_SPLIT, OP_SPLIT, E->last_op,
                        (depth == 0 && !ZE(znoup)) ? E->upstat[tx >> 4] : 0,
                        depth, tx, ty, tw, th);
                    if (ZE(zop)) zopc[OP_SPLIT]++;
                    E->precs[E->npr++] = ((uint32_t)oc << 16) | OP_SPLIT;
                    mdl_bump(&E->pmc[oc], &E->pmc[PM_HI], oc, OP_SPLIT);
                    if (depth == 0)
                        E->upstat[tx >> 4] = 1;
                    if (E->ptab && depth == 0) {
                        vdu_log(&E->ptab[E->cur_ti]);
                        E->ptab[E->cur_ti].op = OP_SPLIT;
                        E->ptab[E->cur_ti].rw = 0;
                    }
                    for (int i = 0; i < 4; i++) {
                        /* once the spent probe budget leaves under ~32b
                           of headroom no child op can fit - the emit=1
                           child bails at the pcap checkpoints instead
                           of running its full ladder for a dead probe */
                        E->pcap = ZE(znopc) ? INT32_MAX : bcost - sc;
                        sc += enc_one_tile(E, tx + (i & 1) * (tw / 2),
                                           ty + (i >> 1) * (th / 2),
                                           (i & 1) ? tw - tw / 2 : tw / 2,
                                           (i >> 1) ? th - th / 2 : th / 2,
                                           depth + 1, Sio, emit);
                        E->pcap = INT32_MAX;
                        if (sc >= bcost) break;   /* cannot win - quit early */
                    }
                    if (ZE(ztxf)) sc = 0x7FFFFFFF;   /* force-lose debug */
                    /* ZTXC: compare probe-emitted records against a redo;
                       ZTXCT restricts the compare to one tile index */
                    int zcmp = ZE(ztxc)
                            || (ZE(ztxct) >= 0 && E->cur_ti == ZE(ztxct));
                    if (zcmp && sc < bcost) {
                        /* archive the emitted subtree records */
                        size_t np = E->npr - txn.npr, nr = E->nrr - txn.nrr;
                        if (!cp_p) { cp_p = malloc(1<<20); cp_r = malloc(1<<20); }
                        if ((np + 1) * 4 <= (1u<<20) && (nr + 1) * 4 <= (1u<<20)) {
                            memcpy(cp_p, E->precs + txn.npr, np * 4);
                            memcpy(cp_r, E->rrecs + txn.nrr, nr * 4);
                            cp_np = np; cp_nr = nr; cp_ok = 1;
                        } else cp_ok = 0;
                        txn_rollback(E, Sio, &txn, tx, ty, tw, th);
                        Sio = Sreal;
                        /* pretend the probe won; commit re-emits the
                           subtree normally and we diff the records */
                        bcost = sc; bop = OP_SPLIT; bpred = 0;
                    } else if (sc < bcost) {
                        bcost = sc; bop = OP_SPLIT; bpred = 0;
                        txn_open = 1;
                        if (!ZE(znopb)) {
                            E->pbias[pd] -= 8;
                            if (E->pbias[pd] < 0) E->pbias[pd] = 0;
                        }
                        if (!txn.lite) {
                        Sio = &txn.S;   /* alts still price the leaf
                                           against the pre-split state */
                        /* alternative evals below reuse E->recon as
                           scratch and would clobber the subtree's
                           emitted recon - keep a clean copy */
                        if (txn.has_rc)
                            for (int y = 0; y < th; y++)
                                memcpy(txn.rc2 + y * tw,
                                       E->recon + (size_t)(ty + y) * E->w + tx,
                                       (size_t)tw * sizeof(int16_t));
                        /* the emitted children already advanced every
                           encoder scalar; the alternatives below must
                           instead observe the PRE-probe state, since
                           that is the only state the decoder can reach
                           if the subtree loses.  Stash the post values
                           and swap the snapshot in. */
                        {
                            int col = tx >> 4;
                            txn.pqs = E->qs;         E->qs = txn.qs;
                            txn.plast_op = E->last_op;
                                                     E->last_op = txn.last_op;
                            txn.pall_run = E->all_run;
                                                     E->all_run = txn.all_run;
                            txn.pnn_cnt = E->nn_cnt; E->nn_cnt = txn.nn_cnt;
                            txn.pprev_copy = E->prev_copy;
                                                     E->prev_copy = txn.prev_copy;
                            txn.pprev_dx = E->prev_dx;
                                                     E->prev_dx = txn.prev_dx;
                            txn.pprev_dy = E->prev_dy;
                                                     E->prev_dy = txn.prev_dy;
                            txn.plast_tmv[0] = E->last_tmv[0];
                            txn.plast_tmv[1] = E->last_tmv[1];
                            E->last_tmv[0] = txn.last_tmv[0];
                            E->last_tmv[1] = txn.last_tmv[1];
                            txn.plast_imv[0] = E->last_imv[0];
                            txn.plast_imv[1] = E->last_imv[1];
                            E->last_imv[0] = txn.last_imv[0];
                            E->last_imv[1] = txn.last_imv[1];
                            txn.pup = E->upstat[col];
                                                     E->upstat[col] = txn.up;
                            txn.pmx = E->mv_ax[col]; E->mv_ax[col] = txn.mx;
                            txn.pmy = E->mv_ay[col]; E->mv_ay[col] = txn.my;
                            txn.ppre_slot = E->pre_slot;
                                                     E->pre_slot = txn.pre_slot;
                            if (E->vops) {
                                int vi = vcell_idx(E->ntx, tx, tw);
                                txn.pvc0 = E->vops[vi];
                                E->vops[vi] = txn.vc0;
                            }
                            if (depth == 0 && E->ptab) {
                                txn.pptab_v = 1;
                                txn.pptab = E->ptab[E->cur_ti];
                                E->ptab[E->cur_ti] = txn.pre_slot;
                            }
                            if (txn.has_rc)
                                for (int y = 0; y < th; y++)
                                    memcpy(E->recon + (size_t)(ty + y) * E->w + tx,
                                           txn.rc + y * tw,
                                           (size_t)tw * sizeof(int16_t));
                        }
                        } else
                            zpu2 = NULL;   /* lite win: keep the emitted
                                              subtree state as-is */
                        if (qds_v >= 0)
                            atomic_fetch_add(&pcw[120 + (qds_v > 300 ? 19 :
                                             qds_v * 20 / 301)], 1);
                        if (poi >= 0)
                            atomic_fetch_add(&pcw[poi], 1);
                        if (depth == 0)
                            atomic_fetch_add(&pcw[140 +
                                (bcost > npix * 128 * 19 ? 19
                                 : bcost / (npix * 128))], 1);
                        if (ZE(zprelog))
                            fprintf(stderr, "TXN d%d sc1=%d bc=%d W\n",
                                    depth, sc, bcost);
                        if (ZE(zt)) { extern _Atomic long ztxw; atomic_fetch_add(&ztxw, 1); }
                    } else {
                        txn_rollback(E, Sio, &txn, tx, ty, tw, th);
                        if (ZE(zprelog))
                            fprintf(stderr, "TXN d%d sc1=%d bc=%d\n",
                                    depth, sc, bcost);
                        if (qds_v >= 0)
                            atomic_fetch_add(&pcl[120 + (qds_v > 300 ? 19 :
                                             qds_v * 20 / 301)], 1);
                        if (poi >= 0)
                            atomic_fetch_add(&pcl[poi], 1);
                        if (depth == 0)
                            atomic_fetch_add(&pcl[140 +
                                (bcost > npix * 128 * 19 ? 19
                                 : bcost / (npix * 128))], 1);
                        if (!ZE(znopb) && E->pbias[pd] < 96) E->pbias[pd] += 1;
                        if (ZE(zt)) { extern _Atomic long ztxl; atomic_fetch_add(&ztxl, 1); }
                    }
                } else {
                    notxn:;
                    if (ZE(zd2)) {
                        int qm[4] = {0, 0, 0, 0};
                        for (int y = 0; y < th; y++)
                            for (int x = 0; x < tw; x++)
                                qm[(x >= tw/2) + 2 * (y >= th/2)] +=
                                    E->curr[(size_t)(ty + y) * E->w + tx + x];
                        int a0 = (tw / 2) * (th / 2);
                        int a1 = (tw - tw / 2) * (th / 2);
                        int a2 = (tw / 2) * (th - th / 2);
                        int a3 = (tw - tw / 2) * (th - th / 2);
                        int lo = 1 << 30, hi = -(1 << 30);
                        int m0 = qm[0] / a0, m1 = qm[1] / a1,
                            m2 = qm[2] / a2, m3 = qm[3] / a3;
                        int mm[4] = {m0, m1, m2, m3};
                        for (int i = 0; i < 4; i++) {
                            if (mm[i] < lo) lo = mm[i];
                            if (mm[i] > hi) hi = mm[i];
                        }
                        E->qd_spread = hi - lo;
                    }
                    pstate_t *Scp = &E->sc_stk[depth < ENC_MAXD ? depth
                                               : ENC_MAXD - 1];
                    *Scp = *Sio;
                    /* probe children now advance the encoder scalars a
                       real emit would (sibling fidelity).  Snapshot the
                       pre-probe values and restore below - on a win the
                       real emit=1 commit replays them, on a loss the
                       leaf's own commit needs the originals */
                    int sv_qs = E->qs, sv_lop = E->last_op,
                        sv_ar = E->all_run, sv_nn = E->nn_cnt,
                        sv_pc = E->prev_copy, sv_pdx = E->prev_dx,
                        sv_pdy = E->prev_dy,
                        sv_tm0 = E->last_tmv[0], sv_tm1 = E->last_tmv[1],
                        sv_im0 = E->last_imv[0], sv_im1 = E->last_imv[1];
                    int c0 = tx >> 4, c1 = (tx + tw - 1) >> 4;
                    int sv_mx[2] = {E->mv_ax[c0], E->mv_ax[c1]},
                        sv_my[2] = {E->mv_ay[c0], E->mv_ay[c1]};
                    for (int i = 0; i < 4; i++) {
                        cap_t *cpk = &E->cap_stk[dslot * 4 + i];
                        cpk->valid = 0;
                        E->cap_out = cpk;
                        /* if this child alone cannot stay under the
                           remaining split budget the probe is already
                           dead - it may early-out with any overage */
                        E->pcap = ZE(znopc) ? INT32_MAX : bcost - sc;
                        sc += enc_one_tile(E, tx + (i & 1) * (tw / 2),
                                           ty + (i >> 1) * (th / 2),
                                           (i & 1) ? tw - tw / 2 : tw / 2,
                                           (i >> 1) ? th - th / 2 : th / 2,
                                           depth + 1, Scp, 0);
                        E->pcap = INT32_MAX;
                        E->cap_out = NULL;
                        capn = i + 1;

                        if (sc >= bcost) break;
                    }
                    E->qs = sv_qs; E->last_op = sv_lop;
                    E->all_run = sv_ar; E->nn_cnt = sv_nn;
                    E->prev_copy = sv_pc; E->prev_dx = sv_pdx;
                    E->prev_dy = sv_pdy;
                    E->last_tmv[0] = sv_tm0; E->last_tmv[1] = sv_tm1;
                    E->last_imv[0] = sv_im0; E->last_imv[1] = sv_im1;
                    E->mv_ax[c0] = sv_mx[0]; E->mv_ax[c1] = sv_mx[1];
                    E->mv_ay[c0] = sv_my[0]; E->mv_ay[c1] = sv_my[1];
                    if (ZE(zd2))
                        fprintf(stderr, "QPROBE e=%d lop=%d win=%d sc=%d bc=%d @%d,%d %dx%d\n",
                                emit, bop, sc < bcost, sc, bcost, tx, ty, tw, th);
                    if (qds_v >= 0)
                        atomic_fetch_add(&(sc < bcost ? pcw : pcl)
                            [120 + (qds_v > 300 ? 19 : qds_v * 20 / 301)], 1);
                    if (sc < bcost) { bcost = sc; bop = OP_SPLIT; bpred = 0;
                        if (!ZE(znopb)) {
                            E->pbias[pd] -= 8;
                            if (E->pbias[pd] < 0) E->pbias[pd] = 0;
                        }
                    } else if (!ZE(znopb) && E->pbias[pd] < 96)
                        E->pbias[pd] += 1;
                }
                }
                probe_done:;
            }
            ZT_ACC(4, ztt);

            /* ---- alternative representations, picked by
               bits + LAMBDA*distortion ---- */
            static const int LAM = 64;  /* Q8 bits per unit of |err| */
            if (depth == 0 && E->recon) {
                long dcur = 0;
                if (bpred)
                    for (int y = 0; y < th; y++)
                        for (int x = 0; x < tw; x++) {
                            int d = brecon[y * tw + x]
                                  - E->curr[(size_t)(ty + y) * E->w + tx + x];
                            dcur += d < 0 ? -d : d;
                        }
                long tot = (long)bcost + LAM * dcur;

                /* OP_NOISE: prediction + synthesized residual. Qualify
                   when the PRED residual stream is white (unpredictable =
                   noise by definition); sig_0 tiles degenerate to a
                   residual-free PRED, which is exact and almost free. */
                if (E->nseed) {
                    pstate_t St0 = *Sio; size_t n0 = 0;
                    sim_pred_tile(E->prev, E->curr, E->w, E->h,
                                  tx, ty, tw, th, 0, 0, E->has_prev,
                                  &St0, wcur, &n0, E->W, E->luma,
                                  NULL, 1, E->cm, 0, NULL, NULL, INT_MAX, NULL, pxp);
                    int64_t s2 = 0, lag = 0; int pr = 0; int cnt = 0;
                    int rmax = 0;
                    for (size_t i = 0; i < n0; i++) {
                        int midx = (int)(wcur[i] >> 16);
                        if (midx >= NRES_MDL) continue; /* flag/run recs */
                        int r = zigzag_dec(wcur[i] & 0xFFFFu);
                        int ar = r < 0 ? -r : r;
                        if (ar > rmax) rmax = ar;
                        s2 += (int64_t)r * r;
                        if (cnt) lag += (int64_t)pr * r;
                        pr = r; cnt++;
                    }
                    int sig = 0;
                    if (cnt) {
                        int64_t v = s2 / cnt;
                        while ((int64_t)(sig + 1) * (sig + 1) <= v) sig++;
                    }
                    int64_t rho100 = (cnt && s2) ?
                        lag * 100 / s2 : 0;
                    /* tail check: structured outliers (text edges under
                       noise) keep the bounded ops.  Among legal sigmas,
                       prefer values already stored by replay-reachable
                       descriptors - adopting the above-cell or prev-frame
                       sig turns the leaf into a one-symbol VRPT/PSAME. */
                    if (sig <= 255 && cnt >= npix / 4 &&
                        rho100 > -35 && rho100 < 35) {
                        int cs[3]; int ncs = 0;
                        cs[ncs++] = sig;
                        const vop_t *veN =
                            &E->vops[vcell_idx(E->ntx, tx, tw)];
                        int vok = veN->op == OP_NOISE &&
                                  veN->rw == tw && veN->rx == tx &&
                                  veN->ry + veN->rw == ty;
                        const vop_t *peN = (depth == 0 && E->ptab)
                            ? &E->ptab[E->cur_ti] : NULL;
                        int pok = peN && peN->op == OP_NOISE &&
                                  peN->rw == tw && peN->rx == tx &&
                                  peN->op != OP_SPLIT;
                        if (vok && veN->sig != sig)
                            cs[ncs++] = veN->sig;
                        if (pok && peN->sig != sig &&
                            !(vok && peN->sig == veN->sig))
                            cs[ncs++] = peN->sig;
                        for (int ci = 0; ci < ncs; ci++) {
                            int s = cs[ci];
                            if (rmax > 4 * s + 4) continue;
                            pstate_t St = *Sio;
                            sim_noise_tile(E, tx, ty, tw, th, &St, s,
                                           E->nseed +
                                           (uint32_t)(ty * 7919 + tx));
                            long dn = 0; int emax = 0;
                            for (int y = 0; y < th; y++)
                                for (int x = 0; x < tw; x++) {
                                    int d = E->recon[(size_t)(ty + y) * E->w
                                                     + tx + x]
                                          - E->curr[(size_t)(ty + y) * E->w
                                                    + tx + x];
                                    int ad = d < 0 ? -d : d;
                                    dn += ad;
                                    if (ad > emax) emax = ad;
                                }
                            if (emax > E->qs_hi) continue;
                            long cn;
                            if (vok && s == veN->sig)
                                cn = mlensym_cost_q8(&E->pmc[oc],
                                       &E->pmc[PM_HI], OP_VRPT);
                            else if (pok && s == peN->sig)
                                cn = mlensym_cost_q8(&E->pmc[oc],
                                       &E->pmc[PM_HI], OP_PSAME);
                            else
                                cn = mlensym_cost_q8(&E->pmc[oc],
                                       &E->pmc[PM_HI], OP_NOISE)
                                   + mlensym_cost_q8(&E->pmc[PM_QT],
                                       &E->pmc[PM_HI], (uint32_t)s);
                            if (cn + LAM * dn < tot) {
                                tot = cn + LAM * dn;
                                bop = OP_NOISE; bsig = s;
                                tile_cp(brecon, E->recon, E->w,
                                        tx, ty, tw, th);
                                *bSp = St; bnd = -1; bnr = 0; bpred = 0;
                            }
                        }

                    }
                }
            }
            /* ---- per-tile quantizer choice (OP_QS directive) ----
               A residual-coded winner is re-simulated at qs_hi (never
               qs=1: that made high-eps files larger than mid-eps because
               LAM * distortion preferred lossless tiles). Screen still
               has the frame-level lossless sibling in v3sq. */
            if (depth == 0 && E->recon && bpred) {
                long dcur = 0;
                for (int y = 0; y < th; y++)
                    for (int x = 0; x < tw; x++) {
                        int d = brecon[y * tw + x]
                              - E->curr[(size_t)(ty + y) * E->w + tx + x];
                        dcur += d < 0 ? -d : d;
                    }
                long tot = (long)bcost + LAM * dcur;
                int alts[1] = { E->qs_hi };
                for (int ai = 0; ai < 2; ai++) {
                    int q = alts[ai];
                    if (q == E->qs || q < 1 || q > 255) continue;
                    pstate_t St; size_t nt = 0;
                    if (!resim_op(E, bop, bdx, bdy, tx, ty, tw, th,
                                  Sio, &St, wcur, &nt, q))
                        continue;
                    long cbits = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_QS)
                               + mlensym_cost_q8(&E->pmc[PM_QT], &E->pmc[PM_HI],
                                                 (uint32_t)(q - 1))
                               + mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], (uint32_t)bop)
                               + (bop == OP_COPY || bop == OP_ICOPY ||
                                  bop == OP_MVRUN || bop == OP_IRUN ||
                                  bop == OP_COPYNF || bop == OP_ICOPYNF ||
                                  (bop == OP_TX && E->has_prev)
                                  ? mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                                    zigzag_enc(bdx - mvpred(E, bop, 0, depth, tx, tw)))
                                    + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                                      zigzag_enc(bdy - mvpred(E, bop, 1, depth, tx, tw)))
                                  : 0)
                               + recs_cost_q8(wcur, nt, E->cm);
                    long da = 0;
                    for (int y = 0; y < th; y++)
                        for (int x = 0; x < tw; x++) {
                            int d = E->recon[(size_t)(ty + y) * E->w + tx + x]
                                  - E->curr[(size_t)(ty + y) * E->w + tx + x];
                            da += d < 0 ? -d : d;
                        }
                    if (cbits + LAM * da < tot) {
                        tot = cbits + LAM * da;
                        { uint32_t *wt_ = wcur; wcur = brecs; brecs = wt_; }
                        tile_cp(brecon, E->recon, E->w, tx, ty, tw, th);
                        bnr = nt; *bSp = St; bnd = -1; bq = q;
                        bcost = (int)cbits;
                    }
                }
            }

            /* ---- MV rescue sweep -----------------------------------
               The heuristic probes are axis-aligned or coarse diagonals;
               a rigid drag at an odd offset (e.g. (-2,+1)) falls between
               them and the tile then pays full residual coding for
               content that exists verbatim in the previous frame.  When
               the winning leaf still buys residual bits, sweep a window
               for a source position that fits the live error bound.
               Hits re-enter the normal MVRUN/MVRUNP cost path, so the
               bitstream and decoder are untouched - encoder cost only. */
            /* the probe children run with emit=1 but must observe the
               same machinery as the old emit=0 probe: skip the rescue
               sweep inside transactions (only the txn-owning tile's
               own alternatives run it) */
            /* the probe children run with emit=1 but must observe the
               same machinery as the old emit=0 probe: skip the rescue
               sweep inside transactions (only the txn-owning tile's
               own alternatives run it) */
            if (emit && (!ztxn_n || txn_open) && E->has_prev && !ZE(znrs) &&
                bcost > ZE(zsm) && tw >= 4 && th >= 4 &&
                copy_sad0 < npix * 4 &&
                bop != OP_MVRUN && bop != OP_MVRUNP &&
                bop != OP_IRUN && bop != OP_IRUNP &&
                bop != OP_PREDZ && bop != OP_NNZ) {
                const int lim = (E->qs - 1) >> 1;
                const int RW = ZE(zrw);
                int mp0 = mvpred(E, OP_MVRUN, 0, depth, tx, tw);
                int mp1 = mvpred(E, OP_MVRUN, 1, depth, tx, tw);
                const int16_t *ct = E->curr + (size_t)ty * E->w + tx;
                /* probe pre-filter: wrong positions die on the first
                   samples, so the average position costs ~8 reads */
                const int po[8][2] = {
                    {0, 0}, {tw - 1, 0}, {0, th - 1}, {tw - 1, th - 1},
                    {tw / 2, th / 2}, {tw / 4, (3 * th) / 4},
                    {(3 * tw) / 4, th / 4}, {(3 * tw) / 4, (3 * th) / 4}};
                int cv[8];
                for (int k = 0; k < 8; k++)
                    cv[k] = ct[(size_t)po[k][1] * E->w + po[k][0]];
                /* candidate window bounds, hoisted out of the rings:
                   cx in [cxlo,cxhi], cy in [cylo,cyhi] */
                const int cxlo = -tx, cxhi = E->w - tw - tx;
                const int cylo = -E->yoff - ty,
                          cyhi = E->hfull - E->yoff - th - ty;
                int hdx[16], hdy[16], nh = 0;
                for (int r = 0; r <= RW && !nh; r++) {
                    for (int dy = -r; dy <= r; dy++) {
                        int cy = mp1 + dy;
                        if (cy < cylo || cy > cyhi) continue;
                        const int16_t *prow0 = E->prev
                            + (size_t)(ty + cy) * E->w + tx;
                        if (dy == -r || dy == r) {
                            /* full top/bottom ring row: contiguous cx */
                            int lo = mp0 - r, hi = mp0 + r;
                            if (lo < cxlo) lo = cxlo;
                            if (hi > cxhi) hi = cxhi;
                            int cx = lo;
#if defined(__ARM_NEON)
                            int16x8_t vlim = vdupq_n_s16((int16_t)lim);
                            for (; cx + 7 <= hi; cx += 8) {
                                uint16x8_t okm = (uint16x8_t)vdupq_n_s16(-1);
                                for (int k = 0; k < 8; k++) {
                                    int16x8_t vc = vdupq_n_s16((int16_t)cv[k]);
                                    int16x8_t vp = vld1q_s16(prow0 + cx
                                        + po[k][1] * E->w + po[k][0]);
                                    int16x8_t ad = vabdq_s16(vc, vp);
                                    okm = vandq_u16(okm,
                                        vcleq_s16(ad, vlim));
                                }
                                if (vmaxvq_u16(okm))
                                for (int lane = 0; lane < 8; lane++)
                                    if (okm[lane]) {
                                        const int16_t *pp = prow0 + cx + lane;
                                        if (tile_maxd(ct, pp, E->w, tw, th) <= lim) {
                                            hdx[nh] = cx + lane;
                                            hdy[nh] = cy;
                                            if (++nh == 16) break;
                                        }
                                    }
                                if (nh == 16) break;
                            }
#endif
                            for (; cx <= hi; cx++) {
                                const int16_t *pp = prow0 + cx;
                                int ok = 1;
                                for (int k = 0; k < 8; k++) {
                                    int dd = cv[k]
                                        - pp[(size_t)po[k][1] * E->w + po[k][0]];
                                    if (dd < -lim || dd > lim) { ok = 0; break; }
                                }
                                if (ok && tile_maxd(ct, pp, E->w, tw, th) <= lim) {
                                    hdx[nh] = cx; hdy[nh] = cy;
                                    if (++nh == 16) break;
                                }
                            }
                        } else {
                            /* ring sides: only cx = mp0 +/- r */
                            for (int s = 0; s < 2; s++) {
                                int cx = s ? mp0 + r : mp0 - r;
                                if (cx < cxlo || cx > cxhi) continue;
                                const int16_t *pp = prow0 + cx;
                                int ok = 1;
                                for (int k = 0; k < 8; k++) {
                                    int dd = cv[k]
                                        - pp[(size_t)po[k][1] * E->w + po[k][0]];
                                    if (dd < -lim || dd > lim) { ok = 0; break; }
                                }
                                if (ok && tile_maxd(ct, pp, E->w, tw, th) <= lim) {
                                    hdx[nh] = cx; hdy[nh] = cy;
                                    if (++nh == 16) break;
                                }
                            }
                        }
                        if (nh == 16) break;
                    }
                }
                int wbest = -1;
                for (int i = 0; i < nh; i++) {
                    int ip = (hdx[i] == E->last_tmv[0] &&
                              hdy[i] == E->last_tmv[1]);
                    int c = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                ip ? OP_MVRUNP : OP_MVRUN)
                          + (ip ? 0 :
                             mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                 zigzag_enc(hdx[i] - mp0))
                           + mlensym_cost_q8(&E->pmc[PM_MV], &E->pmc[PM_HI],
                                 zigzag_enc(hdy[i] - mp1)));
                    if (c < bcost) { bcost = c; wbest = i; }
                }
                if (wbest >= 0) {
                    int ip = (hdx[wbest] == E->last_tmv[0] &&
                              hdy[wbest] == E->last_tmv[1]);
                    bop = ip ? OP_MVRUNP : OP_MVRUN;
                    bdx = hdx[wbest]; bdy = hdy[wbest];
                    bnr = 0; bpred = 0; bq = E->qs;
                }
            }

            alt_done:;
            if (!emit && bop != OP_SPLIT && E->vops) {
                /* commit-time replay upgrades also fire for replayed
                   bundles: vops cells are untouched by probe siblings
                   (keyed per child x-range), so pricing the leaf at the
                   upgraded emitted cost here is exact */
                const vop_t *ve =
                    &E->vops[vcell_idx(E->ntx, tx, tw)];
                if (ve->rw == tw && ve->rx == tx &&
                    ve->ry + ve->rw == ty &&
                    vop_match(ve, bop, bdx, bdy, bpk, bpal, bidx,
                              npix, bsig)) {
                    int pal_op = bop == OP_PAL || bop == OP_PALR ||
                                 bop == OP_PALT || bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC ||
                                 bop == OP_PALD;
                    int vc;
                    if (pal_op && !memcmp(ve->imap, bidx,
                                          (size_t)npix))
                        vc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                             OP_PALX);
                    else {
                        int nc = mlensym_cost_q8(&E->pmc[oc],
                                    &E->pmc[PM_HI], (uint32_t)bop)
                               + leaf_pcost(E, Sio, bop, bdx, bdy, bpk,
                                            bpal, bsig, depth, tx, tw);
                        vc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                             OP_VRPT);
                        if (vc >= nc) vc = bcost;  /* commit takes plain op */
                    }
                    bcost = vc;
                }
            }
            if (!emit && E->cap_out && bop != OP_SPLIT) {
                cap_t *cpk = E->cap_out;
                cpk->op = bop; cpk->dx = bdx; cpk->dy = bdy;
                cpk->pk = bpk; cpk->bq = bq; cpk->sig = bsig;
                cpk->pred = bpred; cpk->cost = bcost; cpk->nrec = bnr;
                /* palette tables and the recon snapshot only matter for
                   the ops that consume them - skip the dead copies */
                int pal_op = bop == OP_PAL || bop == OP_PALR ||
                             bop == OP_PALX || bop == OP_PALD ||
                             bop == OP_PALT || bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC;
                if (pal_op) {
                    memcpy(cpk->th, bth, sizeof bth);
                    memcpy(cpk->pal, bpal, sizeof bpal);
                    memcpy(cpk->idx, bidx, sizeof bidx);
                }
                if (E->recon)
                    memcpy(cpk->rc, brecon, sizeof brecon);
                memcpy(cpk->recs, brecs, bnr * sizeof(uint32_t));
                cpk->bnd = bnd; cpk->ar = bAr; cpk->nr = bNr;
                if (bnd >= 0) {
                    memcpy(cpk->u.dl.dc, bdc,
                           (size_t)bnd * sizeof(uint16_t));
                    memcpy(cpk->u.dl.dr, bdr,
                           (size_t)bnd * sizeof(*bdr));
                } else {
                    cpk->u.bs = *bSp;
                }
                cpk->valid = 1;
            }
            if (!emit && bop != OP_SPLIT) {
                /* probe-sibling fidelity: leave the same scalar
                   post-state a real leaf commit would (records/models/
                   vops stay untouched - models are frozen during probes
                   and vops cells are keyed per child x-range, so
                   siblings never observe them) */
                if (bop == OP_COPY || bop == OP_MVRUN ||
                    bop == OP_COPYP || bop == OP_MVRUNP ||
                    bop == OP_COPYNF || bop == OP_TX) {
                    E->mv_ax[tx / TILE_SZ] = bdx;
                    E->mv_ay[tx / TILE_SZ] = bdy;
                    E->prev_dx = bdx; E->prev_dy = bdy; E->prev_copy = 1;
                    E->last_tmv[0] = bdx; E->last_tmv[1] = bdy;
                } else {
                    E->mv_ax[tx / TILE_SZ] = 0x7FFFFFFF;
                    E->prev_copy = 0;
                    if (bop == OP_ICOPY || bop == OP_IRUN ||
                        bop == OP_IRUNP || bop == OP_ICOPYNF) {
                        E->last_imv[0] = bdx; E->last_imv[1] = bdy;
                    }
                }
                if (bop == OP_NN || bop == OP_NNF || bop == OP_NNZ)
                    E->nn_cnt++;
                E->last_op = bop < N_OPOPS ? bop : 0;
            }
            if (txn_open) {
                if (bop != OP_SPLIT) {
                    /* an alternative leaf beat the emitted subtree:
                       unwind every child side effect, then fall through
                       to the normal leaf commit */
                    if (ZE(zd2)) fprintf(stderr, "TXN-LOSE @%d,%d\n", tx, ty);
                    Sio = Sreal;
                    txn_rollback(E, Sio, &txn, tx, ty, tw, th);
                    if (ZE(zt)) atomic_fetch_add(&ztxo, 1);
                } else {
                    /* children already emitted everything; the commit
                       path's only remaining work is the post-split
                       bookkeeping */
                    if (ZE(zd2)) fprintf(stderr, "TXN-WIN @%d,%d\n", tx, ty);
                    Sio = Sreal;
                    ztxn_n--;
                    if (!ztxn_n) { mdu_n = 0; vdu_n = 0; }
                    if (txn.lite && ztxn_n) {
                        /* nested win: nothing to restore or replay -
                           the outer txn's win path replays the whole
                           record range, scalars already hold post
                           values, recon is already the subtree's */
                        E->mv_ax[tx / TILE_SZ] = 0x7FFFFFFF;
                        E->prev_copy = 0;
                        return bcost;
                    }
                    if (txn.lite) zpu2 = NULL;   /* outer lite win:
                        subtree state stays - disarm the row logger */
                    for (size_t i = txn.npr; i < E->npr; i++) {
                        uint32_t r = E->precs[i];
                        mdl_bump_raw(&E->pmc[r >> 16], &E->pmc[PM_HI],
                                     r >> 16, r & 0xFFFFu);
                    }
                    for (size_t i = txn.nrr; i < E->nrr; i++) {
                        uint32_t r = E->rrecs[i];
                        mdl_bump_raw(&E->cm[r >> 16], &E->cm[HI_MDL_IDX],
                                     r >> 16, r & 0xFFFFu);
                    }
                    {
                        int col = tx >> 4;
                        E->qs = txn.pqs;     E->last_op = txn.plast_op;
                        E->all_run = txn.pall_run;
                        E->nn_cnt = txn.pnn_cnt;
                        E->prev_copy = txn.pprev_copy;
                        E->prev_dx = txn.pprev_dx;
                        E->prev_dy = txn.pprev_dy;
                        E->last_tmv[0] = txn.plast_tmv[0];
                        E->last_tmv[1] = txn.plast_tmv[1];
                        E->last_imv[0] = txn.plast_imv[0];
                        E->last_imv[1] = txn.plast_imv[1];
                        E->upstat[col] = txn.pup;
                        E->mv_ax[col] = txn.pmx;
                        E->mv_ay[col] = txn.pmy;
                        E->pre_slot = txn.ppre_slot;
                        if (E->vops)
                            E->vops[vcell_idx(E->ntx, tx, tw)] = txn.pvc0;
                        if (txn.pptab_v)
                            E->ptab[E->cur_ti] = txn.pptab;
                    }
                    if (txn.has_rc)
                        for (int y = 0; y < th; y++)
                            memcpy(E->recon + (size_t)(ty + y) * E->w + tx,
                                   txn.rc2 + y * tw,
                                   (size_t)tw * sizeof(int16_t));
                    E->mv_ax[tx / TILE_SZ] = 0x7FFFFFFF;
                    E->prev_copy = 0;
                    return bcost;
                }
            }
            cap_commit:;
            /* ---- commit winner ---- */
            if (bq != E->qs) {
                if (emit) {
                    if (ZE(zd2)) fprintf(stderr, "E %x ti=%d op=%d up=%d\n",
                                               (unsigned)((uintptr_t)E & 0xffff),
                                               E->cur_ti, OP_QS,
                                               (depth == 0 && !ZE(znoup)) ? E->upstat[tx >> 4] : 0);
                    E->precs[E->npr++] = ((uint32_t)oc << 16) | OP_QS;
                    mdl_bump(&E->pmc[oc], &E->pmc[PM_HI], oc, OP_QS);
                    E->precs[E->npr++] = ((uint32_t)PM_QT << 16)
                                       | (uint32_t)(bq - 1);
                    mdl_bump(&E->pmc[PM_QT], &E->pmc[PM_HI], PM_QT, (uint32_t)(bq - 1));
                }
                E->qs = bq;    /* stream state evolves in both modes */
            }
            int vrpt = 0, psame = 0, palx = 0, psamx = 0;
            vop_t *ve = NULL;
            if (emit && bop != OP_SPLIT) {
                ve = &E->vops[vcell_idx(E->ntx, tx, tw)];
                int nc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], (uint32_t)bop)
                       + leaf_pcost(E, Sio, bop, bdx, bdy, bpk, bpal, bsig, depth, tx, tw);
                if (ve->rw == tw && ve->rx == tx &&
                    ve->ry + ve->rw == ty &&
                    vop_match(ve, bop, bdx, bdy, bpk, bpal, bidx, npix, bsig)) {
                    if ((bop == OP_PAL || bop == OP_PALR ||
                         bop == OP_PALT || bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC ||
                         bop == OP_PALD) &&
                        !memcmp(ve->imap, bidx, (size_t)npix)) {
                        /* descriptor-exact replay: the whole leaf costs
                           one op record - nothing streams */
                        palx = 1;
                    } else {
                        int vc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_VRPT);
                        vrpt = vc < nc;
                    }
                }
                if (!vrpt && !palx && depth == 0 && E->ptab &&
                    !ZE(znops)) {
                    vop_t *pe = &E->ptab[E->cur_ti];
                    if (pe->rw == tw && pe->rx == tx &&
                        pe->op != OP_SPLIT) {
                        if (ZE(zps)) {
                            static int stt[4] = {0,0,0,0};
                            stt[pe->op == bop ?
                                (vop_match(pe, bop, bdx, bdy, bpk, bpal, bidx, npix, bsig) ? 0 : 1)
                                : (pe->op == OP_SPLIT ? 3 : 2)]++;
                            if ((stt[0]+stt[1]+stt[2]+stt[3]) % 2000 == 0)
                                fprintf(stderr, "PSSTAT full=%d oponly=%d opdiff=%d\n",
                                        stt[0], stt[1], stt[2]);
                        }
                        if (vop_match(pe, bop, bdx, bdy, bpk, bpal, bidx, npix, bsig)) {
                            if ((bop == OP_PAL || bop == OP_PALR ||
                                 bop == OP_PALT || bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC ||
                                 bop == OP_PALD) &&
                                !memcmp(pe->imap, bidx, (size_t)npix))
                                psamx = 1;
                            else {
                                int vc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_PSAME);
                                psame = vc < nc;
                            }
                        }
                    }
                }
            }
            if (emit) {
                int eop = palx ? OP_PALX : psamx ? OP_PSAMX :
                          psame ? OP_PSAME : vrpt ? OP_VRPT : bop;
                if (ZE(zd2)) fprintf(stderr, "E %x ti=%d op=%d bop=%d lo=%d up=%d d%d @%d,%d %dx%d\n",
                                           (unsigned)((uintptr_t)E & 0xffff),
                                           E->cur_ti, eop, bop, E->last_op,
                                           (depth == 0 && !ZE(znoup)) ? E->upstat[tx >> 4] : 0,
                                           depth, tx, ty, tw, th);
                if (ZE(zop)) {
                    zopc[eop < 43 ? eop : 42]++;
                    if (depth == 0)
                        fprintf(stderr, "TC %d,%d %dx%d eop=%d bop=%d c=%d\n",
                                tx, ty, tw, th, eop, bop, bcost);
                }
                E->precs[E->npr++] = ((uint32_t)oc << 16) | (uint32_t)eop;
                mdl_bump(&E->pmc[oc], &E->pmc[PM_HI], oc, (uint32_t)eop);
                if (depth == 0)
                    E->upstat[tx >> 4] = (bop == OP_SPLIT);
                if (bop != OP_SPLIT)
                    E->last_op = bop < N_OPOPS ? bop : 0;
            }
            /* program memory: top-level slots remember this frame's
               leaf (or a SPLIT marker) for next frame's OP_PSAME */
            if (emit && E->ptab && depth == 0) {
                vop_t *pe = &E->ptab[E->cur_ti];
                vdu_log(pe);
                if (bop == OP_SPLIT) { pe->op = OP_SPLIT; pe->rw = 0; }
                else {
                    pe->rx = (int16_t)tx; pe->ry = (int16_t)ty;
                    pe->rw = (int16_t)tw; pe->op = bop;
                    pe->dx = (int16_t)bdx; pe->dy = (int16_t)bdy;
                    pe->npal = (bop == OP_PAL || bop == OP_PALR ||
                                bop == OP_PALT || bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC ||
                                bop == OP_PALD) ? bpk : 0;
                    if (bop == OP_PAL || bop == OP_PALR || bop == OP_PALT ||
                        bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC || bop == OP_PALD) {
                        memcpy(pe->pal, bpal, (size_t)bpk * sizeof(int16_t));
                        memcpy(pe->imap, bidx, (size_t)npix);
                    }
                    pe->sig = bsig;
                }
            }
            if (bop == OP_SPLIT) {
                size_t cpbase = txn.npr, crbase = txn.nrr;
                int zcmp = (ZE(ztxc) || ZE(ztxct) >= 0) && cp_ok;
                /* prefix replay only: a child's captured delta assumes
                   the probe-time entry state, which holds only if every
                   prior sibling committed the same post-state.  Once a
                   child falls back to a fresh eval the chain is broken -
                   all later children must re-eval too. */
                int rok = 1;
                for (int i = 0; i < 4; i++) {
                    cap_t *cpk = &E->cap_stk[dslot * 4 + i];
                    rok &= (i < capn && cpk->valid);
                    E->cap_rep = (rok && cpk->op >= 0) ? cpk : NULL;
                    if (ZE(zd2)) { E->capn_commit++;
                        if (E->cap_rep) E->capn_rep++;
                        else if (!rok) E->capn_chain++;
                        else if (cpk->op < 0) E->capn_trans++; }
                    enc_one_tile(E, tx + (i & 1) * (tw / 2),
                                 ty + (i >> 1) * (th / 2),
                                 (i & 1) ? tw - tw / 2 : tw / 2,
                                 (i >> 1) ? th - th / 2 : th / 2,
                                 depth + 1, Sio, emit);

                }
                E->cap_rep = NULL;
                if (zcmp) {
                    size_t np = E->npr - cpbase, nr = E->nrr - crbase;
                    if (np != cp_np || nr != cp_nr)
                        fprintf(stderr, "TXC-COUNT @%d,%d np %zu!=%zu nr %zu!=%zu\n",
                                tx, ty, np, cp_np, nr, cp_nr);
                    else {
                        for (size_t i = 0; i < np; i++)
                            if (E->precs[cpbase + i] != cp_p[i]) {
                                fprintf(stderr, "TXC-P@%d,%d rec+%zu %08x!=%08x\n",
                                        tx, ty, i, E->precs[cpbase+i], cp_p[i]);
                                break;
                            }
                        for (size_t i = 0; i < nr; i++)
                            if (E->rrecs[crbase + i] != cp_r[i]) {
                                fprintf(stderr, "TXC-R@%d,%d rec+%zu %08x!=%08x\n",
                                        tx, ty, i, E->rrecs[crbase+i], cp_r[i]);
                                break;
                            }
                    }
                    cp_ok = 0;
                }
                if (emit) {
                    E->mv_ax[tx / TILE_SZ] = 0x7FFFFFFF;
                    E->prev_copy = 0;
                }
                return bcost;
            }
            if (ZE(zr) && bpred)
                fprintf(stderr, "E ti=%d bop=%d @%d,%d %dx%d nrr=%zu bnr=%zu\n",
                        E->cur_ti, bop, tx, ty, tw, th, E->nrr, bnr);
            /* materialize decoder-view pixels: winning sim already wrote
               them for pred ops (snapshot saved them from later evals);
               lossless ops reproduce the original exactly */
            if (E->recon) {
                if (bpred || bop == OP_NOISE)
                    tile_cp_back(E->recon, E->w, brecon, tx, ty, tw, th);
                else if (bop == OP_MVRUN || bop == OP_MVRUNP) {
                    /* bounded skip: decoder view is the source rect */
                    for (int y = ty; y < ty + th; y++) {
                        int sy = y + bdy;
                        if (sy < 0) sy = 0; else if (sy >= E->h) sy = E->h - 1;
                        for (int x = tx; x < tx + tw; x++) {
                            int sx = x + bdx;
                            if (sx < 0) sx = 0;
                            else if (sx >= E->w) sx = E->w - 1;
                            E->recon[(size_t)y * E->w + x] =
                                E->prev[(size_t)sy * E->w + sx];
                        }
                    }
                } else if (bop == OP_IRUN || bop == OP_IRUNP) {
                    /* forward self-copy from already-decoded pixels,
                       same order the decoder uses */
                    for (int y = ty; y < ty + th; y++) {
                        int sy = y + bdy;
                        if (sy < 0) sy = 0; else if (sy >= E->h) sy = E->h - 1;
                        for (int x = tx; x < tx + tw; x++) {
                            int sx = x + bdx;
                            if (sx < 0) sx = 0;
                            else if (sx >= E->w) sx = E->w - 1;
                            E->recon[(size_t)y * E->w + x] =
                                E->recon[(size_t)sy * E->w + sx];
                        }
                    }
                } else
                    for (int y = ty; y < ty + th; y++)
                        memcpy(E->recon + (size_t)y * E->w + tx,
                               E->curr + (size_t)y * E->w + tx,
                               (size_t)tw * sizeof(int16_t));
            }
            if (emit && ZE(zpix)) {
                const int16_t *vv = E->recon ? E->recon : E->curr;
                uint32_t hsh = 5381;
                for (int y = ty; y < ty + th; y++)
                    for (int x = tx; x < tx + tw; x++)
                        hsh = hsh * 33 + (uint16_t)vv[(size_t)y * E->w + x];
                fprintf(stderr, "EPX %x @%d,%d %dx%d op=%d h=%08x n=%zu\n",
                        (unsigned)((uintptr_t)E & 0xffff),
                        tx, ty, tw, th, bop, hsh, bnr);
            }
            if (emit) {
                if (!vrpt && !psame && (bop == OP_COPY || bop == OP_ICOPY ||
                    bop == OP_MVRUN || bop == OP_IRUN ||
                    bop == OP_COPYNF || bop == OP_ICOPYNF ||
                    (bop == OP_TX && E->has_prev))) {
                    int px = mvpred(E, bop, 0, depth, tx, tw), py = mvpred(E, bop, 1, depth, tx, tw);
                    E->precs[E->npr++] = ((uint32_t)PM_MV << 16) | zigzag_enc(bdx - px);
                    E->precs[E->npr++] = ((uint32_t)PM_MV << 16) | zigzag_enc(bdy - py);
                    mdl_bump(&E->pmc[PM_MV], &E->pmc[PM_HI], PM_MV, zigzag_enc(bdx - px));
                    mdl_bump(&E->pmc[PM_MV], &E->pmc[PM_HI], PM_MV, zigzag_enc(bdy - py));
                }
                if (bop == OP_COPY || bop == OP_COPYP ||
                    bop == OP_MVRUN || bop == OP_MVRUNP ||
                    bop == OP_COPYNF || bop == OP_TX) {
                    E->last_tmv[0] = bdx; E->last_tmv[1] = bdy;
                    if (emit && ZE(zmv)) fprintf(stderr,"EMV %x @%d,%d %dx%d op=%d tmv<-%d,%d T%d\n",(unsigned)((uintptr_t)E&0xffff),tx,ty,tw,th,bop,bdx,bdy,ztxn_n);
                } else if (bop == OP_ICOPY || bop == OP_IRUN ||
                           bop == OP_IRUNP || bop == OP_ICOPYNF) {
                    E->last_imv[0] = bdx; E->last_imv[1] = bdy;
                }
                if (bop == OP_CFL && !vrpt && !psame) {
                    E->precs[E->npr++] = ((uint32_t)PM_CFLA << 16)
                                       | zigzag_enc(bdx);
                    E->precs[E->npr++] = ((uint32_t)PM_CFLB << 16)
                                       | zigzag_enc(bdy);
                    mdl_bump(&E->pmc[PM_CFLA], &E->pmc[PM_HI], PM_CFLA,
                             zigzag_enc(bdx));
                    mdl_bump(&E->pmc[PM_CFLB], &E->pmc[PM_HI], PM_CFLB,
                             zigzag_enc(bdy));
                }
                if (bop == OP_NOISE && !vrpt && !psame) {
                    E->precs[E->npr++] = ((uint32_t)PM_QT << 16)
                                       | (uint32_t)bsig;
                    mdl_bump(&E->pmc[PM_QT], &E->pmc[PM_HI], PM_QT, (uint32_t)bsig);
                }
                if (bop == OP_PAL || bop == OP_PALR) {
                    if (!palx && !psamx) {
                    /* params replay (vrpt/psame) skips the palette but the
                       index map still streams; pixel-exact replays
                       (palx/psamx) stream nothing */
                    if (!vrpt && !psame) {
                    E->precs[E->npr++] = ((uint32_t)PM_PALN << 16) | (uint32_t)(bpk - 1);
                    mdl_bump(&E->pmc[PM_PALN], &E->pmc[PM_HI], PM_PALN, (uint32_t)(bpk - 1));
                    for (int i = 0; i < bpk; i++) {
                        uint32_t v = (uint32_t)(bpal[i] + LIT_BIAS);
                        E->precs[E->npr++] = ((uint32_t)PM_PALV << 16) | v;
                        mdl_bump(&E->pmc[PM_PALV], &E->pmc[PM_HI], PM_PALV, v);
                    }
                    }
                    if (bop == OP_PAL) {
                        for (int i = 0; i < npix; i++) {
                            E->precs[E->npr++] = ((uint32_t)PM_PALI << 16) | bidx[i];
                            mdl_bump(&E->pmc[PM_PALI], &E->pmc[PM_HI], PM_PALI, bidx[i]);
                        }
                    } else {
                        for (int i = 0; i < npix; ) {
                            int r = 1;
                            while (i + r < npix && bidx[i + r] == bidx[i]) r++;
                            E->precs[E->npr++] = ((uint32_t)PM_PALI << 16) | bidx[i];
                            mdl_bump(&E->pmc[PM_PALI], &E->pmc[PM_HI], PM_PALI, bidx[i]);
                            E->precs[E->npr++] = ((uint32_t)PM_REPN << 16) | (uint32_t)(r - 1);
                            mdl_bump(&E->pmc[PM_REPN], &E->pmc[PM_HI], PM_REPN, (uint32_t)(r - 1));
                            i += r;
                        }
                    }
                    }
                } else if (bop == OP_PALT) {
                    /* PALT descriptors only replay pixel-exact
                       (PALX/PSAMX): a params replay would need the
                       thresholds, which the descriptor does not carry */
                    if (!palx && !psamx) {
                    E->precs[E->npr++] = ((uint32_t)PM_PALN << 16) | (uint32_t)(bpk - 1);
                    mdl_bump(&E->pmc[PM_PALN], &E->pmc[PM_HI], PM_PALN, (uint32_t)(bpk - 1));
                    for (int i = 0; i < bpk; i++) {
                        uint32_t v = (uint32_t)(bpal[i] + LIT_BIAS);
                        E->precs[E->npr++] = ((uint32_t)PM_PALV << 16) | v;
                        mdl_bump(&E->pmc[PM_PALV], &E->pmc[PM_HI], PM_PALV, v);
                    }
                    for (int i = 0; i + 1 < bpk; i++) {
                        uint32_t v = (uint32_t)zigzag_enc(bth[i]);
                        E->precs[E->npr++] = ((uint32_t)PM_CFLB << 16) | v;
                        mdl_bump(&E->pmc[PM_CFLB], &E->pmc[PM_HI], PM_CFLB, v);
                    }
                    }
                } else if (bop == OP_PALQ) {
                    if (!palx && !psamx) {
                    E->precs[E->npr++] = ((uint32_t)PM_PALN << 16) | (uint32_t)(bpk - 1);
                    mdl_bump(&E->pmc[PM_PALN], &E->pmc[PM_HI], PM_PALN, (uint32_t)(bpk - 1));
                    for (int i = 0; i < bpk; i++) {
                        int slot = -1;
                        for (int s = 0; s < E->palq_n; s++)
                            if (E->palq[s] == bpal[i]) { slot = s; break; }
                        if (slot >= 0) {
                            E->precs[E->npr++] = ((uint32_t)PM_PALQ << 16) | (uint32_t)slot;
                            mdl_bump(&E->pmc[PM_PALQ], &E->pmc[PM_HI],
                                     PM_PALQ, (uint32_t)slot);
                        } else {
                            E->precs[E->npr++] = ((uint32_t)PM_PALQ << 16) | PALQ_MISS;
                            mdl_bump(&E->pmc[PM_PALQ], &E->pmc[PM_HI],
                                     PM_PALQ, PALQ_MISS);
                            uint32_t v = (uint32_t)(bpal[i] + LIT_BIAS);
                            E->precs[E->npr++] = ((uint32_t)PM_PALV << 16) | v;
                            mdl_bump(&E->pmc[PM_PALV], &E->pmc[PM_HI], PM_PALV, v);
                        }
                    }
                    for (int i = 0; i + 1 < bpk; i++) {
                        uint32_t v = (uint32_t)zigzag_enc(bth[i]);
                        E->precs[E->npr++] = ((uint32_t)PM_CFLB << 16) | v;
                        mdl_bump(&E->pmc[PM_CFLB], &E->pmc[PM_HI], PM_CFLB, v);
                    }
                    }
                } else if (bop == OP_PALZ) {
                    if (!palx && !psamx) {
                    E->precs[E->npr++] = ((uint32_t)PM_PALN << 16) | (uint32_t)(bpk - 1);
                    mdl_bump(&E->pmc[PM_PALN], &E->pmc[PM_HI], PM_PALN, (uint32_t)(bpk - 1));
                    for (int i = 0; i < bpk; i++) {
                        uint32_t v = zigzag_enc(bpal[i]);
                        E->precs[E->npr++] = ((uint32_t)PM_PALZ << 16) | v;
                        mdl_bump(&E->pmc[PM_PALZ], &E->pmc[PM_HI], PM_PALZ, v);
                    }
                    int zpred = 0;
                    for (int i = 0; i + 1 < bpk; i++) {
                        uint32_t v = (uint32_t)zigzag_enc(bth[i] - zpred);
                        zpred = bth[i];
                        E->precs[E->npr++] = ((uint32_t)PM_CFLB << 16) | v;
                        mdl_bump(&E->pmc[PM_CFLB], &E->pmc[PM_HI], PM_CFLB, v);
                    }
                    }
                } else if (bop == OP_PALU) {
                    if (!palx && !psamx) {
                    E->precs[E->npr++] = ((uint32_t)PM_PALN << 16) | (uint32_t)(bpk - 1);
                    mdl_bump(&E->pmc[PM_PALN], &E->pmc[PM_HI], PM_PALN, (uint32_t)(bpk - 1));
                    for (int i = 0; i < bpk; i++) {
                        int slot = -1;
                        for (int s = 0; s < E->palq_n; s++)
                            if (E->palq[s] == bpal[i]) { slot = s; break; }
                        if (slot >= 0) {
                            E->precs[E->npr++] = ((uint32_t)PM_PALQ << 16) | (uint32_t)slot;
                            mdl_bump(&E->pmc[PM_PALQ], &E->pmc[PM_HI],
                                     PM_PALQ, (uint32_t)slot);
                        } else {
                            E->precs[E->npr++] = ((uint32_t)PM_PALQ << 16) | PALQ_MISS;
                            mdl_bump(&E->pmc[PM_PALQ], &E->pmc[PM_HI],
                                     PM_PALQ, PALQ_MISS);
                            uint32_t v = (uint32_t)(bpal[i] + LIT_BIAS);
                            E->precs[E->npr++] = ((uint32_t)PM_PALV << 16) | v;
                            mdl_bump(&E->pmc[PM_PALV], &E->pmc[PM_HI], PM_PALV, v);
                        }
                    }
                    int zpred = 0;
                    for (int i = 0; i + 1 < bpk; i++) {
                        uint32_t v = (uint32_t)zigzag_enc(bth[i] - zpred);
                        zpred = bth[i];
                        E->precs[E->npr++] = ((uint32_t)PM_CFLB << 16) | v;
                        mdl_bump(&E->pmc[PM_CFLB], &E->pmc[PM_HI], PM_CFLB, v);
                    }
                    }
                } else if (bop == OP_PALC) {
                    if (!palx && !psamx) {
                    if (!vrpt && !psame) {
                    E->precs[E->npr++] = ((uint32_t)PM_PALN << 16) | (uint32_t)(bpk - 1);
                    mdl_bump(&E->pmc[PM_PALN], &E->pmc[PM_HI], PM_PALN, (uint32_t)(bpk - 1));
                    for (int i = 0; i < bpk; i++) {
                        uint32_t v = (uint32_t)(bpal[i] + LIT_BIAS);
                        E->precs[E->npr++] = ((uint32_t)PM_PALV << 16) | v;
                        mdl_bump(&E->pmc[PM_PALV], &E->pmc[PM_HI], PM_PALV, v);
                    }
                    }
                    for (int i = 0; i < npix; i++) {
                        int pu = i >= tw ? bidx[i - tw] : 0;
                        int pl = (i % tw) ? bidx[i - 1] : pu;
                        int c = bidx[i] == pl ? 0 : bidx[i] == pu ? 1 : 2;
                        E->precs[E->npr++] = ((uint32_t)PM_PALF << 16) | (uint32_t)c;
                        mdl_bump(&E->pmc[PM_PALF], &E->pmc[PM_HI], PM_PALF, (uint32_t)c);
                        if (c == 2) {
                            E->precs[E->npr++] = ((uint32_t)PM_PALI << 16) | bidx[i];
                            mdl_bump(&E->pmc[PM_PALI], &E->pmc[PM_HI], PM_PALI, bidx[i]);
                        }
                    }
                    }
                } else if (bop == OP_PALD) {
                    if (!palx && !psamx) {
                    if (!vrpt && !psame) {
                    E->precs[E->npr++] = ((uint32_t)PM_PALN << 16) | (uint32_t)(bpk - 1);
                    mdl_bump(&E->pmc[PM_PALN], &E->pmc[PM_HI], PM_PALN, (uint32_t)(bpk - 1));
                    for (int i = 0; i < bpk; i++) {
                        uint32_t v = (uint32_t)(bpal[i] + LIT_BIAS);
                        E->precs[E->npr++] = ((uint32_t)PM_PALV << 16) | v;
                        mdl_bump(&E->pmc[PM_PALV], &E->pmc[PM_HI], PM_PALV, v);
                    }
                    }
                    for (int i = 0; i < npix; i++) {
                        int a = (i % tw) ? bidx[i - 1] : 0;
                        int b = (i >= tw) ? bidx[i - tw] : a;
                        int c2 = (i >= tw && (i % tw))
                               ? bidx[i - tw - 1] : a;
                        int p0 = a + b - c2;
                        int pa = p0 > a ? p0 - a : a - p0;
                        int pb = p0 > b ? p0 - b : b - p0;
                        int pc = p0 > c2 ? p0 - c2 : c2 - p0;
                        int pr = pa <= pb && pa <= pc ? a
                               : pb <= pc ? b : c2;
                        uint32_t v = zigzag_enc((int)bidx[i] - pr);
                        E->precs[E->npr++] = ((uint32_t)PM_PALD << 16) | v;
                        mdl_bump(&E->pmc[PM_PALD], &E->pmc[PM_HI],
                                 PM_PALD, v);
                    }
                    }
                } else if (bop == OP_ROWD || bop == OP_ROWQ || bop == OP_ROWS) {
                    rowd_body(E, oc, 1, bop == OP_ROWQ, bop == OP_ROWS,
                              bidx, tw, th, bpk, bpal, 0x7fffffff);
                } else if (bop == OP_RAW) {
                    for (int y = ty; y < ty + th; y++)
                        for (int x = tx; x < tx + tw; x++) {
                            uint32_t v = (uint32_t)(E->curr[(size_t)y * E->w + x] + LIT_BIAS);
                            E->precs[E->npr++] = ((uint32_t)PM_RAW << 16) | v;
                            mdl_bump(&E->pmc[PM_RAW], &E->pmc[PM_HI], PM_RAW, v);
                        }
                }
                /* Recent-color cache tracks values that actually went
                   out on this stream. Exact replays (PALX/PSAMX) and
                   param replays send none, so they leave the cache. */
                if (bpk > 0 && !palx && !psamx && !psame && !vrpt &&
                    (bop == OP_PAL || bop == OP_PALR || bop == OP_PALT ||
                     bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC || bop == OP_PALD ||
                     bop == OP_ROWD || bop == OP_ROWQ || bop == OP_ROWS)) {
                    /* Lossless row leaves stream values in sorted order,
                       so the cache has to learn them in that order too.
                       The decoder notes the bytes it actually read.
                       Lossy planes (recon set) keep first-seen order. */
                    if (!E->recon && !rowpal_nosort &&
                        (bop == OP_ROWD || bop == OP_ROWQ || bop == OP_ROWS)) {
                        int16_t ord[PAL_MAX];
                        for (int i = 0; i < bpk; i++) ord[i] = bpal[i];
                        for (int a = 1; a < bpk; a++) {
                            int16_t v = ord[a];
                            int b = a;
                            while (b > 0 && ord[b - 1] > v) {
                                ord[b] = ord[b - 1];
                                b--;
                            }
                            ord[b] = v;
                        }
                        for (int i = 0; i < bpk; i++)
                            palq_note(E->palq, &E->palq_n, ord[i]);
                    } else {
                        for (int i = 0; i < bpk; i++)
                            palq_note(E->palq, &E->palq_n, bpal[i]);
                    }
                }
                if (bpred) {
                    memcpy(E->rrecs + E->nrr, brecs, bnr * sizeof(uint32_t));
                    if (ZE(zrs) && E->cur_ti >= 0)
                        for (size_t i = 0; i < bnr; i++)
                            fprintf(stderr, "RS E ti=%d op=%d @%d,%d %u:%u\n",
                                    E->cur_ti, bop, tx, ty,
                                    brecs[i] >> 16, brecs[i] & 0xFFFFu);
                    for (size_t i = 0; i < bnr; i++)
                        mdl_bump(&E->cm[E->rrecs[E->nrr + i] >> 16], &E->cm[HI_MDL_IDX],
                                 E->rrecs[E->nrr + i] >> 16,
                                 E->rrecs[E->nrr + i] & 0xFFFFu);
                    E->nrr += bnr;
                }
            }
            if (bpred || bop == OP_NOISE) {
                if (bnd < 0) *Sio = *bSp;
                else {
                    for (int _i = 0; _i < bnd; _i++) {
                        int _c = bdc[_i];
                        memcpy(Sio->As[_c], bdr[_i], NSPAT * sizeof(int32_t));
                        Sio->At[_c] = bdr[_i][NSPAT];
                        Sio->N[_c]  = bdr[_i][NSPAT + 1];
                    }
                    Sio->A_run = bAr; Sio->N_run = bNr;
                }
            }
            if (emit) {
                if (bop == OP_COPY || bop == OP_MVRUN ||
                    bop == OP_COPYP || bop == OP_MVRUNP ||
                    bop == OP_COPYNF || bop == OP_TX) {
                    E->mv_ax[tx / TILE_SZ] = bdx; E->mv_ay[tx / TILE_SZ] = bdy;
                    E->prev_dx = bdx; E->prev_dy = bdy; E->prev_copy = 1;
                } else {
                    E->mv_ax[tx / TILE_SZ] = 0x7FFFFFFF; E->prev_copy = 0;
                }
            }
            /* publish the leaf into the vops lane AFTER emission: MV
               prediction must see the previous lane state, matching the
               decoder which writes the cell only after the leaf has
               fully decoded */
            if (emit && ve) {
                if (bop == OP_NN || bop == OP_NNF || bop == OP_NNZ) E->nn_cnt++;
                vdu_log(ve);
                ve->rx = (int16_t)tx; ve->ry = (int16_t)ty;
                ve->rw = (int16_t)tw; ve->op = bop;
                ve->dx = (int16_t)bdx; ve->dy = (int16_t)bdy;
                ve->npal = (bop == OP_PAL || bop == OP_PALR ||
                            bop == OP_PALT || bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC ||
                            bop == OP_PALD) ? bpk : 0;
                if (bop == OP_PAL || bop == OP_PALR || bop == OP_PALT ||
                    bop == OP_PALQ || bop == OP_PALZ || bop == OP_PALU || bop == OP_PALC || bop == OP_PALD) {
                    memcpy(ve->pal, bpal, (size_t)bpk * sizeof(int16_t));
                    memcpy(ve->imap, bidx, (size_t)npix);
                }
                ve->sig = bsig;
            }
            ZT_ACC(5, ztt);
            return bcost;
}

static int enc_one_tile(ectx_t *E, int tx, int ty, int tw, int th,
                        int depth, pstate_t *Sio, int emit);

/* would tile index ti reproduce exactly under the previous leaf op?
   RUN: identical to prev at same coords. MVRUNP: SAD0 at last_tmv.
   IRUNP: SAD0 at last_imv with the source already encoded. */
static int tile_is_repeat(const ectx_t *E, int ti)
{
    int tx = (ti % E->ntx) * TILE_SZ, ty = (ti / E->ntx) * TILE_SZ;
    int tw = E->w - tx < TILE_SZ ? E->w - tx : TILE_SZ;
    int th = E->h - ty < TILE_SZ ? E->h - ty : TILE_SZ;
    int lim = (E->qs - 1) >> 1;
    if (E->last_op == OP_RUN) {
        if (!E->has_prev) return 0;
        return tile_maxd(E->curr + (size_t)ty * E->w + tx,
                         E->prev + (size_t)ty * E->w + tx,
                         E->w, tw, th) <= lim;
    }
    if (E->last_op == OP_MVRUNP && E->has_prev) {
        int dx = E->last_tmv[0], dy = E->last_tmv[1];
        if (mv_contained(E, tx, ty, tw, th, dx, dy))
            return tile_maxd(E->curr + (size_t)ty * E->w + tx,
                             E->prev + (size_t)(ty + dy) * E->w + tx + dx,
                             E->w, tw, th) <= lim;
        if (mv_full_hang(E, tx, ty, tw, th, dx, dy))
            return tile_maxd_clamp(E->curr, E->prev, E->w, E->h,
                                   tx, ty, tw, th, dx, dy) <= lim;
        return 0;
    }
    if (E->last_op == OP_IRUNP) {
        int dx = E->last_imv[0], dy = E->last_imv[1];
        int sx = tx + dx, sy = ty + dy;
        if (sx < 0 || sy < 0 || sx + tw > E->w || sy + th > E->h) return 0;
        int sti = ((sy + th - 1) / TILE_SZ) * E->ntx + (sx + tw - 1) / TILE_SZ;
        if (sti >= ti) return 0;            /* source must precede (raster) */
        return tile_maxd(E->curr + (size_t)ty * E->w + tx,
                         (E->recon ? E->recon : E->curr)
                           + (size_t)sy * E->w + sx,
                         E->w, tw, th) <= lim;
    }
    return 0;
}

/* repeat scan phase: write only the replayed pixels into recon, so
   the next tile's IRUNP source reads see exactly what the decoder
   will have.  Bookkeeping (vops/ptab/mv lanes) must NOT be touched
   here - a run that dies at length 1 falls back to a normal leaf
   encode, whose VRPT/mvpred reads must see the honest cell state. */
static void apply_rpt_recon(ectx_t *E, int t2, int w, int h, int ntx,
                            const int16_t *prev)
{
    int rx = (t2 % ntx) * TILE_SZ, ry = (t2 / ntx) * TILE_SZ;
    int rw = w - rx < TILE_SZ ? w - rx : TILE_SZ;
    int rh = h - ry < TILE_SZ ? h - ry : TILE_SZ;
    if (E->recon) {
        if (E->last_op == OP_IRUNP) {
            int ddx = E->last_imv[0], ddy = E->last_imv[1];
            for (int y = ry; y < ry + rh; y++) {
                int sy = y + ddy;
                if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
                for (int x = rx; x < rx + rw; x++) {
                    int sx = x + ddx;
                    if (sx < 0) sx = 0;
                    else if (sx >= w) sx = w - 1;
                    E->recon[(size_t)y * w + x] =
                        E->recon[(size_t)sy * w + sx];
                }
            }
        } else {
            int ddx = E->last_op == OP_MVRUNP ? E->last_tmv[0] : 0;
            int ddy = E->last_op == OP_MVRUNP ? E->last_tmv[1] : 0;
            for (int y = ry; y < ry + rh; y++) {
                int sy = y + ddy;
                if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
                for (int x = rx; x < rx + rw; x++) {
                    int sx = x + ddx;
                    if (sx < 0) sx = 0;
                    else if (sx >= w) sx = w - 1;
                    E->recon[(size_t)y * w + x] =
                        prev[(size_t)sy * w + sx];
                }
            }
        }
    }
}

/* repeat commit phase: publish the replayed descriptor and advance
   the MV bookkeeping for each tile the RPT record covers. */
static void apply_rpt_book(ectx_t *E, int t2, int w, int ntx)
{
    int rx = (t2 % ntx) * TILE_SZ, ry = (t2 / ntx) * TILE_SZ;
    int rw = w - rx < TILE_SZ ? w - rx : TILE_SZ;
    {
        vop_t *ve = &E->vops[vcell_idx(ntx, rx, rw)];
        ve->rx = (int16_t)rx; ve->ry = (int16_t)ry;
        ve->rw = (int16_t)rw; ve->op = E->last_op;
        ve->npal = 0; ve->sig = 0;
        ve->dx = (int16_t)(E->last_op == OP_MVRUNP
                           ? E->last_tmv[0]
                           : E->last_op == OP_IRUNP ? E->last_imv[0] : 0);
        ve->dy = (int16_t)(E->last_op == OP_MVRUNP
                           ? E->last_tmv[1]
                           : E->last_op == OP_IRUNP ? E->last_imv[1] : 0);
        if (E->ptab) E->ptab[t2] = *ve;
    }
    if (E->last_op == OP_MVRUNP) {
        E->mv_ax[(t2 % ntx)] = E->last_tmv[0];
        E->mv_ay[(t2 % ntx)] = E->last_tmv[1];
        E->prev_dx = E->last_tmv[0]; E->prev_dy = E->last_tmv[1];
        E->prev_copy = 1;
    } else {
        E->mv_ax[(t2 % ntx)] = 0x7FFFFFFF;
        E->prev_copy = 0;
    }
}

/* Exact copy of tile ti from prev at (dx,dy), within the quantizer
   bound. (0,0) is the co-located RUN test. Overhang uses the same
   clamp as the decoder, and only on a full frame. */
static int tile_copy_ok(const ectx_t *E, int ti, int dx, int dy, int lim)
{
    int tx = (ti % E->ntx) * TILE_SZ, ty = (ti / E->ntx) * TILE_SZ;
    if (ty >= E->h) return 0;
    int tw = E->w - tx < TILE_SZ ? E->w - tx : TILE_SZ;
    int th = E->h - ty < TILE_SZ ? E->h - ty : TILE_SZ;
    if (mv_contained(E, tx, ty, tw, th, dx, dy))
        return tile_maxd(E->curr + (size_t)ty * E->w + tx,
                         E->prev + (size_t)(ty + dy) * E->w + tx + dx,
                         E->w, tw, th) <= lim;
    if (mv_full_hang(E, tx, ty, tw, th, dx, dy))
        return tile_maxd_clamp(E->curr, E->prev, E->w, E->h,
                               tx, ty, tw, th, dx, dy) <= lim;
    return 0;
}

/* Symbols the ordinary RPT / MVRUNP / RUN walk would emit for ak[0..k),
   where 1 is a predicted-MV tile and 0 is a co-located tile. Frozen
   CDF, so this is the cost the first segment actually pays. */
static int span_alt_cost(const ectx_t *E, int ti, int k, const uint8_t *ak)
{
    int ntx = E->ntx;
    int lop = E->last_op;
    int cost = 0;
    uint8_t us[128];
    if (!ZE(znoup)) memcpy(us, E->upstat, (size_t)ntx);
    for (int i = 0; i < k; ) {
        int want = ak[i] ? OP_MVRUNP : OP_RUN;
        int col = (ti + i) % ntx;
        int up = ZE(znoup) ? 0 : us[col];
        int lopm = lop < N_OPOPS ? lop : 0;
        if (lopm == want) {
            int r = 1;
            while (i + r < k && ak[i + r] == ak[i]) r++;
            if (r >= 2) {
                int oc = PM_OP0 + (lopm << 1) + up;
                cost += mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_RPT);
                cost += mlensym_cost_q8(&E->pmc[PM_REPN], &E->pmc[PM_HI],
                                        (uint32_t)(r - 1));
                if (!ZE(znoup))
                    for (int j = 0; j < r; j++)
                        us[(ti + i + j) % ntx] = 0;
                i += r;
                continue;
            }
        }
        {
            int oc = PM_OP0 + (lopm << 1) + up;
            cost += mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI],
                                    (uint32_t)want);
        }
        lop = want;
        if (!ZE(znoup)) us[col] = 0;
        i++;
    }
    return cost;
}

/* OP_SPAN over a prefix of tiles that are motion at last_tmv or a
   single co-located hole. Returns tiles consumed, or 0 with encoder
   state unchanged. Full frames only: repeat recon clamps to the
   plane, and the decoder clamps to the same window only then. */
static int try_op_span(ectx_t *E, int ti, int nti, int w, int h, int ntx,
                       const int16_t *prev)
{
    if (!E->has_prev || E->yoff != 0 || E->h != E->hfull) return 0;
    if (E->last_tmv[0] == 0 && E->last_tmv[1] == 0) return 0;
    if (ntx < 1 || ntx > 128 || ti >= nti) return 0;
    int dx = E->last_tmv[0], dy = E->last_tmv[1];
    int lim = (E->qs - 1) >> 1;
    int runs[64];
    uint8_t ak[512];
    int nr = 0, run = 0, trail = 0, k = 0, holes = 0;
    for (int i = 0; i < nti - ti && k < 512; ) {
        int at = ti + i;
        int mot = tile_copy_ok(E, at, dx, dy, lim);
        int hol = tile_copy_ok(E, at, 0, 0, lim);
        if (mot) {
            if (run == 0 && nr >= 64) break;
            ak[k] = hol ? 0 : 1;   /* real walk prefers the RUN early-out */
            run++;
            trail = 0;
            k++;
            i++;
            /* A pure motion run is already an OP_RPT. The bytes we
               can still save are the per-row holes, which sit inside
               the first row on the measured scroll. */
            if (holes == 0 && k >= ntx) return 0;
        } else if (hol) {
            if (run == 0 || trail || nr >= 64) break;
            runs[nr++] = run;
            run = 0;
            trail = 1;
            holes++;
            ak[k] = 0;
            k++;
            i++;
        } else break;
    }
    if (run > 0) {
        if (nr >= 64) {
            k -= run;
            run = 0;
            trail = 1;
        } else runs[nr++] = run;
    }
    if (nr < 1 || holes < 1 || k < 2) return 0;
    int expect = trail;
    for (int r = 0; r < nr; r++) expect += runs[r];
    expect += nr - 1;
    if (expect != k) return 0;
    int lopm = E->last_op < N_OPOPS ? E->last_op : 0;
    int up0 = ZE(znoup) ? 0 : E->upstat[ti % ntx];
    int oc = PM_OP0 + (lopm << 1) + up0;
    int sc = mlensym_cost_q8(&E->pmc[oc], &E->pmc[PM_HI], OP_SPAN);
    sc += mlensym_cost_q8(&E->pmc[PM_REPN], &E->pmc[PM_HI], (uint32_t)(k - 1));
    for (int r = 0; r < nr; r++)
        sc += mlensym_cost_q8(&E->pmc[PM_RRL], &E->pmc[PM_HI],
                              (uint32_t)(runs[r] - 1));
    int alt = span_alt_cost(E, ti, k, ak);
    if (sc >= alt) return 0;
    if (ZE(zd2))
        fprintf(stderr, "E SPAN ti=%d k=%d nr=%d holes=%d sc=%d alt=%d\n",
                ti, k, nr, holes, sc, alt);
    E->precs[E->npr++] = ((uint32_t)oc << 16) | (uint32_t)OP_SPAN;
    mdl_bump(&E->pmc[oc], &E->pmc[PM_HI], (uint32_t)oc, (uint32_t)OP_SPAN);
    E->precs[E->npr++] = ((uint32_t)PM_REPN << 16) | (uint32_t)(k - 1);
    mdl_bump(&E->pmc[PM_REPN], &E->pmc[PM_HI], PM_REPN, (uint32_t)(k - 1));
    for (int r = 0; r < nr; r++) {
        E->precs[E->npr++] = ((uint32_t)PM_RRL << 16) | (uint32_t)(runs[r] - 1);
        mdl_bump(&E->pmc[PM_RRL], &E->pmc[PM_HI], PM_RRL,
                 (uint32_t)(runs[r] - 1));
    }
    E->all_run = 0;
    int p = 0;
    for (int r = 0; r < nr; r++) {
        for (int j = 0; j < runs[r]; j++, p++) {
            E->last_op = OP_MVRUNP;
            apply_rpt_recon(E, ti + p, w, h, ntx, prev);
            apply_rpt_book(E, ti + p, w, ntx);
            E->upstat[(ti + p) % ntx] = 0;
        }
        if (r + 1 < nr || trail) {
            E->last_op = OP_RUN;
            apply_rpt_recon(E, ti + p, w, h, ntx, prev);
            apply_rpt_book(E, ti + p, w, ntx);
            E->upstat[(ti + p) % ntx] = 0;
            p++;
        }
    }
    return k;
}

/* encode one plane as: prog_hdr[4] + prog_blob + res_blob.
   prog_hdr bit31 = learned-predictor weights precede the tile ops;
   prog_hdr==0 means every tile is RUN (4-byte record, no streams). */
/* pm_io/rm_io: persistent model arrays (in = carried base state, out =
   post-stream state on success).  mwarm mirrors the decoder's model
   warmup flag: cold streams run the dual-theme pass and still write the
   winning post-state back through pm_io/rm_io. */
static int32_t enc_plane_core(const int16_t *prev, const int16_t *curr,
                              int w, int h, const int16_t *luma,
                              const int16_t *W,
                              uint8_t *out, int32_t out_cap,
                              int qs, int qs_hi, int16_t *recon,
                              vop_t *ptab, const int16_t *Wbase,
                              mdl64_t *pm_io, mdl64_t *rm_io, int mwarm,
                              pstate_t *ps_io, int *nn_out,
                              int yoff, int hfull, int32_t abest)
{
    int ntx = (w + TILE_SZ - 1) / TILE_SZ;
    int nty = (h + TILE_SZ - 1) / TILE_SZ;
    /* worst program records per tile: OP_SPLIT + 4 children, each up to
       1 op + 1 PALN + PAL_MAX entries + 2*(TILE/2)^2 palette records
       (PALR emits an (idx,run) pair per index run: <= 2 recs/px) */
    size_t maxpr = (size_t)ntx * nty *
                   (5 + 4 * (2 + PAL_MAX + 2 * (TILE_SZ / 2) * (TILE_SZ / 2)))
                 + 160;
    size_t maxrr = (size_t)w * h * 2 + 16;
    size_t encscr = maxpr * sizeof(uint32_t) + maxrr * sizeof(uint32_t)
                  + sizeof(mdl64_t) * (N_MDL + NPROG_MDL)
                  + 2 * (size_t)ntx * sizeof(int)
                  + (size_t)VOP_CELLS(ntx) * sizeof(vop_t)
                  + (size_t)ntx;
    uint8_t *ea = (uint8_t *)tls_get(TLS_ENCRECS, encscr);
    if (!ea) { return -1; }
    uint32_t *precs = (uint32_t *)ea;
    uint32_t *rrecs = (uint32_t *)(ea + maxpr * sizeof(uint32_t));
    mdl64_t *cm  = (mdl64_t *)(ea + (maxpr + maxrr) * sizeof(uint32_t));
    mdl64_t *pmc = cm + N_MDL;
    /* cost-eval models: peaked-prior init, bumped per committed record,
       renormalized per tile - approximates the real streams' CDFs.
       Warm stateful streams seed from the carried model state so op
       selection prices against the distribution the stream will use. */
    if (pm_io && rm_io && mwarm) {
        memcpy(pmc, pm_io, sizeof(mdl64_t) * NPROG_MDL);
        memcpy(cm, rm_io, sizeof(mdl64_t) * N_MDL);
    } else {
        uint16_t priors[7][MDL_ALPHA];
        for (int c = 0; c < 7; c++) mdl64_build_prior(priors[c], 1, c);
        for (int i = 0; i < N_MDL; i++)
            mdl64_init(&cm[i], priors[mdl64_class((uint32_t)i)]);
        for (int i = 0; i < NPROG_MDL; i++)
            mdl64_init(&pmc[i], priors[prog_cls[i]]);
    }

    int has_prev = (prev != NULL);
    if (!has_prev) {
        /* PREDIR pricing seed: the FLAG prior assumes a binary 0/1
           alphabet and the RUN prior assumes escape lengths, so on a
           prev-less plane a left/up run is priced ~9 bits a symbol
           forever and the candidate can never win a tile - a cold-model
           deadlock.  A few virtual counts in the cost-view models break
           it.  This only biases candidate pricing; the stream's own
           models adapt from actually-emitted records as usual. */
        for (int i = 0; i < NFLG_CTX; i++) {
            mdl64_t *m = &cm[FLAG_MDL_IDX + i];
            for (int k = 0; k < 40; k++)
                mdl_bump(m, &cm[HI_MDL_IDX], (uint32_t)(FLAG_MDL_IDX + i), 4);
            for (int k = 0; k < 24; k++)
                mdl_bump(m, &cm[HI_MDL_IDX], (uint32_t)(FLAG_MDL_IDX + i), 5);
            for (int k = 0; k < 16; k++) {
                mdl_bump(m, &cm[HI_MDL_IDX], (uint32_t)(FLAG_MDL_IDX + i), 2);
                mdl_bump(m, &cm[HI_MDL_IDX], (uint32_t)(FLAG_MDL_IDX + i), 3);
            }
            mdl64_norm(m);   /* cost view reads f[] - materialize now */
        }
        for (int i = 0; i < NRUN_CTX; i++) {
            mdl64_t *m = &cm[RUN_MDL_IDX + i];
            for (int k = 0; k < 40; k++)
                mdl_bump(m, &cm[HI_MDL_IDX], (uint32_t)(RUN_MDL_IDX + i), 15);
            for (int k = 0; k < 16; k++) {
                mdl_bump(m, &cm[HI_MDL_IDX], (uint32_t)(RUN_MDL_IDX + i), 7);
                mdl_bump(m, &cm[HI_MDL_IDX], (uint32_t)(RUN_MDL_IDX + i), 3);
            }
            mdl64_norm(m);
        }
    }
    pstate_t S; pstate_init(&S);
    size_t npr = 0, nrr = 0;
    uint32_t tile_tmp[TILE_SZ * TILE_SZ * 2 + 16];
    int all_run = has_prev;            /* every tile RUN -> plen=0 record */
    int *mv_ax = (int *)(pmc + NPROG_MDL);
    int *mv_ay = mv_ax + ntx;
    for (int i = 0; i < ntx; i++) {
        mv_ax[i] = 0x7FFFFFFF;
        mv_ay[i] = 0x7FFFFFFF;
    }

    ectx_t E;
    /* depth-indexed eval scratch (one allocation per worker thread):
       bS snapshots + BS_SAVE delta rows + record ping-pong + pxcache +
       the notxn probe copy, all slotted by recursion depth */
    size_t edsz = (size_t)ENC_MAXD * (
                    N_CTX * sizeof(uint16_t)
                  + (size_t)N_CTX * (NSPAT + 2) * sizeof(int32_t)
                  + (size_t)TILE_SZ * TILE_SZ * sizeof(pxcache_t)
                  + (size_t)(TILE_SZ * TILE_SZ * 4 + 32) * sizeof(uint32_t)
                  + 2 * sizeof(pstate_t)
                  + 4 * sizeof(cap_t));
    uint8_t *ed = (uint8_t *)tls_get(TLS_EDSTK, edsz);
    if (!ed) { return -1; }
    uint8_t *edp = ed;
    E.bdc_stk   = (uint16_t (*)[N_CTX])edp;
    edp += (size_t)ENC_MAXD * N_CTX * sizeof(uint16_t);
    E.bdr_stk   = (int32_t (*)[NSPAT + 2])edp;
    edp += (size_t)ENC_MAXD * N_CTX * (NSPAT + 2) * sizeof(int32_t);
    E.pxc_stk   = (pxcache_t *)edp;
    edp += (size_t)ENC_MAXD * TILE_SZ * TILE_SZ * sizeof(pxcache_t);
    E.recbuf_stk = (uint32_t *)edp;
    edp += (size_t)ENC_MAXD * (TILE_SZ * TILE_SZ * 4 + 32) * sizeof(uint32_t);
    E.sc_stk    = (pstate_t *)edp;
    edp += (size_t)ENC_MAXD * sizeof(pstate_t);
    E.bS_stk    = (pstate_t *)edp;
    edp += (size_t)ENC_MAXD * sizeof(pstate_t);
    E.cap_stk   = (cap_t *)edp;
    E.cap_out = NULL; E.cap_rep = NULL;
    E.pu.epoch = 0; E.pu2.epoch = 0;
    memset(E.pu.stamp, 0, sizeof E.pu.stamp);
    memset(E.pu2.stamp, 0, sizeof E.pu2.stamp);
    E.pal_epoch = 0;
    memset(E.pal_seen, 0, sizeof E.pal_seen);
    E.palq_n = 0;
    memset(E.palq, 0, sizeof E.palq);
    E.pcap = INT32_MAX;
    E.prev = prev; E.curr = curr; E.luma = luma; E.W = W;
    E.w = w; E.h = h; E.ntx = ntx; E.has_prev = has_prev;
    if (ps_io && mwarm) E.S = *ps_io; else pstate_init(&E.S);
    E.precs = precs; E.rrecs = rrecs; E.tile_tmp = tile_tmp;
    E.npr = 0; E.nrr = 0;
    E.pmc = pmc; E.cm = cm;
    E.mv_ax = mv_ax; E.mv_ay = mv_ay;
    E.prev_dx = 0; E.prev_dy = 0; E.prev_copy = 0;
    E.last_tmv[0] = E.last_tmv[1] = 0;
    E.last_imv[0] = E.last_imv[1] = 0;
    E.all_run = has_prev;
    E.nn_cnt = 0;
    E.fg_hits = 0;
    E.gmv_valid = 0;
    E.last_op = OP_RUN;
    E.qs = qs;
    E.qs_hi = qs_hi > qs ? qs_hi : qs;
    E.recon = (qs > 1) ? recon : NULL;
    E.ptab = ptab;
    E.cur_ti = 0;
    E.yoff = yoff; E.hfull = hfull;
    E.vops = (vop_t *)(mv_ay + ntx);
    memset(E.vops, 0, (size_t)VOP_CELLS(ntx) * sizeof(vop_t));
    E.upstat = (uint8_t *)(E.vops + VOP_CELLS(ntx));
    memset(E.upstat, 0, (size_t)ntx);
    if (ZE(zd2)) fprintf(stderr, "EP %x\n", (unsigned)((uintptr_t)&E & 0xffff));

    /* tile dictionaries: prev-frame windows keyed in GLOBAL coords
       (every prev row is decodable at this point, so any position is a
       legal COPY source) and this stripe's curr windows in local coords
       (row-causality is enforced per candidate by the eval). */
    E.pdict = NULL; E.cdict = NULL;
    if (has_prev && w >= TILE_SZ && hfull >= TILE_SZ) {
        tdent_t *t = (tdent_t *)tls_get(TLS_TDP,
                                        TD_SZ * sizeof(tdent_t));
        if (t) {
            E.pd_.t = t;
            td_build(&E.pd_, prev, w, 0, -yoff, w, hfull, 0, yoff);
            E.pdict = &E.pd_;
        }
    }
    if (w >= TILE_SZ && h >= TILE_SZ) {
        tdent_t *t = (tdent_t *)tls_get(TLS_TDC,
                                        TD_SZ * sizeof(tdent_t));
        if (t) {
            E.cd_.t = t;
            td_build(&E.cd_, curr, w, 0, 0, w, h, 0, 0);
            E.cdict = &E.cd_;
        }
    }

    /* dominant-motion probe: SAD over a decimated tile subset lets wrap
       rows near the frame edge reuse the scroll vector before any tile
       has discovered it locally */
    if (has_prev && !ZE(zngmv)) {
        int pcands[N_MVC + 64][2], npc = 0;
        for (int i = 0; i < N_MVC; i++) {
            pcands[npc][0] = mvc[i][0]; pcands[npc][1] = mvc[i][1]; npc++;
        }
        static const int axd[8] = {1, -1, 2, -2, 4, -4, 8, -8};
        for (int d = 0; d < 4; d++) {
            pcands[npc][0] = axd[d];    pcands[npc][1] = 0;       npc++;
            pcands[npc][0] = axd[d + 4]; pcands[npc][1] = 0;      npc++;
            pcands[npc][0] = 0;         pcands[npc][1] = axd[d];  npc++;
            pcands[npc][0] = 0;         pcands[npc][1] = axd[d + 4]; npc++;
        }
        long best = 0x7FFFFFFFFFFFFFFF;
        for (int i = 0; i < npc; i++) {
            long s = 0; int ns = 0;
            int dx = pcands[i][0], dy = pcands[i][1];
            for (int ty = 0; ty + TILE_SZ <= h; ty += 4 * TILE_SZ)
                for (int tx = 0; tx + TILE_SZ <= w; tx += 4 * TILE_SZ) {
                    if (tx + dx < 0 || tx + TILE_SZ + dx > w ||
                        ty + dy < -yoff ||
                        ty + TILE_SZ + dy > hfull - yoff) continue;
                    s += tile_sad(curr + (size_t)ty * w + tx,
                                  prev + (size_t)(ty + dy) * w + tx + dx,
                                  w, TILE_SZ, TILE_SZ, 0x7FFFFFFF);
                    if (s > best && ns) break;   /* can't win - bail */
                    ns++;
                }
            if (ns && s < best) {
                best = s; E.gmv[0] = dx; E.gmv[1] = dy; E.gmv_valid = 1;
            }
        }
        if (E.gmv[0] == 0 && E.gmv[1] == 0) E.gmv_valid = 0;
    }

    /* quantizer step precedes weights in the program stream (the
       decoder reads it first); a noise seed follows so OP_NOISE tiles
       can synthesize deterministic pseudo-noise */
    if (qs > 1) {
        uint32_t sd = 0x9E3779B9u;
        for (size_t i = 0; i < (size_t)w * h; i += 97)
            sd = sd * 1099511628211u ^ (uint32_t)curr[i];
        sd &= 0xFFFFu;
        E.nseed = sd;
        E.precs[E.npr++] = ((uint32_t)PM_QT << 16) | (uint32_t)(qs - 1);
        mdl_bump(&pmc[PM_QT], &pmc[PM_HI], PM_QT, (uint32_t)(qs - 1));
        E.precs[E.npr++] = ((uint32_t)PM_QT << 16) | sd;
        mdl_bump(&pmc[PM_QT], &pmc[PM_HI], PM_QT, sd);
    }
    /* learned weights travel first in the program stream; stateful
       streams delta-code them against the previous frame's table, and
       an identical table collapses to a single header bit (WSAME) */
    int wsame = W && Wbase &&
                !memcmp(W, Wbase, sizeof(int16_t) * N_NNW * N_NNC);
    if (ZE(zwn) && W) {
        uint32_t hh = 5381;
        for (int i = 0; i < N_NNW * N_NNC; i++) hh = hh * 33 + (uint16_t)W[i];
        fprintf(stderr, "EW: W=%p Wbase=%p wsame=%d qs=%d wh=%08x\n",
                (const void*)W, (const void*)Wbase, wsame, qs, hh);
    }
    if (W && !wsame)
        for (int i = 0; i < N_NNW * N_NNC; i++) {
            /* the record packs into 16 bits - clamp the delta so its
               zigzag encoding stays <= 0xFFFF (delta range of two
               int16 spans twice that) */
            int d16 = W[i] - (Wbase ? Wbase[i] : 0);
            if (ZE(zwn) && (d16 > 32767 || d16 < -32768))
                fprintf(stderr, "WOVF i=%d W=%d base=%d d=%d\n",
                        i, W[i], Wbase[i], d16);
            uint32_t v = zigzag_enc(d16);
            E.precs[E.npr++] = ((uint32_t)PM_NNW << 16) | v;
            mdl_bump(&pmc[PM_NNW], &pmc[PM_HI], PM_NNW, v);
        }

    int nti = ntx * nty;
    /* abest>0: abort once the accumulated modeled tile cost provably
       exceeds the best already-encoded stream.  The decision models
       track the real rANS cost loosely, so the kill line carries a
       wide slack - it only fires on candidates that are losing hard. */
    int64_t acc8 = 0, ab8 = 0;
    if (abest > 0) {
        int pc = ZE(zab);
        ab8 = (int64_t)abest * 2048 * pc / 100 + 65536;
    }
    struct timespec zt0, zt1;
    if (ZE(zt)) clock_gettime(CLOCK_MONOTONIC, &zt0);
    for (int ti = 0; ti < nti; ) {
        int kspan = try_op_span(&E, ti, nti, w, h, ntx, prev);
        if (kspan > 0) {
            ti += kspan;
            if (ti % ntx == 0) {
                cm_norm_sweep(cm, N_MDL);
                cm_norm_sweep(pmc, NPROG_MDL);
                if (ab8 > 0 && acc8 > ab8) return -2;
            }
            continue;
        }
        /* run-collapse: tiles that reproduce exactly under the previous
           leaf op are already optimal; collapse runs >= 2 into one
           OP_RPT record */
        int k = 0;
        if (tile_is_repeat(&E, ti)) {
            int lim = nti - ti < 60000 ? nti - ti : 60000;
            /* mirror the decoder: apply each accepted repeat before
               testing the next tile - an IRUNP source inside tiles
               already claimed by this run must read the replayed
               pixels, not the not-yet-written recon garbage */
            do {
                apply_rpt_recon(&E, ti + k, w, h, ntx, prev);
                k++;
            } while (k < lim && tile_is_repeat(&E, ti + k));
        }
        if (k >= 2) {
            int oc = PM_OP0 + (E.last_op << 1)
                   + (ZE(znoup) ? 0 : E.upstat[ti % ntx]);
            if (ZE(zd2)) fprintf(stderr, "E %x ti=%d op=%d k=%d lo=%d up=%d\n",
                                       (unsigned)((uintptr_t)&E & 0xffff),
                                       ti, OP_RPT, k, E.last_op,
                                       ZE(znoup) ? 0 : E.upstat[ti % ntx]);
            if (ZE(zop)) zopc[OP_RPT]++;
            E.precs[E.npr++] = ((uint32_t)oc << 16) | OP_RPT;
            mdl_bump(&pmc[oc], &pmc[PM_HI], oc, OP_RPT);
            E.precs[E.npr++] = ((uint32_t)PM_REPN << 16) | (uint32_t)(k - 1);
            mdl_bump(&pmc[PM_REPN], &pmc[PM_HI], PM_REPN, (uint32_t)(k - 1));
            if (E.last_op != OP_RUN) E.all_run = 0;
            for (int j = 0; j < k; j++) {
                apply_rpt_book(&E, ti + j, w, ntx);
                E.upstat[(ti + j) % ntx] = 0;
            }
            ti += k;
        } else {
            int tx = (ti % ntx) * TILE_SZ, ty = (ti / ntx) * TILE_SZ;
            int tw = w - tx < TILE_SZ ? w - tx : TILE_SZ;
            int th = h - ty < TILE_SZ ? h - ty : TILE_SZ;
            E.cur_ti = ti;
            if (ZE(zt)) {
                struct timespec a, b;
                clock_gettime(CLOCK_MONOTONIC, &a);
                acc8 += enc_one_tile(&E, tx, ty, tw, th, 0, &E.S, 1);
                clock_gettime(CLOCK_MONOTONIC, &b);
                extern _Atomic long ztile_ns;
                atomic_fetch_add(&ztile_ns,
                    (b.tv_sec - a.tv_sec) * 1000000000L + (b.tv_nsec - a.tv_nsec));
            } else
                acc8 += enc_one_tile(&E, tx, ty, tw, th, 0, &E.S, 1);
            if (ZE(zrh) && E.recon) {
                uint64_t hh = 1469598103934665603ull;
                for (int yy = ty; yy < ty + th; yy++)
                    for (int xx = tx; xx < tx + tw; xx++) {
                        hh ^= (uint16_t)E.recon[(size_t)yy * w + xx];
                        hh *= 1099511628211ull;
                    }
                fprintf(stderr, "RH E %x ti=%d %d,%d %dx%d h=%016llx\n",
                        (unsigned)((uintptr_t)&E & 0xffff), ti, tx, ty, tw, th,
                        (unsigned long long)hh);
            }
            ti++;
        }
        if (ti % ntx == 0) {
            cm_norm_sweep(cm, N_MDL);
            cm_norm_sweep(pmc, NPROG_MDL);
            /* hopeless candidate: the committed modeled cost already
               clears the best stream's size - the losing pass is dead
               no matter what the remaining tiles do */
            if (ab8 > 0 && acc8 > ab8) return -2;
        }
    }
    npr = E.npr; nrr = E.nrr; all_run = E.all_run;
    if (ZE(zpoi)) {
        extern _Atomic long pcw[160], pcl[160];
        for (int i = 0; i < 160; i++)
            if (pcw[i] + pcl[i])
                fprintf(stderr, "POI%d w=%ld l=%ld\n", i,
                        (long)pcw[i], (long)pcl[i]);
    }
    if (ZE(zacc))
        fprintf(stderr, "ACC est=%ld\n", (long)(acc8 / 256));
    if (ZE(zt)) {
        clock_gettime(CLOCK_MONOTONIC, &zt1);
        static _Atomic long zloop_ns, zcalls;
        long ns = (zt1.tv_sec - zt0.tv_sec) * 1000000000L
                + (zt1.tv_nsec - zt0.tv_nsec);
        atomic_fetch_add(&zloop_ns, ns);
        atomic_fetch_add(&zcalls, 1);
        if ((atomic_load(&zcalls) & 7) == 0)
            fprintf(stderr, "ZT loop=%ldms tile=%ldms calls=%ld sec=[%ld %ld %ld %ld %ld %ld]ms\n",
                    atomic_load(&zloop_ns) / 1000000,
                    ztile_ns / 1000000, zcalls,
                    zsec[0]/1000000, zsec[1]/1000000, zsec[2]/1000000,
                    zsec[3]/1000000, zsec[4]/1000000, zsec[5]/1000000);
        fprintf(stderr, "   txn win=%ld lose=%ld ovr=%ld calls=%ld\n",
                (long)ztxw, (long)ztxl, (long)ztxo, (long)zcalls_e);
        if (ZE(zd2)) fprintf(stderr, "   emit0=%ld emit1=%ld cap c=%d rep=%d chain=%d trans=%d\n",
                (long)zemit_n[0], (long)zemit_n[1],
                E.capn_commit, E.capn_rep, E.capn_chain, E.capn_trans);
    }
    if (ZE(zop)) {
        static const char *nm[43] = {"RUN","COPY","PAL","PRED","RAW","ICOPY",
            "NN","SPLIT","MVRUN","IRUN","MVRUNP","COPYP","IRUNP","RPT","QS","NOISE","VRPT","PSAME","CFL","PALR","PALX","PSAMX","PALT","PALC","PREDNF","NNF","PREDZ","NNZ","COPYNF","ICOPYNF",
            "PALD","PREDIR","PALQ","ROWD","ROWQ","ROWS","PALZ","PALU","SPAN","TX","TXI","PREDNFX","PREDX"};
        fprintf(stderr, "OPHIST:");
        for (int i = 0; i < 43; i++) if (zopc[i]) fprintf(stderr, " %s=%d", nm[i], zopc[i]);
        fprintf(stderr, "\n");
        memset(zopc, 0, sizeof(zopc));
    }

    /* all-RUN plane: program carries nothing, no residual stream */
    if (all_run) {
        if (out_cap < 4) { return -1; }
        wr_u32le(out, 0);
        return 4;
    }

    /* pass B: two independent v2-format blobs.  The committed predictor
       state becomes next frame's carried state (the decoder replays the
       same updates, so the planes stay in lockstep). */
    if (ps_io) *ps_io = E.S;
    if (out_cap < 4) return -1;
    int32_t pl = enc_stream_v(precs, npr, NPROG_MDL, 1, out + 4,
                              out_cap - 4, pm_io, mwarm, 1);
    int32_t rl = -1;
    if (pl >= 0) {
        if (ZE(zrs))
            for (uint32_t i = 0; i < nrr; i++)
                fprintf(stderr, "RS E %u:%d\n", rrecs[i] >> 16,
                        (int16_t)(rrecs[i] & 0xFFFF));
        /* residual is not a closed slice: the decoder may see the next
           plane after it, so the segment length has to stay in the blob */
        rl = enc_stream_v(rrecs, nrr, N_MDL, 0, out + 4 + pl,
                          out_cap - 4 - pl, rm_io, mwarm, 0);
    }
    if (pl < 0 || rl < 0) { return -1; }
    if (ZE(zbrk2)) {
        uint16_t prior[7][MDL_ALPHA];
        for (int c = 0; c < 7; c++) mdl64_build_prior(prior[c], 1, c);
        mdl64_t *sim = malloc(sizeof(mdl64_t) * NPROG_MDL);
        for (int i = 0; i < NPROG_MDL; i++)
            mdl64_init(&sim[i], prior[prog_cls[i]]);
        long bits[NPROG_MDL + 64]; memset(bits, 0, sizeof bits);
        for (uint32_t i = 0; i < npr; i++) {
            uint32_t m = precs[i] >> 16, v = precs[i] & 0xFFFFu;
            if (m >= NPROG_MDL + 64) m = NPROG_MDL + 63;
            bits[m] += mlensym_cost_q8(&sim[m], &sim[PM_HI], v);
            mdl_bump(&sim[m], &sim[PM_HI], m, v);
        }
        fprintf(stderr, "BRK2 prog=%d:", pl);
        for (int i = 0; i < NPROG_MDL + 64; i++)
            if (bits[i]) fprintf(stderr, " [%d]=%ld", i, bits[i] >> 8);
        fprintf(stderr, "\n");
        free(sim);
    }
    if (ZE(zbrk)) {
        static const char *mn[NPROG_MDL] = {
            "OP","MV","PALN","PALV","PALI","RAW","NNW"};
        int cnt[NPROG_MDL + 64] = {0};
        for (uint32_t i = 0; i < npr; i++) {
            int m = (int)(precs[i] >> 16);
            if (m < NPROG_MDL + 64) cnt[m]++;
        }
        fprintf(stderr, "PLANE prog=%d res=%d fg=%d |", pl, rl, E.fg_hits);
        for (int i = 0; i < PM_OP0; i++)
            if (cnt[i]) fprintf(stderr, " %s=%d", mn[i], cnt[i]);
        fprintf(stderr, " ops=%d repn=%d qt=%d", 0, cnt[PM_REPN], cnt[PM_QT]);
        int o = 0;
        for (int i = PM_OP0; i < PM_OP0 + N_OPCTX; i++) o += cnt[i];
        fprintf(stderr, " oprecs=%d | res:", o);
        /* per-class bit estimate on the final emitted records: replay the
           cm-model updates the emit path used and accumulate cost */
        {
            double bc[4] = {0,0,0,0};   /* flag, run, res, esc-bits */
            for (uint32_t i = 0; i < nrr; i++) {
                uint32_t m = rrecs[i] >> 16, v = rrecs[i] & 0xFFFFu;
                uint32_t cls;
                if (m >= FLAG_MDL_IDX && m < RUN_MDL_IDX) cls = v;
                else cls = v < LEN_MID ? v : (v < 318 ? LEN_MID : LEN_HI);
                double cost = -log2((double)cm[m].f[cls] / RANS_TOT);
                bc[m >= FLAG_MDL_IDX && m < RUN_MDL_IDX ? 0 :
                   m >= RUN_MDL_IDX && m < HI_MDL_IDX ? 1 : 2] += cost;
                if (v >= LEN_MID && !(m >= FLAG_MDL_IDX && m < RUN_MDL_IDX)) {
                    if (v < 318) bc[3] += LEN_MID_BITS;
                    else {
                        uint32_t hb = (v - 318) >> 6;
                        int hc = hb < (uint32_t)LEN_MID ? (int)hb
                               : (hb < 318u ? LEN_MID : LEN_HI);
                        bc[3] += -log2((double)cm[HI_MDL_IDX].f[hc] / RANS_TOT) + 6
                               + (hc == LEN_MID ? LEN_MID_BITS
                                  : hc == LEN_HI ? LEN_HI_BITS : 0);
                    }
                }
            }
            fprintf(stderr, " | bits fl=%.0f run=%.0f res=%.0f esc=%.0f seg=%ld",
                    bc[0], bc[1], bc[2], bc[3], (long)(nrr ? (nrr+16383)/16384 : 0));
        }
        int rcnt[16] = {0}, rfl = 0, rrun = 0; long rbits = 0;
        for (uint32_t i = 0; i < nrr; i++) {
            int m = (int)(rrecs[i] >> 16); int v = (int)(rrecs[i] & 0xFFFF);
            if (m >= FLAG_MDL_IDX && m < RUN_MDL_IDX) rfl++;
            else if (m == RUN_MDL_IDX) rrun++;
            else { int z = v > 127 ? 127 - v : v; int a = z < 0 ? -z : z;
                   if (a < 16) rcnt[a & 15]++;
                   }
        }
        if (ZE(zrs))
            for (uint32_t i = 0; i < nrr; i++)
                fprintf(stderr, "RSE %u:%u\n", rrecs[i] >> 16, rrecs[i] & 0xFFFFu);
        {   const char *rdp = ZEP(zrdump);
            if (rdp) { char fn[256]; static int dn = 0;
                snprintf(fn, sizeof fn, "%s_%d.bin", rdp, dn++);
                FILE *df = fopen(fn, "wb");
                if (df) { fwrite(&nrr, sizeof nrr, 1, df);
                          fwrite(rrecs, 4, nrr, df); fclose(df); }
                snprintf(fn, sizeof fn, "%s_p%d.bin", rdp, dn - 1);
                df = fopen(fn, "wb");
                if (df) { fwrite(&npr, sizeof npr, 1, df);
                          fwrite(precs, 4, npr, df); fclose(df); } } }
        for (int i = 0; i < 16; i++) if (rcnt[i]) fprintf(stderr, " r%d:%d", i, rcnt[i]);
        fprintf(stderr, " fl=%ld run=%ld", (long)rfl, (long)rrun); (void)rbits;
    }
    {   const char *rdp = ZEP(zrdump);
        if (rdp) { char fn[256]; static _Thread_local int dn = 0;
            unsigned tg = (unsigned)((uintptr_t)&E & 0xffff);
            snprintf(fn, sizeof fn, "%s_%x_%d.bin", rdp, tg, dn++);
            FILE *df = fopen(fn, "wb");
            if (df) { fwrite(&nrr, sizeof nrr, 1, df);
                      fwrite(rrecs, 4, nrr, df); fclose(df); }
            snprintf(fn, sizeof fn, "%s_%x_p%d.bin", rdp, tg, dn - 1);
            df = fopen(fn, "wb");
            if (df) { fwrite(&npr, sizeof npr, 1, df);
                      fwrite(precs, 4, npr, df); fclose(df); } } }
    wr_u32le(out, (uint32_t)pl | (W ? 0x80000000u : 0)
                               | (qs > 1 ? 0x60000000u : 0)
                               | (ptab ? PHDR_STATEFUL : 0)
                               | (wsame ? PHDR_WSAME : 0));
    if (nn_out) *nn_out = E.nn_cnt;
    return 4 + pl + rl;
}

/* encode each plane twice - once with the fixed predictor bank, once
   with a per-plane learned linear predictor - and keep the smaller.
   The encoder is the expensive side of the asymmetry by design. */
static int32_t enc_plane_stream_v3_g(const int16_t *prev,
                                   const int16_t *curr,
                                   int w, int h, const int16_t *luma,
                                   uint8_t *out, int32_t out_cap,
                                   int qs, int qs_hi, int16_t *recon,
                                   vop_t *ptab, int16_t *Wprev,
                                   int *wvalid,
                                   mdl64_t *pm, mdl64_t *rm, int *mvalid,
                                   pstate_t *ps, int yoff, int hfull)
{
    int stateful = (ptab && pm && rm && mvalid && ps) != 0;
    if (qs > 1) {
        /* EV probe: enable the learned predictor for lossy too -
           unconditional train, full weight records each frame */
        int16_t Wq[N_NNW * N_NNC];
        const int16_t *Wuse =
            (!luma && train_llp(prev, curr, w, h, luma, Wq, qs, NULL)) ? Wq : NULL;
        if (!(stateful && *mvalid)) {
            int32_t r = enc_plane_core(prev, curr, w, h, luma, Wuse,
                                       out, out_cap, qs, qs_hi, recon,
                                       ptab, NULL, pm, rm,
                                       pm && mvalid ? *mvalid : 0, ps, NULL, yoff, hfull, 0);
            if (pm && mvalid && r > 4) *mvalid = 1;
            if (Wuse && wvalid && Wprev && r > 4) {
                memcpy(Wprev, Wuse, sizeof(Wq)); *wvalid = 1;
            }
            return r;
        }
        /* stale carried models can badly misprice a shifted
           distribution - run the plane cold into scratch state and
           keep whichever stream wins.  PHDR_FRESH tells the decoder to
           re-seed; the post-state still carries forward either way. */
        int ntx = (w + TILE_SZ - 1) / TILE_SZ;
        int nti = ntx * ((h + TILE_SZ - 1) / TILE_SZ);
        vop_t *pt_c = (vop_t *)malloc((size_t)nti * sizeof(vop_t));
        vop_t *pt_w = (vop_t *)malloc((size_t)nti * sizeof(vop_t));
        vop_t *pt_n = (vop_t *)malloc((size_t)nti * sizeof(vop_t));
        vop_t *pt_s = (vop_t *)malloc((size_t)nti * sizeof(vop_t));
        mdl64_t *pm_c = (mdl64_t *)malloc(NPROG_MDL * sizeof(mdl64_t));
        mdl64_t *rm_c = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        mdl64_t *pm_w = (mdl64_t *)malloc(NPROG_MDL * sizeof(mdl64_t));
        mdl64_t *rm_w = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        mdl64_t *pm_n = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        mdl64_t *rm_n = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        mdl64_t *pm_s = (mdl64_t *)malloc(NPROG_MDL * sizeof(mdl64_t));
        mdl64_t *rm_s = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        int16_t *rc_c = recon ? (int16_t *)malloc((size_t)w * h * 2) : NULL;
        int16_t *rc_w = recon ? (int16_t *)malloc((size_t)w * h * 2) : NULL;
        int16_t *rc_n = recon ? (int16_t *)malloc((size_t)w * h * 2) : NULL;
        int16_t *rc_s = recon ? (int16_t *)malloc((size_t)w * h * 2) : NULL;
        uint8_t *tmp = (uint8_t *)malloc((size_t)out_cap);
        uint8_t *tmpw = (uint8_t *)malloc((size_t)out_cap);
        uint8_t *tmpn = (uint8_t *)malloc((size_t)out_cap);
        uint8_t *tmps = (uint8_t *)malloc((size_t)out_cap);
        pstate_t ps_c, ps_w, ps_n, ps_s;
        if (!pt_c || !pt_w || !pt_n || !pt_s || !pm_c || !rm_c || !pm_w ||
            !rm_w || !pm_n || !rm_n || !pm_s || !rm_s ||
            !tmp || !tmpw || !tmpn || !tmps ||
            (recon && (!rc_c || !rc_w || !rc_n || !rc_s))) {
            free(pt_c); free(pt_w); free(pt_n); free(pt_s);
            free(pm_c); free(rm_c); free(pm_w); free(rm_w);
            free(pm_n); free(rm_n); free(pm_s); free(rm_s);
            free(rc_c); free(rc_w); free(rc_n); free(rc_s);
            free(tmp); free(tmpw); free(tmpn); free(tmps);
            return -1;
        }
        /* stale carried models can badly misprice a shifted
           distribution - the cold stream keeps PHDR_FRESH honest.
           Warm candidates run into scratch too: the winner's post-state
           is the only one committed, and the unused-table retry must not
           double-bump the carried models. */
        memcpy(pt_c, ptab, (size_t)nti * sizeof(vop_t));
        const int16_t *Wb = (wvalid && *wvalid) ? Wprev : NULL;
        memcpy(pt_w, ptab, (size_t)nti * sizeof(vop_t));
        memcpy(pm_w, pm, NPROG_MDL * sizeof(mdl64_t));
        memcpy(rm_w, rm, N_MDL * sizeof(mdl64_t));
        ps_w = *ps;
        int nnw = 0;
        int32_t lw = enc_plane_core(prev, curr, w, h, luma, Wuse,
                                    tmpw, out_cap, qs, qs_hi, rc_w,
                                    pt_w, Wb, pm_w, rm_w, 1, &ps_w, &nnw, yoff, hfull, 0);
        /* the weight preamble can outweigh a few OP_NN wins - always
           re-encode without W and keep the smaller stream.  A carried
           table is a third variant: WSAME reuses last frame's weights
           for zero record cost. */
        int32_t ln = -1, ls = -1;
        if (lw > 4 && (Wuse || Wb) && !ZE(z1p)) {
            memcpy(pt_n, ptab, (size_t)nti * sizeof(vop_t));
            memcpy(pm_n, pm, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm_n, rm, N_MDL * sizeof(mdl64_t));
            ps_n = *ps;
            ln = enc_plane_core(prev, curr, w, h, luma, NULL,
                                tmpn, out_cap, qs, qs_hi, rc_n,
                                pt_n, NULL, pm_n, rm_n, 1, &ps_n, NULL, yoff, hfull,
                                lw > 4 ? lw : 0);
        }
        int32_t lc = ZE(z1p) ? -1 : enc_plane_core(prev, curr, w, h, luma, Wuse,
                                    tmp, out_cap, qs, qs_hi, rc_c,
                                    pt_c, Wb, pm_c, rm_c, 0, &ps_c, NULL, yoff, hfull,
                                    lw > 4 ? (ln > 4 && ln < lw ? ln : lw)
                                           : (ln > 4 ? ln : 0));
        if (Wb && lw > 4 && !ZE(z1p)) {
            memcpy(pt_s, ptab, (size_t)nti * sizeof(vop_t));
            memcpy(pm_s, pm, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm_s, rm, N_MDL * sizeof(mdl64_t));
            ps_s = *ps;
            {   int32_t ab = lw > 4 ? lw : 0;
                if (ln > 4 && (ab <= 0 || ln < ab)) ab = ln;
                if (lc > 4 && (ab <= 0 || lc < ab)) ab = lc;
                ls = enc_plane_core(prev, curr, w, h, luma, Wprev,
                                tmps, out_cap, qs, qs_hi, rc_s,
                                pt_s, Wb, pm_s, rm_s, 1, &ps_s, NULL, yoff, hfull, ab); }
        }
        /* winner commit: ln > lw-with-W is possible only when the table
           paid for itself, so rank strictly by emitted size */
        int32_t r = lw; const uint8_t *wsrc = tmpw;
        int16_t *wrc = rc_w; vop_t *wpt = pt_w;
        mdl64_t *wpm = pm_w, *wrm = rm_w; pstate_t wps = ps_w;
        int wcar = (Wuse != NULL);
        if (ln > 4 && (r < 0 || ln < r)) {
            r = ln; wsrc = tmpn; wrc = rc_n; wpt = pt_n;
            wpm = pm_n; wrm = rm_n; wps = ps_n; wcar = 0;
        }
        if (ls > 4 && (r < 0 || ls < r)) {
            r = ls; wsrc = tmps; wrc = rc_s; wpt = pt_s;
            wpm = pm_s; wrm = rm_s; wps = ps_s; wcar = 0;
        }
        if (lc > 4 && (r < 0 || lc < r)) {
            r = lc; wsrc = tmp; wrc = rc_c; wpt = pt_c;
            wpm = pm_c; wrm = rm_c; wps = ps_c; wcar = (Wuse != NULL);
        }
        if (r >= 4) {
            memcpy(out, wsrc, (size_t)r);
            if (wsrc == tmp)
                wr_u32le(out, rd_u32le(out) | PHDR_FRESH);
            if (recon && wrc) memcpy(recon, wrc, (size_t)w * h * 2);
            memcpy(ptab, wpt, (size_t)nti * sizeof(vop_t));
            memcpy(pm, wpm, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm, wrm, N_MDL * sizeof(mdl64_t));
            *ps = wps;
            *mvalid = 1;
            /* an all-RUN winner (r==4) emitted no weight records, so the
               decoder keeps its previous table: advance Wprev only when
               the stream actually carried W */
            if (wcar && wvalid && Wprev && r > 4) {
                memcpy(Wprev, Wuse, sizeof(Wq)); *wvalid = 1;
            }
        }
        free(pt_c); free(pt_w); free(pt_n); free(pt_s);
        free(pm_c); free(rm_c); free(pm_w); free(rm_w);
        free(pm_n); free(rm_n); free(pm_s); free(rm_s);
        free(rc_c); free(rc_w); free(rc_n); free(rc_s);
        free(tmp); free(tmpw); free(tmpn); free(tmps);
        return r;
    }

    /* Each candidate encode publishes this frame's leaf table.  Losers
       must not clobber the winner's program memory, so candidates write
       into a scratch preloaded with the PREVIOUS table and only the
       winner is committed back to ptab.  The same isolation applies to
       the carried entropy models: each candidate serializes from the
       carried base state and only the winner's post-state persists. */
    /* scene-change probe shared by the whole lossless duel below: on
       steady content the carried models are tracking and a converged W
       stays near-optimal, so neither the extra encode passes nor the
       least-squares refit pays.  Sample ~2k pixels and call a frame
       "moved" only when a quarter of them differ. */
    int moved = 1, churned = 1;
    if (prev && !ZE(znllp)) {
        int changed = 0, n = 0;
        size_t np = (size_t)w * h, step = np / 2048 + 1;
        for (size_t i = 0; i < np; i += step) {
            n++;
            if (prev[i] != curr[i]) changed++;
        }
        moved = changed * 4 > n;
        churned = changed * 2 > n;   /* >50% differ: real scene churn,
                                        not just animation */
    }
    /* steady state: collapse the duel to a single pass.  With a live
       Wprev the warm+W stream stands alone - the WSAME marker costs ~a
       bit and fresh-table competition only matters once content has
       actually moved.  Without one, warm-noW stands alone; a periodic
       retry keeps W discoverable when the content demands it (the
       process-wide tick is a heuristic cadence, not bookkeeping - the
       duel itself resumes below whenever the probe fires). */
    if (stateful && !moved) {
        /* *mvalid doubles as the per-plane steady-frame counter: it
           lives in the carried zv3s_t slot, so the retrain cadence is
           deterministic per plane (a process-wide tick raced across
           the parallel plane workers and made output run-dependent). */
        const uint32_t stick = (uint32_t)*mvalid;
        int ntx0 = (w + TILE_SZ - 1) / TILE_SZ;
        int nti0 = ntx0 * ((h + TILE_SZ - 1) / TILE_SZ);
        vop_t *pt_r = (vop_t *)malloc((size_t)nti0 * sizeof(vop_t));
        mdl64_t *pm_r = (mdl64_t *)malloc(NPROG_MDL * sizeof(mdl64_t));
        mdl64_t *rm_r = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        if (!pt_r || !pm_r || !rm_r) {
            free(pt_r); free(pm_r); free(rm_r); return -1;
        }
        pstate_t ps_r = *ps;
        memcpy(pt_r, ptab, (size_t)nti0 * sizeof(vop_t));
        memcpy(pm_r, pm, NPROG_MDL * sizeof(mdl64_t));
        memcpy(rm_r, rm, N_MDL * sizeof(mdl64_t));
        const int16_t *Wq = *wvalid ? Wprev : NULL;
        int r = -1;
        if (*wvalid || ((stick & 7) != 0)) {
            /* steady single pass: carried W (or no W), warm models */
            r = enc_plane_core(prev, curr, w, h, luma, (int16_t *)Wq,
                               out, out_cap, 1, 1, NULL,
                               pt_r, *wvalid ? Wprev : NULL, pm_r, rm_r,
                               *mvalid, &ps_r, NULL, yoff, hfull, 0);
        }
        /* every ~8th steady frame (or whenever no W exists yet) the
           duel below runs instead, refreshing the table */
        if (r > 0) {
            if (ZE(zwin)) fprintf(stderr, "WIN steady\n");
            memcpy(ptab, pt_r, (size_t)nti0 * sizeof(vop_t));
            memcpy(pm, pm_r, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm, rm_r, N_MDL * sizeof(mdl64_t));
            *ps = ps_r; *mvalid = (int)(stick + 1);
            free(pt_r); free(pm_r); free(rm_r);
            return r;
        }
        /* fall through to the full duel only when the single pass
           failed outright or this frame was picked for a retrain */
        free(pt_r); free(pm_r); free(rm_r);
    }

    /* Each candidate encode publishes this frame's leaf table.  Losers
       must not clobber the winner's program memory, so candidates write
       into a scratch preloaded with the PREVIOUS table and only the
       winner is committed back to ptab.  The same isolation applies to
       the carried entropy models: each candidate serializes from the
       carried base state and only the winner's post-state persists. */
    int ntx = (w + TILE_SZ - 1) / TILE_SZ;
    int nti = ntx * ((h + TILE_SZ - 1) / TILE_SZ);
    vop_t *pt_cur = NULL, *pt_win = NULL;
    mdl64_t *pm_cur = NULL, *rm_cur = NULL, *pm_win = NULL, *rm_win = NULL;
    pstate_t ps_cur, ps_win;
    if (stateful) {
        pt_cur = (vop_t *)malloc((size_t)nti * sizeof(vop_t));
        pt_win = (vop_t *)malloc((size_t)nti * sizeof(vop_t));
        pm_cur = (mdl64_t *)malloc(NPROG_MDL * sizeof(mdl64_t));
        rm_cur = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        pm_win = (mdl64_t *)malloc(NPROG_MDL * sizeof(mdl64_t));
        rm_win = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        if (!pt_cur || !pt_win || !pm_cur || !rm_cur || !pm_win || !rm_win) {
            free(pt_cur); free(pt_win);
            free(pm_cur); free(rm_cur); free(pm_win); free(rm_win);
            return -1;
        }
        memcpy(pt_cur, ptab, (size_t)nti * sizeof(vop_t));
        memcpy(pm_cur, pm, NPROG_MDL * sizeof(mdl64_t));
        memcpy(rm_cur, rm, N_MDL * sizeof(mdl64_t));
        ps_cur = *ps;
    }
    /* stateless duel reorder: the trained-W pass nearly always wins;
       run it first into scratch, then let the no-W pass chase it under
       an abort bound - when W wins, the no-W pass dies early */
    /* stateless duel, reordered: train W first - the residual ratio
       (llp vs raw spatial) is a strong predictor of which pass wins.
       Clear W win -> skip the no-W pass entirely; clear W loss -> skip
       the W encode; only the grey zone pays for both encodes. */
    int16_t Wp[N_NNW * N_NNC];
    int32_t lb0 = -1;
    uint8_t *tmp0 = NULL;
    double wr = 1.0;
    if (!stateful && (moved || !wvalid || !*wvalid)
        && train_llp(prev, curr, w, h, luma, Wp, qs, &wr)
        && wr <= 1.05) {
        tmp0 = (uint8_t *)malloc((size_t)out_cap);
        if (tmp0)
            lb0 = enc_plane_core(prev, curr, w, h, luma, Wp, tmp0,
                                 out_cap, 1, 1, NULL,
                                 NULL, NULL, pm_cur, rm_cur,
                                 0, NULL, NULL, yoff, hfull, 0);
    }
    int32_t la;
    if (lb0 > 0 && wr < 0.95) {
        la = 0x3FFFFFFF;            /* W clearly better - skip no-W pass */
        if (ZE(zrs)) fprintf(stderr, "=== candA skipped wr=%.3f ===\n", wr);
    } else {
        if (ZE(zrs)) fprintf(stderr, "=== candA noW ===\n");
        la = enc_plane_core(prev, curr, w, h, luma, NULL,
                            out, out_cap, 1, 1, NULL,
                            pt_cur, NULL, pm_cur, rm_cur,
                            stateful ? *mvalid : 0,
                            stateful ? &ps_cur : NULL, NULL, yoff, hfull,
                            lb0 > 4 ? lb0 : 0);
    }
    if (lb0 > 0 && (la <= 0 || lb0 <= la)) {
        memcpy(out, tmp0, (size_t)lb0);
        free(tmp0);
        free(pt_cur); free(pt_win);
        free(pm_cur); free(rm_cur); free(pm_win); free(rm_win);
        return lb0;
    }
    free(tmp0);
    if (ZE(zrs)) fprintf(stderr, "=== candA done la=%d ===\n", la);
    if (la > 0 && stateful) {
        memcpy(pt_win, pt_cur, (size_t)nti * sizeof(vop_t));
        memcpy(pm_win, pm_cur, NPROG_MDL * sizeof(mdl64_t));
        memcpy(rm_win, rm_cur, N_MDL * sizeof(mdl64_t));
        ps_win = ps_cur;
    }
    if (la <= 4) {          /* failure or all-RUN: nothing to predict */
        if (la > 0 && stateful) {
            memcpy(ptab, pt_win, (size_t)nti * sizeof(vop_t));
            memcpy(pm, pm_win, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm, rm_win, N_MDL * sizeof(mdl64_t));
            *ps = ps_win;
        }
        free(pt_cur); free(pt_win);
        free(pm_cur); free(rm_cur); free(pm_win); free(rm_win);
        return la;
    }
    uint8_t *tmp = (uint8_t *)malloc((size_t)out_cap);
    if (!tmp) {
        if (stateful) {
            memcpy(ptab, pt_win, (size_t)nti * sizeof(vop_t));
            memcpy(pm, pm_win, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm, rm_win, N_MDL * sizeof(mdl64_t));
            *ps = ps_win;
            *mvalid = 1;
        }
        free(pt_cur); free(pt_win);
        free(pm_cur); free(rm_cur); free(pm_win); free(rm_win);
        return la;
    }

    /* stateful: the previous frame's table may still win - reusing it
       emits a WSAME header bit instead of 144 delta records */
    if (stateful && *wvalid && !ZE(znwr)) {
        memcpy(pt_cur, ptab, (size_t)nti * sizeof(vop_t));
        memcpy(pm_cur, pm, NPROG_MDL * sizeof(mdl64_t));
        memcpy(rm_cur, rm, N_MDL * sizeof(mdl64_t));
        ps_cur = *ps;
        if (ZE(zrs)) fprintf(stderr, "=== candR wsame ===\n");
        int32_t lr = enc_plane_core(prev, curr, w, h, luma, Wprev, tmp,
                                    out_cap, 1, 1, NULL,
                                    pt_cur, Wprev, pm_cur, rm_cur,
                                    *mvalid, &ps_cur, NULL, yoff, hfull,
                                    la > 4 ? la : 0);
        if (ZE(zrs)) fprintf(stderr, "=== candR done lr=%d ===\n", lr);
        if (lr > 0 && lr < la) {
            if (ZE(zwin)) fprintf(stderr, "WIN wsame\n");
            memcpy(out, tmp, (size_t)lr);
            memcpy(ptab, pt_cur, (size_t)nti * sizeof(vop_t));
            memcpy(pm, pm_cur, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm, rm_cur, N_MDL * sizeof(mdl64_t));
            *ps = ps_cur;
            *mvalid = 1;
            free(tmp); free(pt_cur); free(pt_win);
            free(pm_cur); free(rm_cur); free(pm_win); free(rm_win);
            return lr;      /* Wprev stays valid for the next frame */
        }
    }
    /* cold fallback: a shifted distribution can leave the carried models
       badly mispriced (scene change, content swap), and the lossless
       duel above only ever prices warm streams.  One fresh-seeded pass
       keeps the stream honest, mirroring the lossy duel; PHDR_FRESH
       tells the decoder to re-seed, and the winner's post-state is what
       carries forward either way.  Runs before candB so a cold win can
       never strand a stale Wprev.  Only worth its encode cost when the
       probe says the frame actually moved. */
    if (stateful && *mvalid && churned) {
        vop_t *pt_c = (vop_t *)malloc((size_t)nti * sizeof(vop_t));
        mdl64_t *pm_c = (mdl64_t *)malloc(NPROG_MDL * sizeof(mdl64_t));
        mdl64_t *rm_c = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
        uint8_t *tc = (uint8_t *)malloc((size_t)out_cap);
        if (pt_c && pm_c && rm_c && tc) {
            pstate_t ps_c = *ps;
            memcpy(pt_c, ptab, (size_t)nti * sizeof(vop_t));
            int32_t lc = enc_plane_core(prev, curr, w, h, luma, NULL,
                                        tc, out_cap, 1, 1, NULL,
                                        pt_c, NULL, pm_c, rm_c, 0,
                                        &ps_c, NULL, yoff, hfull,
                                        la > 4 ? la : 0);
            if (lc > 0 && lc < la) {
                if (ZE(zwin)) fprintf(stderr, "WIN cold\n");
                memcpy(out, tc, (size_t)lc); la = lc;
                wr_u32le(out, rd_u32le(out) | PHDR_FRESH);
                memcpy(pt_win, pt_c, (size_t)nti * sizeof(vop_t));
                memcpy(pm_win, pm_c, NPROG_MDL * sizeof(mdl64_t));
                memcpy(rm_win, rm_c, N_MDL * sizeof(mdl64_t));
                ps_win = ps_c;
            }
        }
        free(pt_c); free(pm_c); free(rm_c); free(tc);
    }
    /* retrain only when the probe fired OR there's no carried table to
       reuse - stateless callers (no Wprev bookkeeping) and keyframes
       always retrain as before. */
    int retrain = moved || !wvalid || !*wvalid;
    int16_t W[N_NNW * N_NNC];
    if (!stateful
        || !retrain || !train_llp(prev, curr, w, h, luma, W, qs, NULL)) {
        if (stateful) {
            memcpy(ptab, pt_win, (size_t)nti * sizeof(vop_t));
            memcpy(pm, pm_win, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm, rm_win, N_MDL * sizeof(mdl64_t));
            *ps = ps_win;
            *mvalid = 1;
        }
        free(tmp); free(pt_cur); free(pt_win);
        free(pm_cur); free(rm_cur); free(pm_win); free(rm_win);
        return la;
    }
    if (stateful) {
        memcpy(pt_cur, ptab, (size_t)nti * sizeof(vop_t));
        memcpy(pm_cur, pm, NPROG_MDL * sizeof(mdl64_t));
        memcpy(rm_cur, rm, N_MDL * sizeof(mdl64_t));
        ps_cur = *ps;
    }
    if (ZE(zwd) && Wprev && *wvalid) {
        int mx = 0; for (int i = 0; i < N_NNW * N_NNC; i++) {
            int d = W[i] - Wprev[i]; if (d < 0) d = -d; if (d > mx) mx = d;
        }
        fprintf(stderr, "WDRIFT max=%d\n", mx);
    }
    if (ZE(zrs)) fprintf(stderr, "=== candB newW ===\n");
    int32_t lb = enc_plane_core(prev, curr, w, h, luma, W, tmp,
                                out_cap, 1, 1, NULL,
                                stateful ? pt_cur : NULL,
                                (stateful && *wvalid) ? Wprev : NULL,
                                pm_cur, rm_cur,
                                stateful ? *mvalid : 0,
                                stateful ? &ps_cur : NULL, NULL, yoff, hfull,
                                la > 4 ? la : 0);
    if (ZE(zrs)) fprintf(stderr, "=== candB done lb=%d ===\n", lb);
    if (lb > 0 && lb < la) {
        if (ZE(zwin)) fprintf(stderr, "WIN newW\n");
        memcpy(out, tmp, (size_t)lb); la = lb;
        if (stateful) {
            memcpy(pt_win, pt_cur, (size_t)nti * sizeof(vop_t));
            memcpy(pm_win, pm_cur, NPROG_MDL * sizeof(mdl64_t));
            memcpy(rm_win, rm_cur, N_MDL * sizeof(mdl64_t));
            ps_win = ps_cur;
            memcpy(Wprev, W, sizeof(W)); *wvalid = 1;
        }
    }
    if (stateful) {
        memcpy(ptab, pt_win, (size_t)nti * sizeof(vop_t));
        memcpy(pm, pm_win, NPROG_MDL * sizeof(mdl64_t));
        memcpy(rm, rm_win, N_MDL * sizeof(mdl64_t));
        *ps = ps_win;
        *mvalid = 1;
    }
    free(tmp); free(pt_cur); free(pt_win);
    free(pm_cur); free(rm_cur); free(pm_win); free(rm_win);
    return la;
}

/* stripe framing: a plane stream may be split into ns independent
   substreams, each a normal [phdr|prog|res] stream covering a band of
   whole tile rows.  Layout:
       [u8 ns][u32 off x ns][stripe streams]
   offsets are relative to the start of the plane stream.
   Every stripe warms from the SAME carried state (the frame's pm/rm/
   ps/Wprev); the decoder mirrors this by giving each stripe a private
   copy of the carried state.  Only the LAST stripe's post-state becomes
   the next frame's carried state.  Intra-copy candidates are bounded
   in stripe-local coords, so they can never reach across stripe starts;
   the top edge of each stripe reuses the plane-top border predictors
   (top == NULL), which both sides derive identically. */
static int stripe_bounds(int nty, int ns, int s, int h, int *r0, int *r1) {
    int sht = (nty + ns - 1) / ns;
    int a = s * sht * TILE_SZ;
    int b = a + sht * TILE_SZ;
    if (a >= h) return 0;
    if (b > h) b = h;
    *r0 = a; *r1 = b;
    return 1;
}

/* ---- encoder stripe fan-out ------------------------------------------
   Stripes are independent streams: each worker clones the frame-entry
   state, encodes into private scratch, and the main thread compacts the
   streams in order after the join.  Only the LAST stripe's post-state is
   committed to the carried slots - identical to the sequential version
   where the last stripe wrote them in place. */
typedef struct {
    const int16_t *pv, *cv, *lv;
    int w, sh, qs, qs_hi;
    int16_t *rv;
    vop_t *pt;
    int stateful;
    mdl64_t *pm_s, *rm_s;
    int16_t *Wp_s;
    int wv_s, mv_s;
    pstate_t ps_s;
    const int16_t *Wprev;
    const mdl64_t *pm, *rm;
    const int *wvalid, *mvalid;
    const pstate_t *ps;
    uint8_t *scratch;
    int32_t cap, len;
    int yoff, hfull;
} esjob_t;

static int enc_spawn(pthread_t *t, void *(*fn)(void *), void *a);

static void *enc_stripe_worker(void *arg) {
    esjob_t *j = (esjob_t *)arg;
    if (j->stateful) {
        memcpy(j->pm_s, j->pm, NPROG_MDL * sizeof(mdl64_t));
        memcpy(j->rm_s, j->rm, N_MDL * sizeof(mdl64_t));
        memcpy(j->Wp_s, j->Wprev, N_NNW * N_NNC * sizeof(int16_t));
        j->wv_s = *j->wvalid; j->mv_s = *j->mvalid; j->ps_s = *j->ps;
        j->len = enc_plane_stream_v3_g(j->pv, j->cv, j->w, j->sh, j->lv,
                                       j->scratch, j->cap,
                                       j->qs, j->qs_hi, j->rv, j->pt,
                                       j->Wp_s, &j->wv_s,
                                       j->pm_s, j->rm_s, &j->mv_s,
                                       &j->ps_s, j->yoff, j->hfull);
    } else {
        j->len = enc_plane_stream_v3_g(j->pv, j->cv, j->w, j->sh, j->lv,
                                       j->scratch, j->cap,
                                       j->qs, j->qs_hi, j->rv, NULL,
                                       NULL, NULL, NULL, NULL, NULL,
                                       NULL, j->yoff, j->hfull);
    }
    return NULL;
}

static int32_t enc_plane_stripe_frame(const int16_t *prev,
                                      const int16_t *curr,
                                      int w, int h, const int16_t *luma,
                                      uint8_t *out, int32_t out_cap,
                                      int qs, int qs_hi, int16_t *recon,
                                      vop_t *ptab, int16_t *Wprev,
                                      int *wvalid,
                                      mdl64_t *pm, mdl64_t *rm,
                                      int *mvalid, pstate_t *ps,
                                      int ns)
{
    int ntx = (w + TILE_SZ - 1) / TILE_SZ;
    int nty = (h + TILE_SZ - 1) / TILE_SZ;
    int sht = (nty + ns - 1) / ns;
    if (sht < 1) sht = 1;
    int nsr = (nty + sht - 1) / sht;          /* real, non-empty count */
    if (nsr < 1) nsr = 1;
    int32_t hd = 1 + 4 * (int32_t)nsr;
    if (out_cap < hd + 4 * (int32_t)nsr) return -1;
    int stateful = (ptab != NULL && pm && rm && mvalid && ps &&
                    Wprev && wvalid);
    out[0] = (uint8_t)nsr;
    if (nsr == 1) {
        /* single stripe: encode straight into the output */
        int32_t r = enc_plane_stream_v3_g(prev, curr, w, h, luma,
                                          out + hd, out_cap - hd,
                                          qs, qs_hi, recon, ptab,
                                          Wprev, wvalid,
                                          pm, rm, mvalid, ps, 0, h);
        if (r < 0) return -1;
        wr_u32le(out + 1, (uint32_t)hd);
        return hd + r;
    }

    /* per-stripe scratch: raw pixels x 3 + slack; a stripe that still
       overflows re-runs inline into the output with the full remaining
       capacity (streams are deterministic against the cloned entry
       state, so the retry reproduces the same program). */
    esjob_t *jobs = (esjob_t *)malloc((size_t)nsr * sizeof(esjob_t));
    if (!jobs) return -1;
    memset(jobs, 0, (size_t)nsr * sizeof(esjob_t));
    int alloc_fail = 0;
    for (int s = 0; s < nsr; s++) {
        int r0, r1;
        stripe_bounds(nty, nsr, s, h, &r0, &r1);
        esjob_t *j = &jobs[s];
        j->sh = r1 - r0;
        j->cap = (int32_t)((size_t)w * j->sh * 3 + 65536);
        if (j->cap > out_cap - hd) j->cap = out_cap - hd;
        j->scratch = (uint8_t *)malloc((size_t)j->cap);
        if (!j->scratch) { alloc_fail = 1; break; }
        if (stateful) {
            j->pm_s = (mdl64_t *)malloc(NPROG_MDL * sizeof(mdl64_t));
            j->rm_s = (mdl64_t *)malloc(N_MDL * sizeof(mdl64_t));
            j->Wp_s = (int16_t *)malloc(N_NNW * N_NNC * sizeof(int16_t));
            if (!j->pm_s || !j->rm_s || !j->Wp_s) { alloc_fail = 1; break; }
        }
        j->pv = prev ? prev + (size_t)r0 * w : NULL;
        j->cv = curr + (size_t)r0 * w;
        j->lv = luma ? luma + (size_t)r0 * w : NULL;
        j->rv = recon ? recon + (size_t)r0 * w : NULL;
        j->pt = ptab ? ptab + (r0 / TILE_SZ) * ntx : NULL;
        j->w = w; j->qs = qs; j->qs_hi = qs_hi;
        j->yoff = r0; j->hfull = h;
        j->stateful = stateful;
        j->Wprev = Wprev; j->pm = pm; j->rm = rm;
        j->wvalid = wvalid; j->mvalid = mvalid; j->ps = ps;
        j->len = -1;
    }
    pthread_t th[12];
    int nth = 0;
    if (!alloc_fail) {
        for (int s = 0; s < nsr; s++) {
            if (nth < 12 &&
                enc_spawn(&th[nth], enc_stripe_worker, &jobs[s]))
                nth++;
            else
                enc_stripe_worker(&jobs[s]);   /* fallback: run inline */
        }
        for (int i = 0; i < nth; i++) pthread_join(th[i], NULL);
    }
    uint8_t *op = out + hd;
    int fail = alloc_fail;
    for (int s = 0; s < nsr && !fail; s++) {
        esjob_t *j = &jobs[s];
        if (j->len < 0 && !alloc_fail) {
            /* scratch overflowed: re-run this stripe inline into the
               output with the full remaining budget */
            int r0, r1;
            stripe_bounds(nty, nsr, s, h, &r0, &r1);
            if (stateful) {
                memcpy(j->pm_s, pm, NPROG_MDL * sizeof(mdl64_t));
                memcpy(j->rm_s, rm, N_MDL * sizeof(mdl64_t));
                memcpy(j->Wp_s, Wprev, N_NNW * N_NNC * sizeof(int16_t));
                j->wv_s = *wvalid; j->mv_s = *mvalid; j->ps_s = *ps;
                j->len = enc_plane_stream_v3_g(
                    j->pv, j->cv, j->w, j->sh, j->lv,
                    op, out_cap - (int32_t)(op - out),
                    qs, qs_hi, j->rv, j->pt,
                    j->Wp_s, &j->wv_s, j->pm_s, j->rm_s, &j->mv_s,
                    &j->ps_s, j->yoff, j->hfull);
            } else {
                j->len = enc_plane_stream_v3_g(
                    j->pv, j->cv, j->w, j->sh, j->lv,
                    op, out_cap - (int32_t)(op - out),
                    qs, qs_hi, j->rv, NULL, NULL, NULL,
                    NULL, NULL, NULL, NULL, j->yoff, j->hfull);
            }
            if (j->len < 0) { fail = 1; break; }
        }
        wr_u32le(out + 1 + 4 * s, (uint32_t)(op - out));
        if (op != j->scratch)
            memcpy(op, j->scratch, (size_t)j->len);
        op += j->len;
    }
    /* commit the last stripe's post-state to the carried slots */
    if (!fail && stateful) {
        esjob_t *j = &jobs[nsr - 1];
        memcpy(pm, j->pm_s, NPROG_MDL * sizeof(mdl64_t));
        memcpy(rm, j->rm_s, N_MDL * sizeof(mdl64_t));
        memcpy(Wprev, j->Wp_s, N_NNW * N_NNC * sizeof(int16_t));
        *wvalid = j->wv_s; *mvalid = j->mv_s; *ps = j->ps_s;
    }
    for (int s = 0; s < nsr; s++) {
        free(jobs[s].scratch);
        free(jobs[s].pm_s); free(jobs[s].rm_s); free(jobs[s].Wp_s);
    }
    free(jobs);
    return fail ? -1 : (int32_t)(op - out);
}

/* chroma-420 wrapper: a chroma plane (luma tap present) that is exactly
   2x2 block-constant is coded on its quarter-resolution grid - the prog
   and res streams, the op tables and the decode work all shrink 4x.
   The decoder replays at w2 x h2 and replicates back to full res.
   nstripes > 1 emits the stripe-framed layout instead; a c420 plane is
   demoted to a single framed stripe so its whole-plane luma decimate
   stays valid. */
static int32_t enc_plane_stream_v3_one(const int16_t *prev,
                                   const int16_t *curr,
                                   int w, int h, const int16_t *luma,
                                   uint8_t *out, int32_t out_cap,
                                   int qs, int qs_hi, int16_t *recon,
                                   vop_t *ptab, int16_t *Wprev,
                                   int *wvalid,
                                   mdl64_t *pm, mdl64_t *rm, int *mvalid,
                                   pstate_t *ps)
{
    if (luma && !(w & 1) && !(h & 1) && w >= 4 && h >= 4 &&
        plane_is_420(curr, w, h)) {
        int w2 = w >> 1, h2 = h >> 1;
        size_t n2 = (size_t)w2 * h2;
        int16_t *c2 = (int16_t *)malloc(n2 * sizeof(int16_t));
        int16_t *p2 = prev ? (int16_t *)malloc(n2 * sizeof(int16_t)) : NULL;
        int16_t *l2 = (int16_t *)malloc(n2 * sizeof(int16_t));
        int16_t *r2 = recon ? (int16_t *)malloc(n2 * sizeof(int16_t)) : NULL;
        int32_t r = -1;
        if (c2 && l2 && (!prev || p2) && (!recon || r2)) {
            plane_dn2(c2, curr, w, h);
            if (prev) plane_dn2(p2, prev, w, h);
            plane_dn2(l2, luma, w, h);
            r = enc_plane_stream_v3_g(p2, c2, w2, h2, l2,
                                      out, out_cap, qs, qs_hi, r2,
                                      ptab, Wprev, wvalid,
                                      pm, rm, mvalid, ps, 0, h2);
            if (r >= 4) {
                uint32_t hdr = rd_u32le(out);
                if ((hdr & PHDR_PLEN) < PHDR_C420) {
                    wr_u32le(out, hdr | PHDR_C420);
                    if (recon) plane_up2(recon, r2, w, h);
                } else {
                    /* prog blob too long to carry the flag: fall back to
                       the full-resolution program */
                    r = enc_plane_stream_v3_g(prev, curr, w, h, luma,
                                              out, out_cap, qs, qs_hi,
                                              recon, ptab, Wprev, wvalid,
                                              pm, rm, mvalid, ps, 0, h);
                }
            }
        }
        free(c2); free(p2); free(l2); free(r2);
        return r;
    }
    return enc_plane_stream_v3_g(prev, curr, w, h, luma,
                                 out, out_cap, qs, qs_hi, recon,
                                 ptab, Wprev, wvalid,
                                 pm, rm, mvalid, ps, 0, h);
}

static int32_t enc_plane_stream_v3(const int16_t *prev,
                                   const int16_t *curr,
                                   int w, int h, const int16_t *luma,
                                   uint8_t *out, int32_t out_cap,
                                   int qs, int qs_hi, int16_t *recon,
                                   vop_t *ptab, int16_t *Wprev,
                                   int *wvalid,
                                   mdl64_t *pm, mdl64_t *rm, int *mvalid,
                                   pstate_t *ps, int nstripes)
{
    if (nstripes == -1) {
        /* framed single stripe: keeps the [ns][offs] plane framing
           uniform when a sibling plane is striped */
        int32_t r = enc_plane_stream_v3_one(prev, curr, w, h, luma,
                                            out + 5, out_cap - 5,
                                            qs, qs_hi, recon,
                                            ptab, Wprev, wvalid,
                                            pm, rm, mvalid, ps);
        if (r < 0) return -1;
        out[0] = 1;
        wr_u32le(out + 1, 5);
        return 5 + r;
    }
    if (nstripes <= 1)
        return enc_plane_stream_v3_one(prev, curr, w, h, luma,
                                       out, out_cap, qs, qs_hi, recon,
                                       ptab, Wprev, wvalid,
                                       pm, rm, mvalid, ps);
    if (luma && !(w & 1) && !(h & 1) && w >= 4 && h >= 4 &&
        plane_is_420(curr, w, h)) {
        /* c420 chroma stays a single stream (the luma decimate spans the
           whole frame); frame it as a one-stripe stream so the decoder's
           plane framing stays uniform */
        int32_t r = enc_plane_stream_v3_one(prev, curr, w, h, luma,
                                            out + 5, out_cap - 5,
                                            qs, qs_hi, recon,
                                            ptab, Wprev, wvalid,
                                            pm, rm, mvalid, ps);
        if (r < 0) return -1;
        out[0] = 1;
        wr_u32le(out + 1, 5);
        return 5 + r;
    }
    return enc_plane_stripe_frame(prev, curr, w, h, luma,
                                  out, out_cap, qs, qs_hi, recon,
                                  ptab, Wprev, wvalid,
                                  pm, rm, mvalid, ps, nstripes);
}

/* pull one op from the program stream and execute it over the given
   region; OP_SPLIT recurses into four half-size children. */
/* decode-side mirror of sim_pred_tile: pulls records from the res
   stream and writes pixels. MV components are clamped on use. */
static void dec_pred_tile(const int16_t *prev, int16_t *curr,
                          int w, int h,
                          int tx, int ty, int tw, int th,
                          int dx, int dy, int has_prev,
                          pstate_t *S, zrd_t *d, const int16_t *W,
                          const int16_t *luma, int qs, int lmode,
                          int sy0, int sy1)
{
    int erq = 0;   /* previous pixel's decoded residual (LLP e tap) */
    if (ZE(zpx2)) fprintf(stderr, "DPT @%d,%d %dx%d enter tpull=%ld hp=%d\n",
            tx, ty, tw, th, d->tpull, has_prev);
    for (int y = ty; y < ty + th; y++) {
        int16_t *row = curr + (size_t)y * w;
        const int16_t *top = y > 0 ? curr + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? curr + (size_t)(y - 2) * w : NULL;
        int sy = y + dy;
        if (sy < sy0) sy = sy0; else if (sy >= sy1) sy = sy1 - 1;
        const int16_t *prow = has_prev ? prev + (size_t)sy * w : NULL;
        int x = tx, xend = tx + tw;
        int pmatch = 0;         /* last flag value - runs cluster */
        int zleft = 0;          /* remaining pixels of a flag-5 run */
        while (x < xend) {
            int a = x > 0 ? row[x - 1] : (top ? top[x] : 0);
            int aa = x > 1 ? row[x - 2] : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            /* top-right is causal only inside the tile on non-first rows
               (same-band tiles decode later); the first row may use the
               whole previous band, which is already decoded. */
            int d2;
            if (!top)         d2 = b;
            else if (y == ty) {
                int rl = (ty % TILE_SZ == 0) ? w
                       : ((tx & ~(2 * tw - 1)) + 2 * tw);
                if (rl > w) rl = w;
                d2 = (x + 1 < rl) ? top[x + 1] : b;
            } else            d2 = (x + 1 < xend) ? top[x + 1] : b;
            /* flag/run/residual slot: same At-history bucket the
               encoder derives from this ctx - no bits transmitted */
            int q1 = q_grad(d2 - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);
            /* floor(At/N) <= T  <=>  At < (T+1)*N  for At,N >= 0 -
               the three bucket compares replace a per-pixel divide */
            int32_t atv = S->At[ctx];
            int64_t nn = S->N[ctx] > 0 ? S->N[ctx] : 1;
            int gb_a = atv < (int64_t)(qs + 1) * nn ? 0 :
                       atv < (int64_t)(qs * 4 + 1) * nn ? 1 :
                       atv < (int64_t)(qs * 16 + 1) * nn ? 2 : 3;
            int gb = gb_a + grad_bucket(a, b, c, d2, qs);
            if (gb > 3) gb = 3;
            int fb = pmatch * 4 + grad_bucket(a, b, c, d2, qs);

            int mflag = 0;
            if (zleft) {
                mflag = 5;
                zleft--;
                pmatch = 4;
            } else if ((has_prev && lmode == 0) || lmode == 3 || lmode == 5) {
                mflag = zrd_sym(d, (uint32_t)(FLAG_MDL_IDX + fb));
                if (mflag == 1 && (lmode == 3 || (lmode == 5 && !has_prev))) {
                    if (ZE(zerr)) fprintf(stderr,
                        "ERR flag1 under intra-flags @%d,%d\n", x, y);
                    d->err = 1; return;
                }
                if (mflag == 5) {
                    int run = (int)zrd_len_sym(d, (uint32_t)(RUN_MDL_IDX + pmatch * 4 + run_bucket(S))) + 1;
                    if (run > xend - x) run = xend - x;
                    if (run < 2) {
                        if (ZE(zerr)) fprintf(stderr,
                            "ERR flag5 run=%d @%d,%d\n", run, x, y);
                        d->err = 1; return;
                    }
                    update_run(S, run);
                    zleft = run - 1;
                    pmatch = 4;
                } else if (mflag && mflag != 4) {
                    int run = (int)zrd_len_sym(d, (uint32_t)(RUN_MDL_IDX + pmatch * 4 + run_bucket(S))) + 1;
                    if (run > xend - x) run = xend - x;
                    if (mflag > 3 || (mflag == 3 && !top)) { if (ZE(zerr)) fprintf(stderr,"ERR mflag=%d top=%p @%d,%d tpull=%ld\n",mflag,(void*)top,x,y,d->tpull); d->err = 1; return; }
                    update_run(S, run);
                    pmatch = mflag;
                    erq = 0;
                    if (mflag == 1) {
                        if (!prow) { d->err = 1; return; }
                        for (int i = 0; i < run; i++) {
                            int sx = x + i + dx;
                            if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
                            row[x + i] = prow[sx];
                        }
                    }
                    else if (mflag == 2)
                        for (int i = 0; i < run; i++) row[x + i] = (int16_t)a;
                    else
                        for (int i = 0; i < run; i++) row[x + i] = top[x + i];
                    x += run;
                    continue;
                }
                if (mflag != 5) pmatch = mflag;   /* 0 or 4; 5 already set 4 */
            }

            int spreds[NSPAT];
            spat_preds(a, b, c, d2, aa, uu, spreds);

            int has_t = has_prev;
            int mode = 0;
            if (!W) {
                int32_t best_A = S->As[ctx][0];
                for (int p = 1; p < NSPAT; p++) {
                    if (S->As[ctx][p] < best_A) { best_A = S->As[ctx][p]; mode = p; }
                }
                if (has_t && S->At[ctx] < best_A) mode = NSPAT;
            }
            int dm = W ? NSPAT : mode;
            if (ZE(zpx)) fprintf(stderr, "DPX @%d,%d px=(%d,%d) a=%d b=%d c=%d d2=%d ctx=%d mode=%d tp=%ld\n",
                    tx, ty, x, y, a, b, c, d2, ctx, dm, d->tpull);
            int r = 0;
            if (lmode != 2 && mflag != 4 && mflag != 5) {
                uint32_t u = zrd_len_sym(d, pred_res_midx(dm, gb, ctx, lmode == 4 || lmode == 5));
                r = zigzag_dec(u);
                if (flip) r = -r;
            }

            int sx = x + dx;
            if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
            int val;
            if (W) {
                int pl = 0, pr = 0, pu = 0, pd = 0;
                /* intra refs (prev == curr) make the right/down taps
                   non-causal - keep them zero, matching the encoder */
                if (has_t && prev != curr) {
                    int py = y + dy;
                    if (py < 0) py = 0; else if (py >= h) py = h - 1;
                    pl = prev[(size_t)py * w + (sx ? sx - 1 : 0)];
                    pr = prev[(size_t)py * w + (sx < w - 1 ? sx + 1 : w - 1)];
                    pu = prev[(size_t)(py ? py - 1 : 0) * w + sx];
                    pd = prev[(size_t)(py < h - 1 ? py + 1 : h - 1) * w + sx];
                }
                val = llp_predict(W, a, b, c, d2,
                                  has_t ? prow[sx] : 0,
                                  luma ? luma[(size_t)y * w + x] : 0,
                                  aa, uu, pl, pr, pu, pd,
                                  qs > 5 ? 0 : erq) + r * qs;
            }
            else if (mode == NSPAT) val = prow[sx] + r * qs;
            else               val = spreds[mode] + r * qs;
            row[x] = (int16_t)val;

            int rs[NSPAT];
            for (int p = 0; p < NSPAT; p++) rs[p] = val - spreds[p];
            int rt = has_t ? (val - prow[sx]) : 0;
            if (flip) {
                for (int p = 0; p < NSPAT; p++) rs[p] = -rs[p];
                rt = -rt;
            }
            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) abs_s[p] = rs[p] < 0 ? -rs[p] : rs[p];
            int abs_t = has_t ? (rt < 0 ? -rt : rt) : abs_s[0];
            if (mflag != 5) {
                update_ctx(S, ctx, abs_s, abs_t);
                erq = r * qs;
            } else {
                erq = 0;
            }
            x++;
        }
    }
}

/* decoder-side OP_NOISE: identical causal loop, but the residual is
   synthesized deterministically instead of pulled from the stream */
static void dec_noise_tile(const int16_t *prev, int16_t *curr,
                           int w, int h,
                           int tx, int ty, int tw, int th,
                           int has_prev, pstate_t *S, const int16_t *W,
                           const int16_t *luma, int sig, uint32_t seed)
{
    (void)h;
    int erq = 0;
    uint64_t ctr = seed;
    for (int y = ty; y < ty + th; y++) {
        int16_t *row = curr + (size_t)y * w;
        const int16_t *top = y > 0 ? curr + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? curr + (size_t)(y - 2) * w : NULL;
        const int16_t *prow = has_prev ? prev + (size_t)y * w : NULL;
        for (int x = tx; x < tx + tw; x++) {
            int a = x > 0 ? row[x - 1] : (top ? top[x] : 0);
            int aa = x > 1 ? row[x - 2] : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d2;
            if (!top)         d2 = b;
            else if (y == ty) {
                int rl = (ty % TILE_SZ == 0) ? w
                       : ((tx & ~(2 * tw - 1)) + 2 * tw);
                if (rl > w) rl = w;
                d2 = (x + 1 < rl) ? top[x + 1] : b;
            } else            d2 = (x + 1 < tx + tw) ? top[x + 1] : b;

            int q1 = q_grad(d2 - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);

            int spreds[NSPAT];
            spat_preds(a, b, c, d2, aa, uu, spreds);
            int has_t = has_prev;
            int mode = 0;
            int32_t best_A = S->As[ctx][0];
            for (int p = 1; p < NSPAT; p++) {
                if (S->As[ctx][p] < best_A) { best_A = S->As[ctx][p]; mode = p; }
            }
            if (has_t && S->At[ctx] < best_A) {
                best_A = S->At[ctx]; mode = NSPAT;
            }

            uint64_t z = smix64(ctr++);
            int64_t n4 = (int64_t)(z & 0xFFFF) + (int64_t)((z >> 16) & 0xFFFF)
                       + (int64_t)((z >> 32) & 0xFFFF) + (int64_t)(z >> 48);
            int nz = (int)((n4 - 131070) * sig / 37838);

            int val;
            if (W) {
                int pl = 0, pr = 0, pu = 0, pd = 0;
                if (has_t) {
                    pl = prow[x ? x - 1 : 0];
                    pr = prow[x + 1 < w ? x + 1 : w - 1];
                    pu = prev[(size_t)(y ? y - 1 : 0) * w + x];
                    pd = prev[(size_t)(y < h - 1 ? y + 1 : h - 1) * w + x];
                }
                val = llp_predict(W, a, b, c, d2,
                                  has_t ? prow[x] : 0,
                                  luma ? luma[(size_t)y * w + x] : 0,
                                  aa, uu, pl, pr, pu, pd, erq) + nz;
            }
            else if (mode == NSPAT) val = prow[x] + nz;
            else               val = spreds[mode] + nz;
            row[x] = (int16_t)val;
            erq = nz;

            int rs[NSPAT];
            for (int p = 0; p < NSPAT; p++) rs[p] = val - spreds[p];
            int rt = has_t ? (val - prow[x]) : 0;
            if (flip) {
                for (int p = 0; p < NSPAT; p++) rs[p] = -rs[p];
                rt = -rt;
            }
            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) abs_s[p] = rs[p] < 0 ? -rs[p] : rs[p];
            int abs_t = has_t ? (rt < 0 ? -rt : rt) : abs_s[0];
            update_ctx(S, ctx, abs_s, abs_t);
        }
    }
}

/* decoder-side OP_CFL: identical causal loop, prediction comes from
   the reconstructed luma plane via the streamed affine params. */
static void dec_cfl_tile(const int16_t *prev, int16_t *curr,
                         int w, int h,
                         int tx, int ty, int tw, int th,
                         int ca, int cb, int has_prev,
                         pstate_t *S, zrd_t *d, const int16_t *luma,
                         int qs)
{
    (void)h;
    if (ZE(zcfl)) fprintf(stderr, "DCFL @%d,%d %dx%d a=%d b=%d hp=%d tp=%ld\n",
                                tx, ty, tw, th, ca, cb, has_prev, d->tpull);
    for (int y = ty; y < ty + th; y++) {
        int16_t *row = curr + (size_t)y * w;
        const int16_t *top = y > 0 ? curr + (size_t)(y - 1) * w : NULL;
        const int16_t *top2 = y > 1 ? curr + (size_t)(y - 2) * w : NULL;
        const int16_t *prow = has_prev ? prev + (size_t)y * w : NULL;
        int x = tx, xend = tx + tw;
        int pmatch = 0;
        while (x < xend) {
            int a = x > 0 ? row[x - 1] : (top ? top[x] : 0);
            int aa = x > 1 ? row[x - 2] : a;
            int b = top ? top[x] : a;
            int uu = top2 ? top2[x] : b;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d2;
            if (!top)         d2 = b;
            else if (y == ty) {
                int rl = (ty % TILE_SZ == 0) ? w
                       : ((tx & ~(2 * tw - 1)) + 2 * tw);
                if (rl > w) rl = w;
                d2 = (x + 1 < rl) ? top[x + 1] : b;
            } else            d2 = (x + 1 < xend) ? top[x + 1] : b;
            int q1 = q_grad(d2 - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);
            /* floor(At/N) <= T  <=>  At < (T+1)*N  for At,N >= 0 -
               the three bucket compares replace a per-pixel divide */
            int32_t atv = S->At[ctx];
            int64_t nn = S->N[ctx] > 0 ? S->N[ctx] : 1;
            int gb_a = atv < (int64_t)(qs + 1) * nn ? 0 :
                       atv < (int64_t)(qs * 4 + 1) * nn ? 1 :
                       atv < (int64_t)(qs * 16 + 1) * nn ? 2 : 3;
            int gb = gb_a + grad_bucket(a, b, c, d2, qs);
            if (gb > 3) gb = 3;
            int fb = pmatch * 4 + grad_bucket(a, b, c, d2, qs);

            if (has_prev) {
                int can_run = zrd_sym(d, (uint32_t)(FLAG_MDL_IDX + fb));
                if (can_run) {
                    int run = (int)zrd_len_sym(d, (uint32_t)(RUN_MDL_IDX + pmatch * 4 + run_bucket(S))) + 1;
                    update_run(S, run);
                    pmatch = 1;
                    if (run > xend - x) run = xend - x;
                    for (int i = 0; i < run; i++) row[x + i] = prow[x + i];
                    x += run;
                    continue;
                }
                pmatch = 0;
            }

            int spreds[NSPAT];
            spat_preds(a, b, c, d2, aa, uu, spreds);

            uint32_t u = zrd_len_sym(d, CFL_MDL_IDX);
            if (ZE(zcfl)) fprintf(stderr, "DCFL px=(%d,%d) u=%d tp=%ld err=%d\n",
                                        x, y, u, d->tpull, d->err);
            int r = zigzag_dec(u);
            if (flip) r = -r;

            int lu = luma ? luma[(size_t)y * w + x] : 0;
            int val = ((ca * lu) >> 6) + cb + r * qs;
            row[x] = (int16_t)val;

            int rs[NSPAT];
            for (int p = 0; p < NSPAT; p++) rs[p] = val - spreds[p];
            int rt = has_prev ? (val - prow[x]) : 0;
            if (flip) {
                for (int p = 0; p < NSPAT; p++) rs[p] = -rs[p];
                rt = -rt;
            }
            int abs_s[NSPAT];
            for (int p = 0; p < NSPAT; p++) abs_s[p] = rs[p] < 0 ? -rs[p] : rs[p];
            int abs_t = has_prev ? (rt < 0 ? -rt : rt) : abs_s[0];
            update_ctx(S, ctx, abs_s, abs_t);
            x++;
        }
    }
}

/* cross-plane progress bar for parallel decode: the luma thread
   publishes completed pixel rows; chroma leaves that can touch luma
   (CfL / PALT / NN luma tap) spin until the rows they read are ready.
   rows_done < 0 signals a producer failure so waiters abort instead of
   hanging. */
typedef struct { _Atomic int rows_done; } dec_pb_t;

static int pb_wait(const dec_pb_t *pb, int need) {
    for (;;) {
        int r = atomic_load_explicit(&pb->rows_done, memory_order_acquire);
        if (r < 0) return -1;
        if (r >= need) return 0;
        sched_yield();
    }
}

/* Implicit-last index runs into dst[0..n). 0 on success. */
static int dec_fill_runs(zrd_t *pd, uint8_t *dst, int n, int k)
{
    int nruns = (int)zrd_len_sym(pd, PM_PALN) + 1;
    if (n < 1 || nruns < 1 || nruns > n) return -1;
    int x = 0;
    for (int i = 0; i < nruns; i++) {
        int idx = (int)zrd_len_sym(pd, PM_PALI);
        if (idx < 0) idx = 0;
        if (idx >= k) idx = k - 1;
        int r;
        if (i + 1 == nruns)
            r = n - x;
        else {
            r = (int)zrd_len_sym(pd, PM_RRL) + 1;
            if (r < 1 || r >= n - x) return -1;
        }
        if (r < 1) return -1;
        for (int t = 0; t < r; t++) dst[x++] = (uint8_t)idx;
    }
    return x == n ? 0 : -1;
}

static int dec_leaf_op(int op, const int16_t *prev, int16_t *curr,
                        int w, int h, const int16_t *luma,
                        int tx, int ty, int tw, int th,
                        int has_prev, pstate_t *S, zrd_t *pd, zrd_t *rd,
                        const int16_t *Wp, int *last_tmv, int *last_imv,
                        int qs, uint32_t nseed, vop_t *vops, int ntx,
                        const vop_t *ovr_in, int sf, const vop_t *slot,
                        int exact_in, const dec_pb_t *luma_pb,
                        int yoff, int hfull)
{
    if (luma && luma_pb && pb_wait(luma_pb, ty + th) < 0) return -1;
    long rs0 = rd ? (long)rd->tpull : 0;
    vop_t st; const vop_t *ovr = NULL; int exact = exact_in;
    vop_t *cell = vops ? &vops[vcell_idx(ntx, tx, tw)] : NULL;
    if (op == OP_VRPT || op == OP_PALX) {
        if (!cell || cell->rw != tw || cell->rx != tx ||
            cell->ry + cell->rw != ty)
            return -1;
        if (op == OP_PALX && cell->op != OP_PAL && cell->op != OP_PALR &&
            cell->op != OP_PALT && cell->op != OP_PALQ &&
            cell->op != OP_PALZ && cell->op != OP_PALU &&
            cell->op != OP_PALC && cell->op != OP_PALD)
            return -1;
        st = *cell;
        if (op == OP_PALX) exact = 1;
        op = st.op; ovr = &st;
    } else if (ovr_in) {
        /* OP_PSAME: replay the stored leaf verbatim - no op parameters
           exist in the stream, all come from the descriptor */
        st = *ovr_in; op = st.op; ovr = &st;
    }
    int edx = 0, edy = 0, enpal = 0, esig = 0;
    int16_t epal[PAL_MAX];
    uint8_t eimap[TILE_SZ * TILE_SZ];
    switch (op) {
    case OP_NOISE: {
        int sig = ovr ? ovr->sig : (int)zrd_len_sym(pd, PM_QT);
        esig = sig;
        dec_noise_tile(prev, curr, w, h, tx, ty, tw, th,
                       has_prev, S, Wp, luma, sig,
                       nseed + (uint32_t)(ty * 7919 + tx));
        break;
    }
    case OP_RUN:
        for (int y = ty; y < ty + th; y++) {
            if (has_prev)
                memcpy(curr + (size_t)y * w + tx,
                       prev + (size_t)y * w + tx,
                       (size_t)tw * sizeof(int16_t));
            else
                memset(curr + (size_t)y * w + tx, 0,
                       (size_t)tw * sizeof(int16_t));
        }
        break;
    case OP_COPY: {
        int bx = 0, by = 0;
        if (sf && slot && mvfam(slot->op) == 1) {
            bx = slot->dx; by = slot->dy;
        } else if (cell && mvfam(cell->op) == 1) {
            bx = cell->dx; by = cell->dy;
        }
        int dx = ovr ? ovr->dx : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + bx;
        int dy = ovr ? ovr->dy : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + by;
        last_tmv[0] = dx; last_tmv[1] = dy; edx = dx; edy = dy;
        if (has_prev)
            dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                          dx, dy, 1, S, rd, Wp, luma, qs, 0, -yoff, hfull - yoff);
        else
            for (int y = ty; y < ty + th; y++)
                memset(curr + (size_t)y * w + tx, 0,
                       (size_t)tw * sizeof(int16_t));
        break;
    }
    case OP_COPYNF: {
        int bx = 0, by = 0;
        if (sf && slot && mvfam(slot->op) == 1) {
            bx = slot->dx; by = slot->dy;
        } else if (cell && mvfam(cell->op) == 1) {
            bx = cell->dx; by = cell->dy;
        }
        int dx = ovr ? ovr->dx : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + bx;
        int dy = ovr ? ovr->dy : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + by;
        last_tmv[0] = dx; last_tmv[1] = dy; edx = dx; edy = dy;
        if (has_prev)
            dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                          dx, dy, 1, S, rd, Wp, luma, qs, 1, -yoff, hfull - yoff);
        else
            for (int y = ty; y < ty + th; y++)
                memset(curr + (size_t)y * w + tx, 0,
                       (size_t)tw * sizeof(int16_t));
        break;
    }
    case OP_PAL: case OP_PALR: case OP_PALT: case OP_PALQ:
    case OP_PALZ: case OP_PALU: case OP_PALC: case OP_PALD: {
        int k = ovr ? ovr->npal : (int)zrd_len_sym(pd, PM_PALN) + 1;
        int16_t pal[PAL_MAX];
        int kc = k < PAL_MAX ? k : PAL_MAX;
        if (ovr) memcpy(pal, ovr->pal, (size_t)kc * sizeof(int16_t));
        else {
            for (int i = 0; i < k; i++) {
                int v;
                if (op == OP_PALQ || op == OP_PALU) {
                    int sym = (int)zrd_len_sym(pd, PM_PALQ);
                    if (sym >= 0 && dpq && dpqn && sym < *dpqn && sym < PALQ_N)
                        v = dpq[sym];
                    else
                        v = (int)zrd_len_sym(pd, PM_PALV) - LIT_BIAS;
                } else if (op == OP_PALZ)
                    v = zigzag_dec((uint32_t)zrd_len_sym(pd, PM_PALZ));
                else
                    v = (int)zrd_len_sym(pd, PM_PALV) - LIT_BIAS;
                if (i < kc) pal[i] = (int16_t)v;
            }
            if (dpq && dpqn)
                for (int i = 0; i < kc; i++) palq_note(dpq, dpqn, pal[i]);
        }
        enpal = kc; memcpy(epal, pal, (size_t)kc * sizeof(int16_t));
        int np = tw * th;
        if (ovr && exact) {
            /* descriptor carries the index map: a pixel-exact replayed
               palette leaf needs zero per-pixel records */
            memcpy(eimap, ovr->imap, (size_t)np);
            for (int i = 0; i < np; i++)
                curr[(size_t)(ty + i / tw) * w + tx + i % tw] =
                    pal[ovr->imap[i] < kc ? ovr->imap[i] : 0];
        } else if (op == OP_PALT || op == OP_PALQ || op == OP_PALZ ||
                   op == OP_PALU) {
            /* params replay of PALT is never emitted (descriptors hold
               no thresholds); PALX/PSAMX arrive via the exact path */
            if (ovr || !luma) return -1;
            int thr[PAL_MAX];
            int zpred = 0;
            for (int j = 0; j + 1 < k && j < PAL_MAX - 1; j++) {
                int z = zigzag_dec((int)zrd_len_sym(pd, PM_CFLB));
                if (op == OP_PALZ || op == OP_PALU) {
                    thr[j] = zpred + z;
                    zpred = thr[j];
                } else
                    thr[j] = z;
            }
            for (int i = 0; i < np; i++) {
                int lu = luma[(size_t)(ty + i / tw) * w + tx + i % tw];
                int j = 0;
                while (j + 1 < k && j < PAL_MAX - 1 && lu >= thr[j]) j++;
                if (j >= kc) j = kc - 1;
                eimap[i] = (uint8_t)j;
                curr[(size_t)(ty + i / tw) * w + tx + i % tw] = pal[j];
            }
        } else if (op == OP_PALC) {
            for (int i = 0; i < np; i++) {
                int c = (int)zrd_len_sym(pd, PM_PALF);
                int pu = i >= tw ? eimap[i - tw] : 0;
                int pl = (i % tw) ? eimap[i - 1] : pu;
                int idx;
                if (c == 0) idx = pl;
                else if (c == 1) idx = pu;
                else {
                    idx = (int)zrd_len_sym(pd, PM_PALI);
                    if (idx >= kc) idx = kc - 1;
                    if (idx < 0) idx = 0;
                }
                eimap[i] = (uint8_t)idx;
                curr[(size_t)(ty + i / tw) * w + tx + i % tw] = pal[idx];
            }
        } else if (op == OP_PALD) {
            for (int i = 0; i < np; i++) {
                int a = (i % tw) ? eimap[i - 1] : 0;
                int b = (i >= tw) ? eimap[i - tw] : a;
                int c = (i >= tw && (i % tw)) ? eimap[i - tw - 1] : a;
                int p0 = a + b - c;
                int pa = p0 > a ? p0 - a : a - p0;
                int pb = p0 > b ? p0 - b : b - p0;
                int pc = p0 > c ? p0 - c : c - p0;
                int pr = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
                int idx = pr
                        + zigzag_dec((int)zrd_len_sym(pd, PM_PALD));
                if (idx < 0) idx = 0; else if (idx >= kc) idx = kc - 1;
                eimap[i] = (uint8_t)idx;
                curr[(size_t)(ty + i / tw) * w + tx + i % tw] = pal[idx];
            }
        } else if (op == OP_PAL) {
            for (int i = 0; i < np; i++) {
                int idx = (int)zrd_len_sym(pd, PM_PALI);
                if (idx >= kc) idx = kc - 1;
                if (idx < 0) idx = 0;
                eimap[i] = (uint8_t)idx;
                curr[(size_t)(ty + i / tw) * w + tx + i % tw] = pal[idx];
            }
        } else {
            for (int i = 0; i < np; ) {
                int idx = (int)zrd_len_sym(pd, PM_PALI);
                if (idx >= kc) idx = kc - 1;
                if (idx < 0) idx = 0;
                int r = (int)zrd_len_sym(pd, PM_REPN) + 1;
                if (r > np - i) r = np - i;
                for (; r--; i++) {
                    eimap[i] = (uint8_t)idx;
                    curr[(size_t)(ty + i / tw) * w + tx + i % tw] = pal[idx];
                }
            }
        }
        break;
    }
    case OP_ICOPY: {
        int bx = 0, by = 0;
        if (sf && slot && mvfam(slot->op) == 2) {
            bx = slot->dx; by = slot->dy;
        } else if (cell && mvfam(cell->op) == 2) {
            bx = cell->dx; by = cell->dy;
        }
        int dx = ovr ? ovr->dx : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + bx;
        int dy = ovr ? ovr->dy : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + by;
        last_imv[0] = dx; last_imv[1] = dy; edx = dx; edy = dy;
        dec_pred_tile(curr, curr, w, h, tx, ty, tw, th,
                      dx, dy, 1, S, rd, Wp, luma, qs, 0, 0, h);
        break;
    }
    case OP_ICOPYNF: {
        int bx = 0, by = 0;
        if (sf && slot && mvfam(slot->op) == 2) {
            bx = slot->dx; by = slot->dy;
        } else if (cell && mvfam(cell->op) == 2) {
            bx = cell->dx; by = cell->dy;
        }
        int dx = ovr ? ovr->dx : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + bx;
        int dy = ovr ? ovr->dy : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + by;
        last_imv[0] = dx; last_imv[1] = dy; edx = dx; edy = dy;
        dec_pred_tile(curr, curr, w, h, tx, ty, tw, th,
                      dx, dy, 1, S, rd, Wp, luma, qs, 1, 0, h);
        break;
    }
    case OP_COPYP:
        edx = last_tmv[0]; edy = last_tmv[1];
        if (ZE(zmv)) fprintf(stderr,"DMV @%d,%d %dx%d COPYP tmv=%d,%d\n",tx,ty,tw,th,edx,edy);
        if (has_prev)
            dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                          last_tmv[0], last_tmv[1], 1, S, rd, Wp, luma, qs, 0, -yoff, hfull - yoff);
        else
            for (int y = ty; y < ty + th; y++)
                memset(curr + (size_t)y * w + tx, 0,
                       (size_t)tw * sizeof(int16_t));
        break;
    case OP_MVRUNP:
        edx = last_tmv[0]; edy = last_tmv[1];
        if (ZE(zmv)) fprintf(stderr,"DMV @%d,%d %dx%d MVRUNP tmv=%d,%d\n",tx,ty,tw,th,edx,edy);
        for (int y = ty; y < ty + th; y++) {
            int sy = y + last_tmv[1];
            if (sy < -yoff) sy = -yoff;
            else if (sy >= hfull - yoff) sy = hfull - yoff - 1;
            int16_t *row = curr + (size_t)y * w;
            const int16_t *prow = prev + (size_t)sy * w;
            if (!has_prev) { memset(row + tx, 0, (size_t)tw * sizeof(int16_t)); continue; }
            copy_shift_row(row, prow, tx, tw, last_tmv[0], w);
        }
        break;
    case OP_MVRUN: {
        int bx = 0, by = 0;
        if (sf && slot && mvfam(slot->op) == 1) {
            bx = slot->dx; by = slot->dy;
        } else if (cell && mvfam(cell->op) == 1) {
            bx = cell->dx; by = cell->dy;
        }
        int dx = ovr ? ovr->dx : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + bx;
        int dy = ovr ? ovr->dy : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + by;
        last_tmv[0] = dx; last_tmv[1] = dy; edx = dx; edy = dy;
        for (int y = ty; y < ty + th; y++) {
            int sy = y + dy;
            if (sy < -yoff) sy = -yoff;
            else if (sy >= hfull - yoff) sy = hfull - yoff - 1;
            int16_t *row = curr + (size_t)y * w;
            const int16_t *prow = prev + (size_t)sy * w;
            if (!has_prev) { memset(row + tx, 0, (size_t)tw * sizeof(int16_t)); continue; }
            copy_shift_row(row, prow, tx, tw, dx, w);
        }
        break;
    }
    case OP_IRUN: {
        int bx = 0, by = 0;
        if (sf && slot && mvfam(slot->op) == 2) {
            bx = slot->dx; by = slot->dy;
        } else if (cell && mvfam(cell->op) == 2) {
            bx = cell->dx; by = cell->dy;
        }
        int dx = ovr ? ovr->dx : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + bx;
        int dy = ovr ? ovr->dy : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + by;
        last_imv[0] = dx; last_imv[1] = dy; edx = dx; edy = dy;
        for (int y = ty; y < ty + th; y++) {
            int sy = y + dy;
            if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
            int16_t *row = curr + (size_t)y * w;
            const int16_t *prow = curr + (size_t)sy * w;
            copy_shift_row(row, prow, tx, tw, dx, w);
        }
        break;
    }
    case OP_IRUNP:
        edx = last_imv[0]; edy = last_imv[1];
        for (int y = ty; y < ty + th; y++) {
            int sy = y + last_imv[1];
            if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
            int16_t *row = curr + (size_t)y * w;
            const int16_t *prow = curr + (size_t)sy * w;
            copy_shift_row(row, prow, tx, tw, last_imv[0], w);
        }
        break;
    case OP_CFL: {
        int ca = ovr ? ovr->dx : zigzag_dec(zrd_len_sym(pd, PM_CFLA));
        int cb = ovr ? ovr->dy : zigzag_dec(zrd_len_sym(pd, PM_CFLB));
        edx = ca; edy = cb;
        dec_cfl_tile(prev, curr, w, h, tx, ty, tw, th,
                     ca, cb, has_prev, S, rd, luma, qs);
        break;
    }
    case OP_RAW:
        for (int y = ty; y < ty + th; y++)
            for (int x = tx; x < tx + tw; x++)
                curr[(size_t)y * w + x] =
                    (int16_t)((int)zrd_len_sym(pd, PM_RAW) - LIT_BIAS);
        break;
    case OP_NN:
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, Wp, luma, qs, 0, -yoff, hfull - yoff);
        break;
    case OP_NNF:
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, Wp, luma, qs, 1, -yoff, hfull - yoff);
        break;
    case OP_PREDNF:
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, NULL, luma, qs, 1, -yoff, hfull - yoff);
        break;
    case OP_PREDNFX:
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, NULL, luma, qs, 4, -yoff, hfull - yoff);
        break;
    case OP_PREDX:
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, NULL, luma, qs, 5, -yoff, hfull - yoff);
        break;
    case OP_PREDIR:
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, NULL, luma, qs, 3, -yoff, hfull - yoff);
        break;
    case OP_PREDZ:
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, NULL, luma, qs, 2, -yoff, hfull - yoff);
        break;
    case OP_NNZ:
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, Wp, luma, qs, 2, -yoff, hfull - yoff);
        break;
    case OP_ROWD: case OP_ROWQ: case OP_ROWS: {
        int k = (int)zrd_len_sym(pd, PM_PALN) + 1;
        if (k < 1 || k > PAL_MAX || tw < 1 || th < 1 ||
            tw > TILE_SZ || th > TILE_SZ)
            return -1;
        int16_t pal[PAL_MAX];
        int pred = 0;
        for (int i = 0; i < k; i++) {
            int v;
            if (op == OP_ROWQ) {
                int sym = (int)zrd_len_sym(pd, PM_PALQ);
                if (sym >= 0 && dpq && dpqn && sym < *dpqn && sym < PALQ_N)
                    v = dpq[sym];
                else if (luma)
                    v = (int)zrd_len_sym(pd, PM_PALV) - LIT_BIAS;
                else
                    v = pred + zigzag_dec((uint32_t)zrd_len_sym(pd, PM_PALV));
            } else if (luma)
                v = (int)zrd_len_sym(pd, PM_PALV) - LIT_BIAS;
            else
                v = pred + zigzag_dec((uint32_t)zrd_len_sym(pd, PM_PALV));
            pred = v;
            pal[i] = (int16_t)v;
        }
        if (dpq && dpqn)
            for (int i = 0; i < k; i++) palq_note(dpq, dpqn, pal[i]);
        int nr = (int)zrd_len_sym(pd, PM_PALN) + 1;
        if (nr < 1 || nr > th) return -1;
        uint8_t pat[TILE_SZ][TILE_SZ];
        if (op == OP_ROWS) {
            int stem_w = (int)zrd_len_sym(pd, PM_PALN) + 1;
            if (stem_w < 1 || stem_w >= tw) return -1;
            uint8_t stem[TILE_SZ];
            if (dec_fill_runs(pd, stem, stem_w, k)) return -1;
            for (int p = 0; p < nr; p++) {
                memcpy(pat[p], stem, (size_t)stem_w);
                if (dec_fill_runs(pd, pat[p] + stem_w, tw - stem_w, k))
                    return -1;
            }
        } else {
            for (int p = 0; p < nr; p++)
                if (dec_fill_runs(pd, pat[p], tw, k)) return -1;
        }
        int nruns = (int)zrd_len_sym(pd, PM_PALN) + 1;
        if (nruns < 1 || nruns > th) return -1;
        int y = 0;
        for (int i = 0; i < nruns; i++) {
            int idx = 0;
            if (i > 0) {
                idx = (int)zrd_len_sym(pd, PM_PALI);
                if (idx < 0 || idx >= nr) idx = 0;
            }
            int r;
            if (i + 1 == nruns)
                r = th - y;
            else {
                r = (int)zrd_len_sym(pd, PM_RRL) + 1;
                if (r < 1 || r >= th - y) return -1;
            }
            if (r < 1) return -1;
            for (int t = 0; t < r; t++, y++) {
                for (int x = 0; x < tw; x++) {
                    int pi = pat[idx][x];
                    if (pi >= k) pi = 0;
                    curr[(size_t)(ty + y) * w + tx + x] = pal[pi];
                }
            }
        }
        if (y != th) return -1;
        break;
    }
    case OP_TX: {
        int bx = 0, by = 0;
        if (sf && slot && mvfam(slot->op) == 1) {
            bx = slot->dx; by = slot->dy;
        } else if (cell && mvfam(cell->op) == 1) {
            bx = cell->dx; by = cell->dy;
        }
        int dx = 0, dy = 0;
        if (has_prev) {
            dx = ovr ? ovr->dx : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + bx;
            dy = ovr ? ovr->dy : zigzag_dec(zrd_len_sym(pd, (uint32_t)PM_MV)) + by;
            last_tmv[0] = dx; last_tmv[1] = dy;
        }
        edx = dx; edy = dy;
        if (dec_tx_tile(prev, curr, w, h, tx, ty, tw, th,
                        dx, dy, has_prev, rd, qs, yoff, hfull, 0) < 0)
            return -1;
        break;
    }
    case OP_TXI:
        edx = 0; edy = 0;
        if (dec_tx_tile(prev, curr, w, h, tx, ty, tw, th,
                        0, 0, 0, rd, qs, yoff, hfull, 1) < 0)
            return -1;
        break;
    default: /* OP_PRED and unknown ops fall through to pred */
        dec_pred_tile(prev, curr, w, h, tx, ty, tw, th,
                      0, 0, has_prev, S, rd, NULL, luma, qs, 0, -yoff, hfull - yoff);
        break;
    }
    if (cell) {
        cell->rx = (int16_t)tx; cell->ry = (int16_t)ty;
        cell->rw = (int16_t)tw;
        cell->op = op; cell->dx = (int16_t)edx; cell->dy = (int16_t)edy;
        cell->npal = enpal; cell->sig = esig;
        if (enpal) {
            memcpy(cell->pal, epal, (size_t)enpal * sizeof(int16_t));
            memcpy(cell->imap, eimap, (size_t)(tw * th));
        }
    }
    if (ZE(zpix)) {
        uint32_t hsh = 5381;
        for (int y = ty; y < ty + th; y++)
            for (int x = tx; x < tx + tw; x++)
                hsh = hsh * 33 + (uint16_t)curr[(size_t)y * w + x];
        fprintf(stderr, "DPX @%d,%d %dx%d op=%d h=%08x n=%ld\n",
                tx, ty, tw, th, op, hsh, rd ? (long)rd->tpull - rs0 : 0);
    }
    return op;
}

/* decode the op subtree covering one rect: QS directives consume no
   pixels, nested SPLIT recurses, leaf ops go through dec_leaf_op.
   Recursion is bounded: rects below 8 px can not split. */
static int dec_rect(const int16_t *prev, int16_t *curr,
                     int w, int h, const int16_t *luma,
                     int rx, int ry, int rw, int rh,
                     int has_prev, pstate_t *S, zrd_t *pd, zrd_t *rd,
                     const int16_t *Wp, int *last_tmv, int *last_imv,
                     int *last_op, int *qsp, uint32_t nseed,
                     vop_t *vops, int sf, const dec_pb_t *luma_pb,
                     int yoff, int hfull)
{
    int ntx = (w + TILE_SZ - 1) / TILE_SZ;
    for (;;) {
        int cop = (int)zrd_len_sym(pd,
                    PM_OP0 + ((*last_op < N_OPOPS ? *last_op : 0) << 1));
        if (ZE(zd2)) fprintf(stderr, "d %x op=%d rx=%d ry=%d %dx%d lo=%d\n",
                                   (unsigned)((uintptr_t)S & 0xffff),
                                   cop, rx, ry, rw, rh, *last_op);
        if (cop == OP_QS) {
            *qsp = (int)zrd_len_sym(pd, PM_QT) + 1;
            continue;
        }
        if (cop == OP_SPLIT && rw >= 8 && rh >= 8) {
            int hw = rw / 2, hh = rh / 2;
            for (int i = 0; i < 4; i++)
                if (dec_rect(prev, curr, w, h, luma,
                             rx + (i & 1) * hw, ry + (i >> 1) * hh,
                             (i & 1) ? rw - hw : hw, (i >> 1) ? rh - hh : hh,
                             has_prev, S, pd, rd, Wp, last_tmv, last_imv,
                             last_op, qsp, nseed, vops, sf, luma_pb,
                             yoff, hfull) < 0)
                    return -1;
            return 0;
        }
        int eop = dec_leaf_op(cop, prev, curr, w, h, luma, rx, ry, rw, rh,
                              has_prev, S, pd, rd, Wp, last_tmv, last_imv,
                              *qsp, nseed, vops, ntx, NULL, sf, NULL, 0,
                              luma_pb, yoff, hfull);
        if (eop < 0) return -1;
        *last_op = eop < N_OPOPS ? eop : 0;
        return 0;
    }
}

/* decode one top-level tile; returns the number of raster tiles consumed
   (OP_RPT expands to k), or -1 on a malformed stream */
static int dec_one_tile(const int16_t *prev, int16_t *curr,
                        int w, int h, const int16_t *luma,
                        int ti, int ntx, int has_prev,
                        pstate_t *S, zrd_t *pd, zrd_t *rd,
                        const int16_t *Wp, int *last_tmv, int *last_imv,
                        int *last_op, int *qsp, uint32_t nseed,
                        vop_t *vops, vop_t *ptab, uint8_t *upstat, int sf,
                        const dec_pb_t *luma_pb, int yoff, int hfull)
{
    int qs = *qsp;
    int tx = (ti % ntx) * TILE_SZ, ty = (ti / ntx) * TILE_SZ;
    int tw = w - tx < TILE_SZ ? w - tx : TILE_SZ;
    int th = h - ty < TILE_SZ ? h - ty : TILE_SZ;
    int op = (int)zrd_len_sym(pd,
                 PM_OP0 + ((*last_op < N_OPOPS ? *last_op : 0) << 1)
                        + (ZE(znoup) ? 0 : upstat[ti % ntx]));
    if (ZE(zd2)) fprintf(stderr, "D %x ti=%d op=%d lo=%d up=%d\n",
                         (unsigned)((uintptr_t)S & 0xffff), ti, op, *last_op,
                         ZE(znoup) ? 0 : upstat[ti % ntx]);
    if (op == OP_QS) {
        *qsp = (int)zrd_len_sym(pd, PM_QT) + 1;
        return pd->err ? -1 : 0;     /* directive: no tile consumed */
    }
    if (op == OP_SPLIT && tw >= 8 && th >= 8) {
        int hw = tw / 2, hh = th / 2;
        for (int i = 0; i < 4; i++)
            if (dec_rect(prev, curr, w, h, luma,
                         tx + (i & 1) * hw, ty + (i >> 1) * hh,
                         (i & 1) ? tw - hw : hw, (i >> 1) ? th - hh : hh,
                         has_prev, S, pd, rd, Wp, last_tmv, last_imv,
                         last_op, qsp, nseed, vops, sf, luma_pb,
                         yoff, hfull) < 0)
                return -1;
        if (ptab) { ptab[ti].op = OP_SPLIT; ptab[ti].rw = 0; }
        upstat[ti % ntx] = 1;
        return 1;
    }
    if (op == OP_RPT) {
        int k = (int)zrd_len_sym(pd, PM_REPN) + 1;
        int rep_op = *last_op;
        for (int j = 0; j < k; j++) {
            int t2 = ti + j;
            int rx = (t2 % ntx) * TILE_SZ, ry = (t2 / ntx) * TILE_SZ;
            if (ry >= h) return -1;
            int rw = w - rx < TILE_SZ ? w - rx : TILE_SZ;
            int rh = h - ry < TILE_SZ ? h - ry : TILE_SZ;
            if (dec_leaf_op(rep_op, prev, curr, w, h, luma, rx, ry, rw, rh,
                            has_prev, S, pd, rd, Wp, last_tmv, last_imv,
                            qs, nseed, vops, ntx, NULL, sf, NULL, 0,
                            luma_pb, yoff, hfull) < 0)
                return -1;
            upstat[t2 % ntx] = 0;
            if (ptab) {
                vop_t *vc = &vops[vcell_idx(ntx, rx, rw)];
                ptab[t2] = *vc; ptab[t2].rw = (int16_t)rw;
            }
        }
        return k;
    }
    if (op == OP_SPAN) {
        /* Motion runs separated by exactly one co-located hole.
           The hole is not a symbol: after each motion length, one
           RUN tile is implied while tiles remain in k. */
        int k = (int)zrd_len_sym(pd, PM_REPN) + 1;
        if (pd->err || k < 1) return -1;
        int done = 0;
        while (done < k) {
            uint32_t Ls = zrd_len_sym(pd, PM_RRL);
            if (pd->err || Ls >= (uint32_t)(k - done)) return -1;
            int L = (int)Ls + 1;
            for (int j = 0; j < L; j++) {
                int t2 = ti + done + j;
                int rx = (t2 % ntx) * TILE_SZ, ry = (t2 / ntx) * TILE_SZ;
                if (ry >= h) return -1;
                int rw = w - rx < TILE_SZ ? w - rx : TILE_SZ;
                int rh = h - ry < TILE_SZ ? h - ry : TILE_SZ;
                int eop = dec_leaf_op(OP_MVRUNP, prev, curr, w, h, luma,
                            rx, ry, rw, rh, has_prev, S, pd, rd, Wp,
                            last_tmv, last_imv, qs, nseed, vops, ntx,
                            NULL, sf, NULL, 0, luma_pb, yoff, hfull);
                if (eop < 0) return -1;
                *last_op = eop < N_OPOPS ? eop : 0;
                upstat[t2 % ntx] = 0;
                if (ptab) {
                    vop_t *vc = &vops[vcell_idx(ntx, rx, rw)];
                    ptab[t2] = *vc; ptab[t2].rw = (int16_t)rw;
                }
            }
            done += L;
            if (done < k) {
                int t2 = ti + done;
                int rx = (t2 % ntx) * TILE_SZ, ry = (t2 / ntx) * TILE_SZ;
                if (ry >= h) return -1;
                int rw = w - rx < TILE_SZ ? w - rx : TILE_SZ;
                int rh = h - ry < TILE_SZ ? h - ry : TILE_SZ;
                int eop = dec_leaf_op(OP_RUN, prev, curr, w, h, luma,
                            rx, ry, rw, rh, has_prev, S, pd, rd, Wp,
                            last_tmv, last_imv, qs, nseed, vops, ntx,
                            NULL, sf, NULL, 0, luma_pb, yoff, hfull);
                if (eop < 0) return -1;
                *last_op = eop < N_OPOPS ? eop : 0;
                upstat[t2 % ntx] = 0;
                if (ptab) {
                    vop_t *vc = &vops[vcell_idx(ntx, rx, rw)];
                    ptab[t2] = *vc; ptab[t2].rw = (int16_t)rw;
                }
                done++;
            }
        }
        return k;
    }
    if (op == OP_PSAME) {
        /* replay the leaf the previous frame stored in this slot */
        if (!ptab) return -1;
        vop_t st = ptab[ti];
        if (st.rw != tw || st.rx != tx || st.op == OP_SPLIT ||
            st.op == OP_VRPT || st.op == OP_PSAME || st.op == OP_RPT ||
            st.op == OP_SPAN || st.op == OP_QS) {
            if (ZE(zd))
                fprintf(stderr, "PSAME reject ti=%d st.op=%d st.rw=%d st.rx=%d tw=%d tx=%d\n",
                        ti, st.op, st.rw, st.rx, tw, tx);
            return -1;
        }
        int eop = dec_leaf_op(st.op, prev, curr, w, h, luma, tx, ty, tw, th,
                              has_prev, S, pd, rd, Wp, last_tmv, last_imv,
                              qs, nseed, vops, ntx, &st, sf, NULL, 0,
                              luma_pb, yoff, hfull);
        if (eop < 0) {
            if (ZE(zd)) fprintf(stderr, "PSAME leaf fail ti=%d op=%d\n", ti, st.op);
            return -1;
        }
        *last_op = eop < N_OPOPS ? eop : 0;
        upstat[ti % ntx] = 0;
        return 1;
    }
    if (op == OP_PSAMX) {
        /* pixel-exact temporal replay: only valid where the stored
           descriptor determines all pixels (palette leaves) */
        if (!ptab) return -1;
        vop_t st = ptab[ti];
        if (st.rw != tw || st.rx != tx ||
            (st.op != OP_PAL && st.op != OP_PALR && st.op != OP_PALT &&
             st.op != OP_PALQ && st.op != OP_PALZ && st.op != OP_PALU &&
             st.op != OP_PALC && st.op != OP_PALD)) {
            if (ZE(zd))
                fprintf(stderr, "PSAMX reject ti=%d st.op=%d\n", ti, st.op);
            return -1;
        }
        int eop = dec_leaf_op(st.op, prev, curr, w, h, luma, tx, ty, tw, th,
                              has_prev, S, pd, rd, Wp, last_tmv, last_imv,
                              qs, nseed, vops, ntx, &st, sf, NULL, 1,
                              luma_pb, yoff, hfull);
        if (eop < 0) return -1;
        *last_op = eop < N_OPOPS ? eop : 0;
        upstat[ti % ntx] = 0;
        return 1;
    }
    if (op != OP_SPLIT) {
        size_t rs0 = rd ? (size_t)rd->tpull : 0;
        int eop = dec_leaf_op(op, prev, curr, w, h, luma, tx, ty, tw, th,
                              has_prev, S, pd, rd, Wp, last_tmv, last_imv,
                              qs, nseed, vops, ntx, NULL, sf,
                              ptab ? &ptab[ti] : NULL, 0, luma_pb, yoff, hfull);
        if (ZE(zr)) fprintf(stderr, "D ti=%d op=%d @%d,%d %dx%d res=%ld\n",
                ti, eop, tx, ty, tw, th, rd ? rd->tpull - (long)rs0 : 0);
        if (eop < 0) return -1;
        *last_op = eop < N_OPOPS ? eop : 0;
        upstat[ti % ntx] = 0;
        if (ptab) ptab[ti] = vops[vcell_idx(ntx, tx, tw)];
        return 1;
    }
    /* OP_SPLIT on a rect too small to subdivide: malformed */
    return -1;
}

/* decode-side counterpart.  Stateful callers pass ptab (one vop_t per
   top-level tile slot - persists across frames and enables OP_PSAME)
   plus Wprev/wvalid for delta-coded weight tables.  Stateless callers
   pass NULLs; streams flagged PHDR_STATEFUL are refused. */
/* pm_carry/rm_carry + mvalid: persistent entropy models (stateful only).
   The first streamed plane cold-inits the arrays and sets *mvalid; later
   planes decode against the carried post-state of the previous frame. */
static int32_t dec_plane_prog_g(const int16_t *prev, int16_t *curr,
                              int w, int h, const int16_t *luma,
                              const uint8_t *in, int32_t in_size,
                              vop_t *ptab, int16_t *Wprev, int *wvalid,
                              mdl64_t *pm_carry, mdl64_t *rm_carry,
                              int *mvalid, pstate_t *ps_carry,
                              dec_pb_t *pub, const dec_pb_t *wait,
                              int yoff, int hfull)
{
    int stateful = (ptab != NULL);
    if (in_size < 4) return -1;
    uint32_t hdr = rd_u32le(in);
    int llp = (int)(hdr >> 31);
    int qnt = (int)((hdr >> 30) & 1);   /* bit29: noise-seed record */
    uint32_t plen = hdr & PHDR_PLEN;
    if ((hdr & PHDR_STATEFUL) && !stateful) return -1;
    if (plen == 0) {                 /* all-RUN plane: 4-byte record */
        for (int y = 0; y < h; y++) {
            if (prev) memcpy(curr + (size_t)y * w, prev + (size_t)y * w,
                             (size_t)w * sizeof(int16_t));
            else memset(curr + (size_t)y * w, 0, (size_t)w * sizeof(int16_t));
        }
        if (pub) atomic_store_explicit(&pub->rows_done, h,
                                       memory_order_release);
        /* the encoder committed a RUN descriptor into every slot;
           mirror that so the next frame's PSAME table stays in sync */
        if (ptab) {
            int nx = (w + TILE_SZ - 1) / TILE_SZ;
            int ni = nx * ((h + TILE_SZ - 1) / TILE_SZ);
            for (int t = 0; t < ni; t++) {
                vop_t *pe = &ptab[t];
                int rx = (t % nx) * TILE_SZ, ry = (t / nx) * TILE_SZ;
                pe->rx = (int16_t)rx; pe->ry = (int16_t)ry;
                pe->rw = (int16_t)(w - rx < TILE_SZ ? w - rx : TILE_SZ);
                pe->op = OP_RUN; pe->dx = 0; pe->dy = 0;
                pe->npal = 0; pe->sig = 0;
            }
        }
        return 4;
    }
    if ((int32_t)(4 + plen) >= in_size) return -1;
    const uint8_t *pb = in + 4;
    const uint8_t *rb = pb + plen;

    mdl64_t pms_l[NPROG_MDL];
    mdl64_t *pms = pm_carry ? pm_carry : pms_l;
    mdl64_t *rms = rm_carry;
    if (!rms) rms = (mdl64_t *)malloc(sizeof(mdl64_t) * N_MDL);
    if (!rms) return -1;
    zrd_t pd, rd;
    int warm = (pm_carry && rm_carry && mvalid &&
                !(hdr & PHDR_FRESH)) ? *mvalid : 0;
    if (zrd_open(&pd, pms, NPROG_MDL, 1, pb, (int32_t)plen, warm) < 0 ||
        zrd_open(&rd, rms, N_MDL, 0, rb, in_size - 4 - (int32_t)plen, warm) < 0) {
        if (!rm_carry) free(rms);
        return -1;
    }
    rd.reslog = 1;

    pstate_t S;
    if (ps_carry && warm) S = *ps_carry; else pstate_init(&S);
    int last_tmv[2] = { 0, 0 }, last_imv[2] = { 0, 0 };
    int last_op = OP_RUN;
    int has_prev = (prev != NULL);
    int qs = 1;
    if (qnt) qs = (int)zrd_len_sym(&pd, PM_QT) + 1;
    uint32_t nseed = 0;
    if (hdr & 0x20000000u) nseed = (uint32_t)zrd_len_sym(&pd, PM_QT);
    int16_t W[N_NNW * N_NNC]; const int16_t *Wp = NULL;
    if (ZE(zwn)) fprintf(stderr, "DW: llp=%d wsame=%d wvalid=%d\n",
            llp, !!(hdr & PHDR_WSAME), stateful && wvalid ? *wvalid : -9);
    if (llp && (hdr & PHDR_WSAME)) {
        /* whole table carried over from the previous frame */
        if (!stateful || !*wvalid) { if (!rm_carry) free(rms); return -1; }
        memcpy(W, Wprev, sizeof(W)); Wp = W;
    } else if (llp) {
        /* stateful streams delta-code weights against the previous
           frame's table (zeroed when absent, so first frame degrades
           to absolute values) */
        const int16_t *base = (stateful && *wvalid) ? Wprev : NULL;
        for (int i = 0; i < N_NNW * N_NNC; i++) {
            int v = zigzag_dec(zrd_len_sym(&pd, PM_NNW));
            W[i] = (int16_t)((base ? base[i] : 0) + v);
        }
        if (stateful) { memcpy(Wprev, W, sizeof(W)); *wvalid = 1; }
        Wp = W;
    }
    if (Wp && ZE(zwn)) {
        uint32_t hh = 5381;
        for (int i = 0; i < N_NNW * N_NNC; i++) hh = hh * 33 + (uint16_t)Wp[i];
        fprintf(stderr, "DWH %08x\n", hh);
    }
    int ntx = (w + TILE_SZ - 1) / TILE_SZ;
    int nti = ntx * ((h + TILE_SZ - 1) / TILE_SZ);
    if (ZE(zd2)) fprintf(stderr, "PLANE %x\n", (unsigned)((uintptr_t)&S & 0xffff));
    vop_t *vops = (vop_t *)tls_get(TLS_VOPS,
                                 (size_t)VOP_CELLS(ntx) * sizeof(vop_t)
                                 + (size_t)ntx);
    if (!vops) { if (!rm_carry) free(rms); return -1; }
    memset(vops, 0, (size_t)VOP_CELLS(ntx) * sizeof(vop_t));
    uint8_t *upstat = (uint8_t *)(vops + VOP_CELLS(ntx));
    memset(upstat, 0, (size_t)ntx);
    int16_t palq[PALQ_N];
    int palq_n = 0;
    dpq = palq; dpqn = &palq_n;
    for (int ti = 0; ti < nti; ) {
        int c = dec_one_tile(prev, curr, w, h, luma, ti, ntx,
                             has_prev, &S, &pd, &rd, Wp,
                             last_tmv, last_imv, &last_op, &qs, nseed,
                             vops, ptab, upstat,
                             stateful, wait, yoff, hfull);
        if (pub && c > 0)
            atomic_store_explicit(&pub->rows_done,
                ti + c >= nti ? h : ((ti + c) / ntx) * TILE_SZ,
                memory_order_release);
        if (c < 0) {
            if (ZE(zd)) fprintf(stderr, "tile fail ti=%d last_op=%d pderr=%d rderr=%d\n",
                                      ti, last_op, pd.err, rd.err);
            if (!rm_carry) free(rms); return -1;
        }
        if (ZE(zrh) && c > 0) {
            for (int t2 = ti; t2 < ti + c && t2 < nti; t2++) {
                int tx = (t2 % ntx) * TILE_SZ, ty = (t2 / ntx) * TILE_SZ;
                int tw = w - tx < TILE_SZ ? w - tx : TILE_SZ;
                int th = h - ty < TILE_SZ ? h - ty : TILE_SZ;
                uint64_t hh = 1469598103934665603ull;
                for (int yy = ty; yy < ty + th; yy++)
                    for (int xx = tx; xx < tx + tw; xx++) {
                        hh ^= (uint16_t)curr[(size_t)yy * w + xx];
                        hh *= 1099511628211ull;
                    }
                fprintf(stderr, "RH D %x ti=%d %d,%d %dx%d h=%016llx\n",
                        (unsigned)((uintptr_t)&S & 0xffff), t2, tx, ty, tw, th,
                        (unsigned long long)hh);
            }
        }
        ti += c;
        if (pd.err || rd.err) {
            if (ZE(zd)) fprintf(stderr, "stream err ti=%d pderr=%d rderr=%d plen=%u hdr=%08x insz=%d ptp=%ld rtp=%ld pcur=%ld rcur=%ld rsegend=%ld\n",
                                      ti, pd.err, rd.err, plen, hdr, in_size,
                                      pd.tpull, rd.tpull,
                                      (long)(pd.cur - pb), (long)(rd.cur - rb),
                                      (long)(rd.seg_end - rb));
            if (!rm_carry) free(rms); return -1;
        }
    }

    int32_t used = 4 + (int32_t)plen + (int32_t)(rd.seg_end - rb);
    if (!rm_carry) free(rms);
    if (!pd.err && !rd.err) {
        if (mvalid) *mvalid = 1;
        if (ps_carry) *ps_carry = S;
        if (pub) atomic_store_explicit(&pub->rows_done, h,
                                       memory_order_release);
    }
    return (pd.err || rd.err) ? -1 : used;
}

/* chroma-420 dispatch: a plane flagged PHDR_C420 was coded on its
   quarter-resolution grid - decode into a scratch image at w2 x h2,
   then replicate back to full res.  Reference planes (prev / luma) are
   decimated with the same top-left rule the encoder used, so both sides
   run identical programs. */
static int32_t dec_plane_prog(const int16_t *prev, int16_t *curr,
                              int w, int h, const int16_t *luma,
                              const uint8_t *in, int32_t in_size,
                              vop_t *ptab, int16_t *Wprev, int *wvalid,
                              mdl64_t *pm_carry, mdl64_t *rm_carry,
                              int *mvalid, pstate_t *ps_carry,
                              dec_pb_t *pub, const dec_pb_t *wait,
                              int yoff, int hfull)
{
    if (in_size < 4) return -1;
    if (hfull <= 0) hfull = h;
    if (!(rd_u32le(in) & PHDR_C420))
        return dec_plane_prog_g(prev, curr, w, h, luma, in, in_size,
                                ptab, Wprev, wvalid,
                                pm_carry, rm_carry, mvalid, ps_carry,
                                pub, wait, yoff, hfull);
    if ((w & 1) || (h & 1) || w < 2 || h < 2) return -1;
    /* quarter-res chroma decimates the WHOLE luma plane up front - the
       pipelined wait degrades to a full barrier.  rows_done counts
       luma-space rows; the luma plane has the full frame height h. */
    if (luma && wait && pb_wait(wait, h) < 0) return -1;
    int w2 = w >> 1, h2 = h >> 1;
    size_t n2 = (size_t)w2 * h2;
    int16_t *scr = (int16_t *)tls_get(TLS_C420, 3 * n2 * sizeof(int16_t));
    if (!scr) return -1;
    int16_t *t  = scr;
    int16_t *p2 = prev ? scr + n2 : NULL;
    int16_t *l2 = luma ? scr + 2 * n2 : NULL;
    if (prev) plane_dn2(p2, prev, w, h);
    if (luma) plane_dn2(l2, luma, w, h);
    int32_t r = dec_plane_prog_g(p2, t, w2, h2, l2, in, in_size,
                                 ptab, Wprev, wvalid,
                                 pm_carry, rm_carry, mvalid, ps_carry,
                                 pub, NULL, 0, h2);
    if (r > 0) plane_up2(curr, t, w, h);
    return r;
}

typedef struct pjob_s {
    const int16_t *prev, *curr, *luma;
    int w, h, qs, qs_hi, nstripes;
    int16_t *recon;
    vop_t *ptab;
    int16_t *Wprev;
    int *wvalid;
    mdl64_t *pm, *rm;
    int *mvalid;
    pstate_t *ps;
    uint8_t *buf;
    int32_t cap, len;
} pjob_t;
/* the encode pipeline is stack-hungry (enc_one_tile alone is ~260KB,
   and candidate streams nest inside stream/core wrappers): give plane
   workers an explicit 4MB stack so an added wrapper frame can never
   push them past the 512KB pthread default */
static int enc_spawn(pthread_t *t, void *(*fn)(void *), void *a) {
    pthread_attr_t at;
    if (pthread_attr_init(&at) == 0) {
        pthread_attr_setstacksize(&at, (size_t)4 << 20);
        int ok = pthread_create(t, &at, fn, a) == 0;
        pthread_attr_destroy(&at);
        if (ok) return 1;
    }
    return pthread_create(t, NULL, fn, a) == 0;
}

static void *enc_plane_job(void *arg) {
    pjob_t *j = (pjob_t *)arg;
    j->len = enc_plane_stream_v3(j->prev, j->curr, j->w, j->h, j->luma,
                                 j->buf, j->cap, j->qs, j->qs_hi, j->recon,
                                 j->ptab, j->Wprev, j->wvalid,
                                 j->pm, j->rm, j->mvalid, j->ps,
                                 j->nstripes);
    return NULL;
}

/* v3s program memory: per-plane leaf-op table (one slot per top-level
   tile) plus the previous LLP table for delta-coded weights.  The blob
   is caller-allocated once via zv3_state_size/init and handed to every
   encode/decode call; it must be zeroed before first use. */
typedef struct {
    uint32_t magic;
    int w, h, nti;
    int wvalid[3];
    int16_t Wprev[3][N_NNW * N_NNC];
    int mvalid[3];                  /* plane's entropy models warmed */
    mdl64_t pm[3][NPROG_MDL];       /* persistent program-stream models */
    mdl64_t rm[3][N_MDL];           /* persistent residual models -
                                       the VM's trained register file */
    pstate_t ps[3];                 /* persistent predictor-selection
                                       state (carried As/At tables) */
    /* vop_t ptab[3 * nti] follows inline */
} zv3s_t;
#define ZV3S_MAGIC 0x5A563353u

int32_t zv3_state_size(int w, int h)
{
    int64_t ntx = (w + TILE_SZ - 1) / TILE_SZ;
    int64_t nty = (h + TILE_SZ - 1) / TILE_SZ;
    int64_t sz = (int64_t)sizeof(zv3s_t) + 3 * ntx * nty * (int64_t)sizeof(vop_t);
    return sz > 0x7FFFFFFF ? -1 : (int32_t)sz;
}

void zv3_state_init(void *st, int w, int h)
{
    memset(st, 0, (size_t)zv3_state_size(w, h));
    zv3s_t *s = (zv3s_t *)st;
    s->magic = ZV3S_MAGIC; s->w = w; s->h = h;
    s->nti = ((w + TILE_SZ - 1) / TILE_SZ) * ((h + TILE_SZ - 1) / TILE_SZ);
}

int32_t zv_encode_frame_v3(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                           int w, int h, uint8_t *out_buf, int32_t out_cap)
{
    if (out_cap < 1) return -1;

    size_t n = (size_t)w * h;
    int16_t *scr = (int16_t *)tls_get(TLS_PLANES6, 6 * n * sizeof(int16_t));
    if (!scr) return -1;
    int16_t *pY  = scr,     *pCo = scr + n,   *pCg = scr + 2 * n;
    int16_t *cY  = scr + 3 * n, *cCo = scr + 4 * n, *cCg = scr + 5 * n;

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);
    deinterleave_planes(curr_ycocg, cY, cCo, cCg, w, h);

    /* planes encode independently (CfL reads the input luma plane, which
       lossless reconstruction reproduces exactly), so run them on three
       threads into private buffers and concatenate afterwards. */
    int32_t pcap = (int32_t)(8 * n + 4096);
    if (pcap > out_cap - 1) pcap = out_cap - 1;
    uint8_t *pbm = (uint8_t *)tls_get(TLS_PB, 3 * (size_t)pcap);
    if (!pbm) return -1;
    uint8_t *pb[3] = { pbm, pbm + pcap, pbm + 2 * (size_t)pcap };
    /* luma stripes split the encode critical path onto parallel
       workers; the chroma planes finish early regardless, so they stay
       framed singles.  ZNSL=1 restores the single-stripe stream. */
    int nst_l = (h >= 256) ? ZE(znsl) : 1;
    if (nst_l < 1) nst_l = 1;
    if (nst_l > 10) nst_l = 10;
    pjob_t jobs[3];
    const int16_t *pp[3] = { prev_ycocg ? pY : NULL, prev_ycocg ? pCo : NULL,
                             prev_ycocg ? pCg : NULL };
    int16_t *cc[3] = { cY, cCo, cCg };
    for (int i = 0; i < 3; i++) {
        jobs[i].prev = pp[i]; jobs[i].curr = cc[i];
        jobs[i].luma = i ? cY : NULL;
        jobs[i].w = w; jobs[i].h = h;
        jobs[i].qs = 1; jobs[i].qs_hi = 1; jobs[i].recon = NULL;
        /* luma dominates the encode critical path; give it stripes when
           the frame is tall enough for the threads to pay off.  With a
           striped sibling, the chroma planes emit framed single-stripe
           streams so the decoder's plane framing stays uniform. */
        jobs[i].nstripes = (i == 0 && h >= 256) ? nst_l
                         : (nst_l > 1 ? (ZE(znsc) > 0 ? ZE(znsc) : -1) : 1);
        jobs[i].ptab = NULL; jobs[i].Wprev = NULL; jobs[i].wvalid = NULL;
        jobs[i].pm = NULL; jobs[i].rm = NULL; jobs[i].mvalid = NULL;
        jobs[i].ps = NULL;
        jobs[i].buf = pb[i]; jobs[i].cap = pcap; jobs[i].len = -1;
    }

    {
        pthread_t th[3]; int nth = 0;
        for (int i = 0; i < 3; i++) {
            if (enc_spawn(&th[i], enc_plane_job, &jobs[i]))
                nth++;
            else
                enc_plane_job(&jobs[i]);     /* fallback: run inline */
        }
        for (int i = 0; i < nth; i++) pthread_join(th[i], NULL);
    }

    /* frame flag bit0 = per-plane offset table (3 x u32le) - lets the
       decoder start each plane stream directly and run chroma planes on
       parallel threads once luma has landed */
    if (out_cap < 13) return -1;
    out_buf[0] = nst_l > 1 ? 3 : 1;
    uint8_t *op = out_buf + 13;
    for (int i = 0; i < 3; i++) {
        if (jobs[i].len < 0 ||
            jobs[i].len > out_cap - (int32_t)(op - out_buf)) return -1;
        wr_u32le(out_buf + 1 + i * 4, (uint32_t)(op - out_buf));
        memcpy(op, pb[i], (size_t)jobs[i].len);
        op += jobs[i].len;
    }
    return (int32_t)(op - out_buf);
}

int32_t zv_encode_frame_v3q(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                            int w, int h, uint8_t *out_buf, int32_t out_cap,
                            int eps, int eps_hi, int16_t *recon_out)
{
    int qs = eps > 0 ? 2 * eps + 1 : 1;
    int qs_hi = eps_hi > eps ? 2 * eps_hi + 1 : qs;
    if (qs > 255) qs = 255;
    if (qs_hi > 255) qs_hi = 255;
    if (qs == 1) {
        int32_t r = zv_encode_frame_v3(prev_ycocg, curr_ycocg,
                                       w, h, out_buf, out_cap);
        if (r > 0 && recon_out)
            memcpy(recon_out, curr_ycocg,
                   (size_t)w * h * 3 * sizeof(int16_t));
        return r;
    }
    if (out_cap < 1) return -1;

    size_t n = (size_t)w * h;
    int16_t *scr = (int16_t *)tls_get(TLS_PLANES9, 9 * n * sizeof(int16_t));
    if (!scr) return -1;
    int16_t *pY  = scr,     *pCo = scr + n,   *pCg = scr + 2 * n;
    int16_t *cY  = scr + 3 * n, *cCo = scr + 4 * n, *cCg = scr + 5 * n;
    int16_t *rY  = scr + 6 * n, *rCo = scr + 7 * n, *rCg = scr + 8 * n;

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);
    deinterleave_planes(curr_ycocg, cY, cCo, cCg, w, h);

    /* serial: chroma CfL reads the DECODED luma plane, so Y must finish
       (producing rY) before chroma planes are evaluated */
    {
        pjob_t jy = { prev_ycocg ? pY : NULL, cY, NULL, w, h, qs, qs_hi, 1, rY,
                      NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      out_buf + 1, out_cap - 1, -1 };
        enc_plane_job(&jy);
        if (jy.len < 0) return -1;
        int cqs = qs * 2; if (cqs > qs_hi) cqs = qs_hi; if (cqs < qs) cqs = qs;
        pjob_t jc1 = { prev_ycocg ? pCo : NULL, cCo, rY, w, h, cqs, qs_hi, 1, rCo,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       out_buf + 1 + jy.len,
                       out_cap - 1 - jy.len, -1 };
        enc_plane_job(&jc1);
        if (jc1.len < 0) return -1;
        pjob_t jc2 = { prev_ycocg ? pCg : NULL, cCg, rY, w, h, cqs, qs_hi, 1, rCg,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       out_buf + 1 + jy.len + jc1.len,
                       out_cap - 1 - jy.len - jc1.len, -1 };
        enc_plane_job(&jc2);
        if (jc2.len < 0) return -1;
        out_buf[0] = 0;
        if (recon_out) interleave_planes(rY, rCo, rCg, recon_out, w, h);
        return 1 + jy.len + jc1.len + jc2.len;
    }
}

int32_t zv_encode_frame_v3s(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                            int w, int h, uint8_t *out_buf, int32_t out_cap,
                            void *state);

/* stateful + quantized: v3q semantics with the persistent leaf-op table,
   LLP state and entropy-model carry of v3s - enabling OP_PSAME replays
   and warm-context op selection under lossy coding too.  Decode with
   zv_decode_frame_v3s (qs is carried in the plane headers). */
int32_t zv_encode_frame_v3sq(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                             int w, int h, uint8_t *out_buf, int32_t out_cap,
                             int eps, int eps_hi, int16_t *recon_out,
                             void *state)
{
    int qs = eps > 0 ? 2 * eps + 1 : 1;
    int qs_hi = eps_hi > eps ? 2 * eps_hi + 1 : qs;
    if (qs > 255) qs = 255;
    if (qs_hi > 255) qs_hi = 255;
    if (!state || out_cap < 1) return -1;
    zv3s_t *s = (zv3s_t *)state;
    if (s->magic != ZV3S_MAGIC || s->w != w || s->h != h) return -1;
    vop_t *ptabs = (vop_t *)(s + 1);
    if (qs == 1) {
        int32_t r = zv_encode_frame_v3s(prev_ycocg, curr_ycocg,
                                        w, h, out_buf, out_cap, state);
        if (r > 0 && recon_out)
            memcpy(recon_out, curr_ycocg,
                   (size_t)w * h * 3 * sizeof(int16_t));
        return r;
    }

    /* Same carried state the lossy pass starts from. A lossless sibling
       is coded from this snapshot and kept when it is strictly smaller:
       on flat screen content the quantizer header and the coarser
       residuals cost more than they save, and error 0 stays inside the
       eps bound. The decoder already accepts that v3s stream. */
    int32_t ssz = zv3_state_size(w, h);
    uint8_t *snap = (ssz > 0) ? (uint8_t *)malloc((size_t)ssz) : NULL;
    if (snap) memcpy(snap, state, (size_t)ssz);

    size_t n = (size_t)w * h;
    int16_t *scr = (int16_t *)tls_get(TLS_PLANES9, 9 * n * sizeof(int16_t));
    if (!scr) { free(snap); return -1; }
    int16_t *pY  = scr,     *pCo = scr + n,   *pCg = scr + 2 * n;
    int16_t *cY  = scr + 3 * n, *cCo = scr + 4 * n, *cCg = scr + 5 * n;
    int16_t *rY  = scr + 6 * n, *rCo = scr + 7 * n, *rCg = scr + 8 * n;

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);
    deinterleave_planes(curr_ycocg, cY, cCo, cCg, w, h);

    /* serial: chroma CfL reads the DECODED luma plane */
    {
        vop_t *pt[3] = { ptabs, ptabs + s->nti, ptabs + (size_t)2 * s->nti };
        const int16_t *pp[3] = { prev_ycocg ? pY : NULL,
                                 prev_ycocg ? pCo : NULL,
                                 prev_ycocg ? pCg : NULL };
        const int16_t *cc[3] = { cY, cCo, cCg };
        int16_t *rr[3] = { rY, rCo, rCg };
        uint8_t *op = out_buf + 1;
        int32_t total = 1;
        int cqs = qs * 2; if (cqs > qs_hi) cqs = qs_hi; if (cqs < qs) cqs = qs;
        for (int i = 0; i < 3; i++) {
            pjob_t j = { pp[i], cc[i], i ? (const int16_t *)rY : NULL,
                         w, h, i ? cqs : qs, qs_hi, 1, rr[i],
                         pt[i],
                         s->Wprev[i], &s->wvalid[i],
                         s->pm[i], s->rm[i],
                         &s->mvalid[i],
                         &s->ps[i],
                         op, out_cap - total, -1 };
            enc_plane_job(&j);
            if (j.len < 0) goto fail;
            op += j.len; total += j.len;
        }
        out_buf[0] = 0;
        if (recon_out) interleave_planes(rY, rCo, rCg, recon_out, w, h);
        if (snap && total > 1) {
            uint8_t *alt = (uint8_t *)malloc((size_t)out_cap);
            if (alt) {
                rowpal_nosort = 1;
                int32_t nll = zv_encode_frame_v3s(
                    prev_ycocg, curr_ycocg, w, h, alt, out_cap, snap);
                rowpal_nosort = 0;
                if (nll > 0 && nll < total) {
                    memcpy(out_buf, alt, (size_t)nll);
                    memcpy(state, snap, (size_t)ssz);
                    if (recon_out)
                        memcpy(recon_out, curr_ycocg,
                               n * 3 * sizeof(int16_t));
                    total = nll;
                }
                free(alt);
            }
        }
        free(snap);
        return total;
    }
fail:
    free(snap);
    return -1;
}

typedef struct {
    const int16_t *prev, *luma;
    int16_t *curr;
    int w, h;
    const uint8_t *in; int32_t in_size;
    vop_t *ptab; int16_t *Wprev; int *wvalid;
    mdl64_t *pm, *rm; int *mvalid; pstate_t *ps;
    dec_pb_t *pub; const dec_pb_t *wait;
    int32_t r;
    /* pre-barriers (optional tail fields - older initializers zero-fill
       them): rows_done targets on luma stripes that must complete before
       this job starts (c420 whole-luma decimate / odd streams) */
    const dec_pb_t *pre[12]; int pre_need[12], npre;
    /* deferred scratch init: non-last stripes get a private copy of the
       carried state, made on the worker thread so the multi-MB memcpy
       parallelises instead of serialising the spawn loop */
    const mdl64_t *pm_src; const mdl64_t *rm_src;
    const int16_t *Wp_src; const pstate_t *ps_src;
    int wv_src, mv_src;
    int yoff, hfull;    /* prev-source y-range for this slice:
                           local sy in [-yoff, hfull-yoff); 0/0 = [0,h) */
} dec_plane_job;

static void *dec_plane_job_run(void *v) {
    dec_plane_job *j = (dec_plane_job *)v;
    if (j->pm_src) {
        memcpy(j->pm, j->pm_src, NPROG_MDL * sizeof(mdl64_t));
        memcpy(j->rm, j->rm_src, N_MDL * sizeof(mdl64_t));
        memcpy(j->Wprev, j->Wp_src, N_NNW * N_NNC * sizeof(int16_t));
        *j->wvalid = j->wv_src; *j->mvalid = j->mv_src;
        *j->ps = *j->ps_src;
    }
    for (int i = 0; i < j->npre; i++)
        if (pb_wait(j->pre[i], j->pre_need[i]) < 0) { j->r = -1; return NULL; }
    j->r = dec_plane_prog(j->prev, j->curr, j->w, j->h, j->luma,
                          j->in, j->in_size, j->ptab, j->Wprev, j->wvalid,
                          j->pm, j->rm, j->mvalid, j->ps,
                          j->pub, j->wait, j->yoff, j->hfull);
    if (j->pub && j->r < 0)
        atomic_store_explicit(&j->pub->rows_done, -1,
                              memory_order_release);
    return NULL;
}

int32_t zv_decode_frame_v3(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                           int w, int h, const uint8_t *in_buf, int32_t in_size)
{
    if (in_size < 1) return -1;

    size_t n = (size_t)w * h;
    int16_t *scr = (int16_t *)tls_get(TLS_PLANES6, 6 * n * sizeof(int16_t));
    if (!scr) return -1;
    int16_t *pY  = scr,     *pCo = scr + n,   *pCg = scr + 2 * n;
    int16_t *cY  = scr + 3 * n, *cCo = scr + 4 * n, *cCg = scr + 5 * n;

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);

    int32_t r;
    if ((in_buf[0] & 3) == 3 && in_size >= 13) {
        /* stripe-framed planes (stateless: no carried state to fan out) */
        const uint8_t *pl[3] = { in_buf + rd_u32le(in_buf + 1),
                                 in_buf + rd_u32le(in_buf + 5),
                                 in_buf + rd_u32le(in_buf + 9) };
        if (pl[0] < in_buf + 13 || pl[1] <= pl[0] || pl[2] <= pl[1] ||
            pl[2] >= in_buf + in_size) return -1;
        int32_t pl_len[3] = { (int32_t)(pl[1] - pl[0]),
                              (int32_t)(pl[2] - pl[1]),
                              (int32_t)(in_buf + in_size - pl[2]) };
        int nsp[3], nj = 0;
        for (int p = 0; p < 3; p++) {
            if (pl_len[p] < 1) return -1;
            nsp[p] = pl[p][0];
            if (nsp[p] < 1 || nsp[p] > 12 ||
                pl_len[p] < 1 + 4 * nsp[p]) return -1;
            nj += nsp[p];
        }
        int K = nsp[0];
        dec_pb_t pb[12];
        for (int i = 0; i < K; i++) atomic_init(&pb[i].rows_done, 0);
        int nty = (h + TILE_SZ - 1) / TILE_SZ;
        int sh_l[12];
        for (int i = 0; i < K; i++) {
            int a, b; stripe_bounds(nty, K, i, h, &a, &b);
            sh_l[i] = b - a;
        }
        dec_plane_job jobs[36];
        pthread_t th[36];
        int nth = 0, ji = 0, bad = 0;
        const int16_t *ppl[3] = { prev_ycocg ? pY : NULL,
                                  prev_ycocg ? pCo : NULL,
                                  prev_ycocg ? pCg : NULL };
        int16_t *cpl[3] = { cY, cCo, cCg };
        for (int p = 0; p < 3 && !bad; p++) {
            for (int st = 0; st < nsp[p]; st++) {
                uint32_t off0 = rd_u32le(pl[p] + 1 + 4 * st);
                uint32_t off1 = (st + 1 < nsp[p])
                                ? rd_u32le(pl[p] + 5 + 4 * st)
                                : (uint32_t)pl_len[p];
                if (off0 < 1 + 4 * (uint32_t)nsp[p] || off1 <= off0 ||
                    off1 > (uint32_t)pl_len[p]) { bad = 1; break; }
                int r0, r1;
                if (!stripe_bounds(nty, nsp[p], st, h, &r0, &r1)) {
                    bad = 1; break;
                }
                dec_plane_job *j = &jobs[ji];
                memset(j, 0, sizeof(*j));
                j->prev = ppl[p] ? ppl[p] + (size_t)r0 * w : NULL;
                j->curr = cpl[p] + (size_t)r0 * w;
                j->w = w; j->h = r1 - r0;
                j->yoff = r0; j->hfull = h;
                j->in = pl[p] + off0;
                j->in_size = (int32_t)(off1 - off0);
                int c420 = !!(rd_u32le(j->in) & PHDR_C420);
                if (p > 0 && !c420)
                    j->luma = cY + (size_t)r0 * w;
                else if (p > 0)
                    j->luma = cY;
                j->r = -1;
                if (p == 0) {
                    j->pub = &pb[st];
                } else if (!c420 && nsp[p] == K) {
                    j->wait = &pb[st];
                } else {
                    for (int b = 0; b < K; b++) {
                        j->pre[j->npre] = &pb[b];
                        j->pre_need[j->npre] = sh_l[b];
                        j->npre++;
                    }
                }
                ji++;
            }
        }
        if (bad || ji != nj) return -1;
        for (int i = 0; i < nj; i++) {
            if (pthread_create(&th[i], NULL, dec_plane_job_run,
                               &jobs[i]) == 0)
                nth++;
            else
                dec_plane_job_run(&jobs[i]);
        }
        for (int i = 0; i < nth; i++) pthread_join(th[i], NULL);
        for (int i = 0; i < nj; i++) if (jobs[i].r < 0) return -1;
        interleave_planes(cY, cCo, cCg, curr_ycocg, w, h);
        return in_size;
    }
    if ((in_buf[0] & 1) && in_size >= 13) {
        /* offset table: absolute byte offsets of each plane stream.
           All three planes decode on parallel threads; chroma leaves
           that read luma (CfL / PALT / NN tap) pipeline-wait on the
           luma thread's row watermark inside dec_leaf_op. */
        const uint8_t *py_in  = in_buf + rd_u32le(in_buf + 1);
        const uint8_t *pco_in = in_buf + rd_u32le(in_buf + 5);
        const uint8_t *pcg_in = in_buf + rd_u32le(in_buf + 9);
        if (py_in < in_buf + 13 || pco_in <= py_in || pcg_in <= pco_in ||
            pcg_in >= in_buf + in_size) return -1;
        dec_pb_t pb; atomic_init(&pb.rows_done, 0);
        dec_plane_job jy = {
            prev_ycocg ? pY : NULL, NULL, cY, w, h,
            py_in, (int32_t)(pco_in - py_in),
            NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            &pb, NULL, -1 };
        dec_plane_job jco = {
            prev_ycocg ? pCo : NULL, cY, cCo, w, h,
            pco_in, (int32_t)(pcg_in - pco_in),
            NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            NULL, &pb, -1 };
        dec_plane_job jcg = {
            prev_ycocg ? pCg : NULL, cY, cCg, w, h,
            pcg_in, (int32_t)(in_buf + in_size - pcg_in),
            NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            NULL, &pb, -1 };
        pthread_t ty, tco;
        /* luma must run somewhere before chroma waiters can finish:
           if its thread fails to spawn, run it inline first */
        int hy = pthread_create(&ty, NULL, dec_plane_job_run, &jy) == 0;
        if (!hy) dec_plane_job_run(&jy);
        int hco = pthread_create(&tco, NULL, dec_plane_job_run, &jco) == 0;
        dec_plane_job_run(&jcg);
        if (hy) pthread_join(ty, NULL);
        if (hco) pthread_join(tco, NULL);
        if (!hco) dec_plane_job_run(&jco);
        if (jy.r < 0 || jco.r < 0 || jcg.r < 0) return -1;
        interleave_planes(cY, cCo, cCg, curr_ycocg, w, h);
        return in_size;
    }

    const uint8_t *ip = in_buf + 1;
    int32_t left = in_size - 1;
    r = dec_plane_prog(prev_ycocg ? pY  : NULL, cY , w, h, NULL, ip, left,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, 0, 0);
    if (r < 0) return -1; ip += r; left -= r;
    r = dec_plane_prog(prev_ycocg ? pCo : NULL, cCo, w, h, cY,  ip, left,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, 0, 0);
    if (r < 0) return -1; ip += r; left -= r;
    r = dec_plane_prog(prev_ycocg ? pCg : NULL, cCg, w, h, cY,  ip, left,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, 0, 0);
    if (r < 0) return -1; ip += r; left -= r;

    interleave_planes(cY, cCo, cCg, curr_ycocg, w, h);
    return (int32_t)(ip - in_buf);
}

int32_t zv_encode_plane_stripe_v3(const int16_t *prev_plane,
                                  int16_t *curr_plane,
                                  const int16_t *luma_plane,
                                  int w, int stripe_h,
                                  uint8_t *out_buf, int32_t out_cap)
{
    return enc_plane_stream_v3(prev_plane, curr_plane, w, stripe_h,
                          luma_plane, out_buf, out_cap, 1, 1, NULL,
                          NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1);
}

int32_t zv_encode_plane_stripe_v3q(const int16_t *prev_plane,
                                   int16_t *curr_plane,
                                   const int16_t *luma_plane,
                                   int w, int stripe_h,
                                   uint8_t *out_buf, int32_t out_cap,
                                   int eps, int eps_hi, int16_t *recon_out)
{
    int qs = eps > 0 ? 2 * eps + 1 : 1;
    int qs_hi = eps_hi > eps ? 2 * eps_hi + 1 : qs;
    if (qs > 255) qs = 255;
    if (qs_hi > 255) qs_hi = 255;
    if (qs == 1)
        return zv_encode_plane_stripe_v3(prev_plane, curr_plane,
                                         luma_plane, w, stripe_h,
                                         out_buf, out_cap);
    return enc_plane_stream_v3(prev_plane, curr_plane, w, stripe_h,
                               luma_plane, out_buf, out_cap,
                               qs, qs_hi, recon_out, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, 1);
}

int32_t zv_decode_plane_stripe_v3(const int16_t *prev_plane,
                                  int16_t *curr_plane,
                                  const int16_t *luma_plane,
                                  int w, int stripe_h,
                                  const uint8_t *in_buf, int32_t in_size)
{
    return dec_plane_prog(prev_plane, curr_plane, w, stripe_h,
                          luma_plane, in_buf, in_size,
                          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                          NULL, NULL, 0, 0);
}

/* stateful frame encode: identical to v3 (lossless, parallel planes)
   but each plane carries a persistent leaf-op table + LLP state, so
   OP_PSAME / WSAME can replay the previous frame's program.  The state
   blob is updated in place and must persist across the sequence. */
int32_t zv_encode_frame_v3s(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                            int w, int h, uint8_t *out_buf, int32_t out_cap,
                            void *state)
{
    if (out_cap < 1 || !state) return -1;
    zv3s_t *s = (zv3s_t *)state;
    if (s->magic != ZV3S_MAGIC || s->w != w || s->h != h) return -1;
    vop_t *ptabs = (vop_t *)(s + 1);

    size_t n = (size_t)w * h;
    int16_t *scr = (int16_t *)tls_get(TLS_PLANES6, 6 * n * sizeof(int16_t));
    if (!scr) return -1;
    int16_t *pY  = scr,     *pCo = scr + n,   *pCg = scr + 2 * n;
    int16_t *cY  = scr + 3 * n, *cCo = scr + 4 * n, *cCg = scr + 5 * n;

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);
    deinterleave_planes(curr_ycocg, cY, cCo, cCg, w, h);

    int32_t pcap = (int32_t)(8 * n + 4096);
    if (pcap > out_cap - 1) pcap = out_cap - 1;
    uint8_t *pbm = (uint8_t *)tls_get(TLS_PB, 3 * (size_t)pcap);
    if (!pbm) return -1;
    uint8_t *pb[3] = { pbm, pbm + pcap, pbm + 2 * (size_t)pcap };
    /* intra-plane stripes: independent substreams let the decoder run
       tile rows in parallel, but each stripe pays rANS stream headers
       plus program-run restarts (~30B/stripe at 720p - ~25% of a small
       inter frame, ~18KB on a keyframe), and the three plane-parallel
       decode lanes already cover the latency case.  Default stays
       single-stripe for maximum compression; ZNS opts back into
       stripe-framed streams for deployments that need the extra
       decode fan-out. */
    int nty_f = (h + TILE_SZ - 1) / TILE_SZ;
    int nst = 1; (void)nty_f;
    {   nst = ZE(zns); }
    if (nst < 1) nst = 1;
    if (nst > 10) nst = 10;
    pjob_t jobs[3];
    const int16_t *pp[3] = { prev_ycocg ? pY : NULL, prev_ycocg ? pCo : NULL,
                             prev_ycocg ? pCg : NULL };
    int16_t *cc[3] = { cY, cCo, cCg };
    for (int i = 0; i < 3; i++) {
        jobs[i].prev = pp[i]; jobs[i].curr = cc[i];
        jobs[i].luma = i ? cY : NULL;
        jobs[i].w = w; jobs[i].h = h;
        jobs[i].qs = 1; jobs[i].qs_hi = 1; jobs[i].recon = NULL;
        jobs[i].nstripes = nst;
        jobs[i].ptab = ptabs + (size_t)i * s->nti;
        jobs[i].Wprev = s->Wprev[i];
        jobs[i].wvalid = &s->wvalid[i];
        jobs[i].pm = s->pm[i]; jobs[i].rm = s->rm[i];
        jobs[i].mvalid = &s->mvalid[i];
        jobs[i].ps = &s->ps[i];
        jobs[i].buf = pb[i]; jobs[i].cap = pcap; jobs[i].len = -1;
    }

    if (ZE(zseq)) {
        for (int i = 0; i < 3; i++) enc_plane_job(&jobs[i]);
    } else {
        pthread_t th[3]; int nth = 0;
        for (int i = 0; i < 3; i++) {
            if (enc_spawn(&th[i], enc_plane_job, &jobs[i]))
                nth++;
            else
                enc_plane_job(&jobs[i]);     /* fallback: run inline */
        }
        for (int i = 0; i < nth; i++) pthread_join(th[i], NULL);
    }

    /* Single-stripe frames use the serial framing (flag byte 0, planes
       back to back). The 12-byte offset table only exists so a decoder
       can start the three planes at once; on the default one-stripe
       path that table is larger than a whole all-RUN inter frame, and
       zv_decode_frame_v3s already walks flag-0 streams. Stripe frames
       (nst>1, ZNS) keep bit0+bit1 and the offset table. Old offset-table
       streams still decode: this only changes what the encoder writes. */
    int framed = nst > 1;
    int hdr = framed ? 13 : 1;
    if (out_cap < hdr) return -1;
    out_buf[0] = framed ? 3 : 0;
    uint8_t *op = out_buf + hdr;
    for (int i = 0; i < 3; i++) {
        if (jobs[i].len < 0 ||
            jobs[i].len > out_cap - (int32_t)(op - out_buf)) return -1;
        if (framed)
            wr_u32le(out_buf + 1 + i * 4, (uint32_t)(op - out_buf));
        memcpy(op, pb[i], (size_t)jobs[i].len);
        op += jobs[i].len;
    }
    return (int32_t)(op - out_buf);
}

/* stateful decode: mirrors zv_encode_frame_v3s.  Chroma CfL consumes the
   freshly decoded luma, so Y must complete first; Co and Cg are then
   independent and decode on parallel threads.  Streams written by this
   encoder set flag byte bit0 and carry a 3-entry u32 offset table so the
   chroma offsets are known without consuming Co; older byte0=0 streams
   fall back to the serial walk. */
int32_t zv_decode_frame_v3s(const int16_t *prev_ycocg, int16_t *curr_ycocg,
                            int w, int h, const uint8_t *in_buf,
                            int32_t in_size, void *state)
{
    if (in_size < 1 || !state) return -1;
    zv3s_t *s = (zv3s_t *)state;
    if (s->magic != ZV3S_MAGIC || s->w != w || s->h != h) return -1;
    vop_t *ptabs = (vop_t *)(s + 1);

    size_t n = (size_t)w * h;
    int16_t *scr = (int16_t *)tls_get(TLS_PLANES6, 6 * n * sizeof(int16_t));
    if (!scr) return -1;
    int16_t *pY  = scr,     *pCo = scr + n,   *pCg = scr + 2 * n;
    int16_t *cY  = scr + 3 * n, *cCo = scr + 4 * n, *cCg = scr + 5 * n;

    if (prev_ycocg) deinterleave_planes(prev_ycocg, pY, pCo, pCg, w, h);

    int32_t r;
    if ((in_buf[0] & 3) == 3 && in_size >= 13) {
        /* stripe-framed planes: each plane stream carries
           [u8 ns][u32 x ns offsets][ns substreams].  Every stripe is a
           self-contained program+residual stream warm-started from the
           frame's carried state; only each plane's LAST stripe commits
           its post-state.  Luma stripes publish row watermarks; chroma
           stripe s leaf-waits on luma stripe s (fractional boundaries
           keep the row spaces aligned). */
        const uint8_t *pl[3] = { in_buf + rd_u32le(in_buf + 1),
                                 in_buf + rd_u32le(in_buf + 5),
                                 in_buf + rd_u32le(in_buf + 9) };
        if (pl[0] < in_buf + 13 || pl[1] <= pl[0] || pl[2] <= pl[1] ||
            pl[2] >= in_buf + in_size) return -1;
        int32_t pl_len[3] = { (int32_t)(pl[1] - pl[0]),
                              (int32_t)(pl[2] - pl[1]),
                              (int32_t)(in_buf + in_size - pl[2]) };
        int nsp[3], nj = 0;
        for (int p = 0; p < 3; p++) {
            if (pl_len[p] < 1) return -1;
            nsp[p] = pl[p][0];
            if (nsp[p] < 1 || nsp[p] > 12 ||
                pl_len[p] < 1 + 4 * nsp[p]) return -1;
            nj += nsp[p];
        }
        int K = nsp[0];
        dec_pb_t pb[12];
        for (int i = 0; i < K; i++) atomic_init(&pb[i].rows_done, 0);
        int ntx = (w + TILE_SZ - 1) / TILE_SZ;
        int nty = (h + TILE_SZ - 1) / TILE_SZ;
        /* luma stripe pixel heights (c420 waiters need them) */
        int sh_l[12];
        for (int i = 0; i < K; i++) {
            int a, b; stripe_bounds(nty, K, i, h, &a, &b);
            sh_l[i] = b - a;
        }
        /* per-stripe scratch state for all but each plane's last stripe */
        typedef struct {
            mdl64_t pm[NPROG_MDL];
            mdl64_t rm[N_MDL];
            int16_t Wp[N_NNW * N_NNC];
            int wv, mv;
            pstate_t ps;
        } sst_t;
        int nsets = nsp[0] + nsp[1] + nsp[2];
        sst_t *sst = (sst_t *)tls_get(TLS_SST,
                                    (size_t)nsets * sizeof(sst_t));
        if (!sst) return -1;
        int last_si[3] = { -1, -1, -1 };
        dec_plane_job jobs[36];
        pthread_t th[36];
        int nth = 0, ji = 0, si = 0, bad = 0;
        const int16_t *ppl[3] = { prev_ycocg ? pY : NULL,
                                  prev_ycocg ? pCo : NULL,
                                  prev_ycocg ? pCg : NULL };
        int16_t *cpl[3] = { cY, cCo, cCg };
        for (int p = 0; p < 3 && !bad; p++) {
            for (int st = 0; st < nsp[p]; st++) {
                uint32_t off0 = rd_u32le(pl[p] + 1 + 4 * st);
                uint32_t off1 = (st + 1 < nsp[p])
                                ? rd_u32le(pl[p] + 5 + 4 * st)
                                : (uint32_t)pl_len[p];
                if (off0 < 1 + 4 * (uint32_t)nsp[p] || off1 <= off0 ||
                    off1 > (uint32_t)pl_len[p]) { bad = 1; break; }
                int r0, r1;
                if (!stripe_bounds(nty, nsp[p], st, h, &r0, &r1)) {
                    bad = 1; break;
                }
                dec_plane_job *j = &jobs[ji];
                memset(j, 0, sizeof(*j));
                j->prev = ppl[p] ? ppl[p] + (size_t)r0 * w : NULL;
                j->curr = cpl[p] + (size_t)r0 * w;
                j->w = w; j->h = r1 - r0;
                j->yoff = r0; j->hfull = h;
                j->in = pl[p] + off0;
                j->in_size = (int32_t)(off1 - off0);
                j->ptab = ptabs + (size_t)p * s->nti
                        + (size_t)(r0 / TILE_SZ) * ntx;
                int c420 = !!(rd_u32le(j->in) & PHDR_C420);
                if (p > 0 && !c420)
                    j->luma = cY + (size_t)r0 * w;
                else if (p > 0)
                    j->luma = cY;          /* c420 decimates whole luma */
                j->r = -1;
                {
                    sst_t *t = &sst[si];
                    j->Wprev = t->Wp; j->wvalid = &t->wv;
                    j->pm = t->pm; j->rm = t->rm;
                    j->mvalid = &t->mv; j->ps = &t->ps;
                    j->pm_src = s->pm[p]; j->rm_src = s->rm[p];
                    j->Wp_src = s->Wprev[p]; j->ps_src = &s->ps[p];
                    j->wv_src = s->wvalid[p]; j->mv_src = s->mvalid[p];
                    if (st == nsp[p] - 1) last_si[p] = si;
                    si++;
                }
                if (p == 0) {
                    j->pub = &pb[st];
                } else if (!c420 && nsp[p] == K) {
                    j->wait = &pb[st];
                } else {
                    for (int b = 0; b < K; b++) {
                        j->pre[j->npre] = &pb[b];
                        j->pre_need[j->npre] = sh_l[b];
                        j->npre++;
                    }
                }
                ji++;
            }
        }
        if (bad || ji != nj) return -1;
        /* luma jobs were built first (plane 0), so every pb producer is
           already launched before chroma waiters appear */
        for (int i = 0; i < nj; i++) {
            if (pthread_create(&th[i], NULL, dec_plane_job_run,
                               &jobs[i]) == 0)
                nth++;
            else
                dec_plane_job_run(&jobs[i]);
        }
        for (int i = 0; i < nth; i++) pthread_join(th[i], NULL);
        int fail = 0;
        for (int i = 0; i < nj; i++) if (jobs[i].r < 0) fail = 1;
        if (fail) return -1;
        /* commit each plane's last stripe's post-state into the carried
           state (mirrors the encoder's last-stripe back-patch) */
        for (int p = 0; p < 3; p++) {
            const sst_t *t = &sst[last_si[p]];
            memcpy(s->pm[p], t->pm, NPROG_MDL * sizeof(mdl64_t));
            memcpy(s->rm[p], t->rm, N_MDL * sizeof(mdl64_t));
            memcpy(s->Wprev[p], t->Wp, N_NNW * N_NNC * sizeof(int16_t));
            s->wvalid[p] = t->wv; s->mvalid[p] = t->mv;
            s->ps[p] = t->ps;
        }
        interleave_planes(cY, cCo, cCg, curr_ycocg, w, h);
        return in_size;
    }
    if ((in_buf[0] & 1) && in_size >= 13) {
        /* offset table: absolute byte offsets of each plane stream.
           All three planes decode on parallel threads; chroma leaves
           that read luma (CfL / PALT / NN tap) pipeline-wait on the
           luma thread's row watermark inside dec_leaf_op. */
        const uint8_t *py_in  = in_buf + rd_u32le(in_buf + 1);
        const uint8_t *pco_in = in_buf + rd_u32le(in_buf + 5);
        const uint8_t *pcg_in = in_buf + rd_u32le(in_buf + 9);
        if (py_in < in_buf + 13 || pco_in <= py_in || pcg_in <= pco_in ||
            pcg_in >= in_buf + in_size) return -1;
        dec_pb_t pb; atomic_init(&pb.rows_done, 0);
        dec_plane_job jy = {
            prev_ycocg ? pY : NULL, NULL, cY, w, h,
            py_in, (int32_t)(pco_in - py_in),
            ptabs + (size_t)0 * s->nti, s->Wprev[0],
            &s->wvalid[0], s->pm[0], s->rm[0], &s->mvalid[0],
            &s->ps[0], &pb, NULL, -1 };
        dec_plane_job jco = {
            prev_ycocg ? pCo : NULL, cY, cCo, w, h,
            pco_in, (int32_t)(pcg_in - pco_in),
            ptabs + (size_t)1 * s->nti, s->Wprev[1], &s->wvalid[1],
            s->pm[1], s->rm[1], &s->mvalid[1], &s->ps[1],
            NULL, &pb, -1 };
        dec_plane_job jcg = {
            prev_ycocg ? pCg : NULL, cY, cCg, w, h,
            pcg_in, (int32_t)(in_buf + in_size - pcg_in),
            ptabs + (size_t)2 * s->nti, s->Wprev[2], &s->wvalid[2],
            s->pm[2], s->rm[2], &s->mvalid[2], &s->ps[2],
            NULL, &pb, -1 };
        pthread_t ty, tco;
        /* luma must run somewhere before chroma waiters can finish:
           if its thread fails to spawn, run it inline first */
        int hy = pthread_create(&ty, NULL, dec_plane_job_run, &jy) == 0;
        if (!hy) dec_plane_job_run(&jy);
        int hco = pthread_create(&tco, NULL, dec_plane_job_run, &jco) == 0;
        dec_plane_job_run(&jcg);
        if (hy) pthread_join(ty, NULL);
        if (hco) pthread_join(tco, NULL);
        if (!hco) dec_plane_job_run(&jco);
        if (jy.r < 0 || jco.r < 0 || jcg.r < 0) return -1;
        interleave_planes(cY, cCo, cCg, curr_ycocg, w, h);
        return in_size;
    }

    const uint8_t *ip = in_buf + 1;
    int32_t left = in_size - 1;
    r = dec_plane_prog(prev_ycocg ? pY  : NULL, cY , w, h, NULL, ip, left,
                       ptabs + (size_t)0 * s->nti, s->Wprev[0],
                       &s->wvalid[0], s->pm[0], s->rm[0], &s->mvalid[0],
                       &s->ps[0], NULL, NULL, 0, 0);
    if (r < 0) return -1; ip += r; left -= r;
    r = dec_plane_prog(prev_ycocg ? pCo : NULL, cCo, w, h, cY,  ip, left,
                       ptabs + (size_t)1 * s->nti, s->Wprev[1],
                       &s->wvalid[1], s->pm[1], s->rm[1], &s->mvalid[1],
                       &s->ps[1], NULL, NULL, 0, 0);
    if (r < 0) return -1; ip += r; left -= r;
    r = dec_plane_prog(prev_ycocg ? pCg : NULL, cCg, w, h, cY,  ip, left,
                       ptabs + (size_t)2 * s->nti, s->Wprev[2],
                       &s->wvalid[2], s->pm[2], s->rm[2], &s->mvalid[2],
                       &s->ps[2], NULL, NULL, 0, 0);
    if (r < 0) return -1; ip += r; left -= r;

    interleave_planes(cY, cCo, cCg, curr_ycocg, w, h);
    return (int32_t)(ip - in_buf);
}

