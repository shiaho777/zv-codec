
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    uint8_t *buf; size_t cap, pos; uint64_t acc; int nbits;
} bw_t;

static void bw_init(bw_t *bw, uint8_t *buf, size_t cap) {
    bw->buf = buf; bw->cap = cap; bw->pos = 0; bw->acc = 0; bw->nbits = 0;
}
static void bw_put(bw_t *bw, uint64_t v, int n) {
    if (n <= 0) return;
    uint64_t m = (n == 64) ? ~(uint64_t)0 : (((uint64_t)1 << n) - 1);
    bw->acc = (bw->acc << n) | (v & m); bw->nbits += n;
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
    const uint8_t *buf; size_t size, pos; uint64_t acc; int nbits;
} br_t;

static void br_init(br_t *br, const uint8_t *buf, size_t size) {
    br->buf = buf; br->size = size; br->pos = 0; br->acc = 0; br->nbits = 0;
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

typedef struct { uint32_t low, high; uint32_t pending; bw_t *bw; } ac_enc_t;
typedef struct { uint32_t low, high, code; br_t *br; } ac_dec_t;

#define AC_QTR  0x40000000u
#define AC_HALF 0x80000000u
#define AC_3Q   0xC0000000u
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
    if (bit == 0) e->high = bound - 1; else e->low = bound;
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
    d->low = 0; d->high = 0xFFFFFFFFu; d->br = br; d->code = 0;
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
    else                 { bit = 1; d->low = bound; }
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

static inline uint32_t zigzag_enc(int v) {
    return (v >= 0) ? ((uint32_t)v << 1) : (((uint32_t)(-v) << 1) - 1);
}
static inline int zigzag_dec(uint32_t u) {
    return (u & 1u) ? -(int)((u >> 1) + 1) : (int)(u >> 1);
}

#define NPRED 3
#define N_CTX_AUDIO 8
#define UNARY_LIMIT 28
#define RESET_N 256

static inline int amag_class(int abs_r) {
    if (abs_r == 0) return 0;
    if (abs_r <= 2) return 1;
    if (abs_r <= 8) return 2;
    if (abs_r <= 32) return 3;
    if (abs_r <= 128) return 4;
    if (abs_r <= 512) return 5;
    if (abs_r <= 2048) return 6;
    return 7;
}

typedef struct {
    int64_t A[N_CTX_AUDIO][NPRED];
    int32_t N[N_CTX_AUDIO];
} astate_t;

static void astate_init(astate_t *s) {
    for (int c = 0; c < N_CTX_AUDIO; c++) {
        for (int p = 0; p < NPRED; p++) s->A[c][p] = 4;
        s->N[c] = 1;
    }
}

static inline int compute_k(int64_t A, int32_t N) {
    int k = 0;
    while (((int64_t)N << k) < A && k < 28) k++;
    return k;
}

static inline void update_actx(astate_t *s, int ctx, const int abs_p[NPRED]) {
    for (int p = 0; p < NPRED; p++) s->A[ctx][p] += abs_p[p];
    s->N[ctx]++;
    if (s->N[ctx] >= RESET_N) {
        for (int p = 0; p < NPRED; p++) s->A[ctx][p] >>= 1;
        s->N[ctx] >>= 1;
    }
}

typedef struct {
    uint16_t res[N_CTX_AUDIO][NPRED][UNARY_LIMIT];
} amodels_t;

static void amodels_init(amodels_t *m) {
    for (int c = 0; c < N_CTX_AUDIO; c++)
        for (int p = 0; p < NPRED; p++)
            for (int i = 0; i < UNARY_LIMIT; i++) m->res[c][p][i] = 0x8000u;
}

static void bypass_eg0_put(ac_enc_t *e, uint32_t v) {
    uint32_t m = v + 1;
    int L = 0; uint32_t t = m >> 1;
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

static void encode_channel(ac_enc_t *ac, const int32_t *samples, int n) {
    astate_t S; astate_init(&S);
    amodels_t M; amodels_init(&M);

    int prev_abs = 0;
    int a_prev = 0, b_prev = 0, c_prev = 0;

    for (int i = 0; i < n; i++) {
        int x = samples[i];
        int p[NPRED];
        p[0] = a_prev;
        p[1] = 2 * a_prev - b_prev;
        p[2] = 3 * a_prev - 3 * b_prev + c_prev;
        int r[NPRED];
        for (int k = 0; k < NPRED; k++) r[k] = x - p[k];

        int ctx = amag_class(prev_abs);

        int mode = 0;
        int64_t bestA = S.A[ctx][0];
        for (int k = 1; k < NPRED; k++) {
            if (S.A[ctx][k] < bestA) { bestA = S.A[ctx][k]; mode = k; }
        }
        int kbits = compute_k(bestA, S.N[ctx]);
        sym_put(ac, zigzag_enc(r[mode]), kbits, M.res[ctx][mode]);

        int abs_p[NPRED];
        for (int k = 0; k < NPRED; k++) abs_p[k] = r[k] < 0 ? -r[k] : r[k];
        update_actx(&S, ctx, abs_p);
        prev_abs = abs_p[mode];

        c_prev = b_prev; b_prev = a_prev; a_prev = x;
    }
}

static void decode_channel(ac_dec_t *ac, int32_t *samples, int n) {
    astate_t S; astate_init(&S);
    amodels_t M; amodels_init(&M);

    int prev_abs = 0;
    int a_prev = 0, b_prev = 0, c_prev = 0;

    for (int i = 0; i < n; i++) {
        int p[NPRED];
        p[0] = a_prev;
        p[1] = 2 * a_prev - b_prev;
        p[2] = 3 * a_prev - 3 * b_prev + c_prev;

        int ctx = amag_class(prev_abs);
        int mode = 0;
        int64_t bestA = S.A[ctx][0];
        for (int k = 1; k < NPRED; k++) {
            if (S.A[ctx][k] < bestA) { bestA = S.A[ctx][k]; mode = k; }
        }
        int kbits = compute_k(bestA, S.N[ctx]);
        uint32_t u = sym_get(ac, kbits, M.res[ctx][mode]);
        int rmode = zigzag_dec(u);
        int x = p[mode] + rmode;
        samples[i] = x;

        int r[NPRED], abs_p[NPRED];
        for (int k = 0; k < NPRED; k++) {
            r[k] = x - p[k];
            abs_p[k] = r[k] < 0 ? -r[k] : r[k];
        }
        update_actx(&S, ctx, abs_p);
        prev_abs = abs_p[mode];

        c_prev = b_prev; b_prev = a_prev; a_prev = x;
    }
}

/* ---- v1: 6 linear predictors + wasted-bit shift.  Stream prefix
   'ZA' 0x01 wasted0 [wasted1] then the same AC payload shape.  Old
   streams without the magic still decode via encode_channel(). ---- */
#define NPRED_V1 6
#define ZA_VER1  1
#define ZA_VER   2

typedef struct {
    int64_t A[N_CTX_AUDIO][NPRED_V1];
    int32_t N[N_CTX_AUDIO];
} astate_v1_t;

typedef struct {
    uint16_t res[N_CTX_AUDIO][NPRED_V1][UNARY_LIMIT];
} amodels_v1_t;

static void astate_v1_init(astate_v1_t *s) {
    for (int c = 0; c < N_CTX_AUDIO; c++) {
        for (int p = 0; p < NPRED_V1; p++) s->A[c][p] = 4;
        s->N[c] = 1;
    }
}
static void amodels_v1_init(amodels_v1_t *m) {
    for (int c = 0; c < N_CTX_AUDIO; c++)
        for (int p = 0; p < NPRED_V1; p++)
            for (int i = 0; i < UNARY_LIMIT; i++) m->res[c][p][i] = 0x8000u;
}
static inline void update_actx_v1(astate_v1_t *s, int ctx, const int abs_p[NPRED_V1]) {
    for (int p = 0; p < NPRED_V1; p++) s->A[ctx][p] += abs_p[p];
    s->N[ctx]++;
    if (s->N[ctx] >= RESET_N) {
        for (int p = 0; p < NPRED_V1; p++) s->A[ctx][p] >>= 1;
        s->N[ctx] >>= 1;
    }
}
static inline void preds_v1(int a, int b, int c, int d, int e, int64_t p[NPRED_V1]) {
    p[0] = a;
    p[1] = 2LL * a - b;
    p[2] = 3LL * a - 3LL * b + c;
    p[3] = 4LL * a - 6LL * b + 4LL * c - d;
    p[4] = 5LL * a - 10LL * b + 10LL * c - 5LL * d + e;
    p[5] = b;
}
static inline int resid_v1(int x, int64_t p) {
    int64_t r = (int64_t)x - p;
    if (r >  262143) r =  262143;
    if (r < -262144) r = -262144;
    return (int)r;
}

static void encode_channel_v1(ac_enc_t *ac, const int32_t *samples, int n) {
    astate_v1_t S; astate_v1_init(&S);
    amodels_v1_t M; amodels_v1_init(&M);
    int prev_abs = 0;
    int a = 0, b = 0, c = 0, d = 0, e = 0;
    for (int i = 0; i < n; i++) {
        int x = samples[i];
        int64_t pv[NPRED_V1];
        preds_v1(a, b, c, d, e, pv);
        int r[NPRED_V1], abs_p[NPRED_V1];
        for (int k = 0; k < NPRED_V1; k++) {
            r[k] = resid_v1(x, pv[k]);
            abs_p[k] = r[k] < 0 ? -r[k] : r[k];
        }
        int ctx = amag_class(prev_abs);
        int mode = 0;
        int64_t bestA = S.A[ctx][0];
        for (int k = 1; k < NPRED_V1; k++)
            if (S.A[ctx][k] < bestA) { bestA = S.A[ctx][k]; mode = k; }
        int kbits = compute_k(bestA, S.N[ctx]);
        sym_put(ac, zigzag_enc(r[mode]), kbits, M.res[ctx][mode]);
        update_actx_v1(&S, ctx, abs_p);
        prev_abs = abs_p[mode];
        e = d; d = c; c = b; b = a; a = x;
    }
}

static void decode_channel_v1(ac_dec_t *ac, int32_t *samples, int n) {
    astate_v1_t S; astate_v1_init(&S);
    amodels_v1_t M; amodels_v1_init(&M);
    int prev_abs = 0;
    int a = 0, b = 0, c = 0, d = 0, e = 0;
    for (int i = 0; i < n; i++) {
        int64_t pv[NPRED_V1];
        preds_v1(a, b, c, d, e, pv);
        int ctx = amag_class(prev_abs);
        int mode = 0;
        int64_t bestA = S.A[ctx][0];
        for (int k = 1; k < NPRED_V1; k++)
            if (S.A[ctx][k] < bestA) { bestA = S.A[ctx][k]; mode = k; }
        int kbits = compute_k(bestA, S.N[ctx]);
        uint32_t u = sym_get(ac, kbits, M.res[ctx][mode]);
        int rmode = zigzag_dec(u);
        int x = (int)(pv[mode] + rmode);
        samples[i] = x;
        int abs_p[NPRED_V1];
        for (int k = 0; k < NPRED_V1; k++) {
            int r = resid_v1(x, pv[k]);
            abs_p[k] = r < 0 ? -r : r;
        }
        update_actx_v1(&S, ctx, abs_p);
        prev_abs = abs_p[mode];
        e = d; d = c; c = b; b = a; a = x;
    }
}

static int wasted_bits(const int32_t *s, int n) {
    int w = 15, any = 0;
    for (int i = 0; i < n; i++) {
        int v = s[i];
        if (v == 0) continue;
        any = 1;
        int tz = 0;
        while (tz < 15 && ((v >> tz) & 1) == 0) tz++;
        if (tz < w) w = tz;
    }
    return any ? w : 0;
}

/* ---- v2: per-block LPC / FLAC-style fixed pred + mid-side.
   'ZA' 0x02 then AC: for each 4096-sample block, stereo mode,
   then per channel wasted/method/coeffs/rice-k + residuals.
   History of 12 original samples carries across blocks. ---- */
#define ABLK     4096
#define MAX_LPC  12
#define LPC_Q    10
#define LPC_ROUND (1 << (LPC_Q - 1))
#define MAX_PORD 3
#define MAX_PART (1 << MAX_PORD)

static void ac_put_bits(ac_enc_t *e, uint32_t v, int n) {
    for (int i = n - 1; i >= 0; i--)
        ac_enc_bit(e, (int)((v >> i) & 1u), AC_BYPASS);
}
static uint32_t ac_get_bits(ac_dec_t *d, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; i++)
        v = (v << 1) | (uint32_t)ac_dec_bit(d, AC_BYPASS);
    return v;
}

typedef struct {
    uint16_t res[N_CTX_AUDIO][UNARY_LIMIT];
    int prev_abs;
} rice_mdl_t;

static void rice_mdl_init(rice_mdl_t *m) {
    for (int c = 0; c < N_CTX_AUDIO; c++)
        for (int i = 0; i < UNARY_LIMIT; i++) m->res[c][i] = 0x8000u;
    m->prev_abs = 0;
}

static inline int32_t samp_at(const int32_t *hist, const int32_t *x, int i) {
    return (i >= 0) ? x[i] : hist[MAX_LPC + i];
}

static void hist_update(int32_t *hist, const int32_t *x, int n) {
    if (n >= MAX_LPC)
        memcpy(hist, x + n - MAX_LPC, MAX_LPC * sizeof(int32_t));
    else if (n > 0) {
        memmove(hist, hist + n, (size_t)(MAX_LPC - n) * sizeof(int32_t));
        memcpy(hist + MAX_LPC - n, x, (size_t)n * sizeof(int32_t));
    }
}

static void residuals_fixed(const int32_t *h, const int32_t *x, int n,
                            int order, int32_t *r) {
    for (int i = 0; i < n; i++) {
        int32_t a = samp_at(h, x, i - 1);
        int32_t b = samp_at(h, x, i - 2);
        int32_t c = samp_at(h, x, i - 3);
        int32_t d = samp_at(h, x, i - 4);
        int64_t p = 0;
        switch (order) {
            case 1: p = a; break;
            case 2: p = 2LL * a - b; break;
            case 3: p = 3LL * a - 3LL * b + c; break;
            case 4: p = 4LL * a - 6LL * b + 4LL * c - d; break;
            default: p = 0; break;
        }
        r[i] = (int32_t)((int64_t)x[i] - p);
    }
}

static void residuals_lpc(const int32_t *h, const int32_t *x, int n,
                          const int16_t *q, int order, int32_t *r) {
    for (int i = 0; i < n; i++) {
        int64_t s = LPC_ROUND;
        for (int k = 1; k <= order; k++)
            s += (int64_t)q[k - 1] * samp_at(h, x, i - k);
        r[i] = x[i] - (int32_t)(s >> LPC_Q);
    }
}

static void autocorr_i(const int32_t *x, int n, int maxo, double *r) {
    for (int lag = 0; lag <= maxo; lag++) {
        int64_t s = 0;
        for (int i = lag; i < n; i++)
            s += (int64_t)x[i] * x[i - lag];
        r[lag] = (double)s;
    }
}

/* Hann window on analysis only; quantized LPC still runs on the samples. */
static void autocorr_hann(const int32_t *x, int n, int maxo, double *r) {
    double wx[ABLK];
    if (n < 2) { autocorr_i(x, n, maxo, r); return; }
    const double s = 6.28318530717958647692 / (double)(n - 1);
    for (int i = 0; i < n; i++)
        wx[i] = (double)x[i] * (0.5 - 0.5 * cos(s * i));
    for (int lag = 0; lag <= maxo; lag++) {
        double acc = 0;
        for (int i = lag; i < n; i++)
            acc += wx[i] * wx[i - lag];
        r[lag] = acc;
    }
}

/* a_out[ord][1..ord] = LPC coeffs for that order. Returns highest stable order. */
static int levinson_all(const double *r, int maxo, double a_out[][MAX_LPC + 1]) {
    double a[MAX_LPC + 1];
    for (int i = 0; i <= maxo; i++) a[i] = 0;
    double e = r[0];
    if (e < 1.0) return 0;
    int last = 0;
    for (int i = 1; i <= maxo; i++) {
        double lambda = r[i];
        for (int j = 1; j < i; j++) lambda -= a[j] * r[i - j];
        double k = lambda / e;
        if (k > 0.999 || k < -0.999) break;
        double anew[MAX_LPC + 1];
        anew[i] = k;
        for (int j = 1; j < i; j++) anew[j] = a[j] - k * a[i - j];
        for (int j = 1; j <= i; j++) a[j] = anew[j];
        e *= (1.0 - k * k);
        for (int j = 1; j <= i; j++) a_out[i][j] = a[j];
        last = i;
        if (e < 1.0) break;
    }
    return last;
}

static int rice_k_from(const int32_t *r, int n) {
    int64_t sa = 1;
    for (int i = 0; i < n; i++) {
        int v = r[i];
        sa += v < 0 ? -v : v;
    }
    int k = 0;
    while (((int64_t)n << k) < sa && k < 14) k++;
    return k;
}

static int64_t rice_bits_k(const int32_t *r, int n, int k) {
    int64_t b = 4; /* k side info */
    for (int i = 0; i < n; i++) {
        uint32_t u = zigzag_enc(r[i]);
        uint32_t q = u >> k;
        if (q > (uint32_t)UNARY_LIMIT) q = (uint32_t)UNARY_LIMIT + 8;
        b += (int64_t)q + 1 + k;
    }
    return b;
}

static int64_t best_partition_bits(const int32_t *r, int n, int *pord_out, int k_out[MAX_PART]) {
    int64_t best = (int64_t)1 << 60;
    int bp = 0, bk[MAX_PART];
    memset(bk, 0, sizeof bk);
    for (int po = 0; po <= MAX_PORD; po++) {
        int np = 1 << po;
        int plen = n >> po;
        if (po > 0 && plen < 32) continue;
        int64_t bits = 2;
        int ks[MAX_PART];
        memset(ks, 0, sizeof ks);
        for (int p = 0; p < np; p++) {
            int start = p * plen;
            int len = (p == np - 1) ? (n - start) : plen;
            ks[p] = rice_k_from(r + start, len);
            bits += rice_bits_k(r + start, len, ks[p]);
        }
        if (bits < best) {
            best = bits;
            bp = po;
            memcpy(bk, ks, sizeof bk);
        }
    }
    *pord_out = bp;
    memcpy(k_out, bk, sizeof bk);
    return best;
}

typedef struct {
    int wasted, is_lpc, order, pord;
    int16_t q[MAX_LPC];
    int k[MAX_PART];
    int64_t bits;
} chplan_t;

static void consider_res(chplan_t *best, const int32_t *res, int n,
                         int wasted, int is_lpc, int order, const int16_t *q) {
    int pord, kk[MAX_PART];
    int64_t rb = best_partition_bits(res, n, &pord, kk);
    int64_t bits = rb + 4 + 1 + 4 + 2; /* wasted, flag, order, pord */
    if (is_lpc) bits += 16LL * order;
    if (bits < best->bits) {
        best->bits = bits;
        best->wasted = wasted;
        best->is_lpc = is_lpc;
        best->order = order;
        best->pord = pord;
        memcpy(best->k, kk, sizeof kk);
        if (is_lpc && q)
            memcpy(best->q, q, (size_t)order * sizeof(int16_t));
    }
}

static void plan_channel(const int32_t *x, const int32_t *hist, int n, chplan_t *best) {
    best->wasted = 0; best->is_lpc = 0; best->order = 0; best->pord = 0;
    best->bits = (int64_t)1 << 60;
    memset(best->q, 0, sizeof best->q);
    memset(best->k, 0, sizeof best->k);

    int w = wasted_bits(x, n);
    int32_t xs[ABLK], hs[MAX_LPC], res[ABLK];
    for (int i = 0; i < n; i++) xs[i] = x[i] >> w;
    for (int i = 0; i < MAX_LPC; i++) hs[i] = hist[i] >> w;

    for (int ord = 0; ord <= 4; ord++) {
        residuals_fixed(hs, xs, n, ord, res);
        consider_res(best, res, n, w, 0, ord, NULL);
    }

    if (n >= 64) {
        double rxx[MAX_LPC + 1];
        double ao[MAX_LPC + 1][MAX_LPC + 1];
        for (int win = 0; win < 2; win++) {
            if (win == 0) autocorr_i(xs, n, MAX_LPC, rxx);
            else          autocorr_hann(xs, n, MAX_LPC, rxx);
            int maxo = levinson_all(rxx, MAX_LPC, ao);
            for (int ord = 1; ord <= maxo; ord++) {
                int16_t q[MAX_LPC];
                for (int k = 1; k <= ord; k++) {
                    int v = (int)lrint(ao[ord][k] * (double)(1 << LPC_Q));
                    if (v > 32767) v = 32767;
                    if (v < -32768) v = -32768;
                    q[k - 1] = (int16_t)v;
                }
                residuals_lpc(hs, xs, n, q, ord, res);
                consider_res(best, res, n, w, 1, ord, q);
            }
        }
    }
}

static void encode_res_part(ac_enc_t *ac, const int32_t *r, int n, int k,
                            rice_mdl_t *M) {
    int prev_abs = M->prev_abs;
    for (int i = 0; i < n; i++) {
        int ctx = amag_class(prev_abs);
        int v = r[i];
        int av = v < 0 ? -v : v;
        sym_put(ac, zigzag_enc(v), k, M->res[ctx]);
        prev_abs = av;
    }
    M->prev_abs = prev_abs;
}

static void decode_res_part(ac_dec_t *ac, int32_t *r, int n, int k,
                            rice_mdl_t *M) {
    int prev_abs = M->prev_abs;
    for (int i = 0; i < n; i++) {
        int ctx = amag_class(prev_abs);
        uint32_t u = sym_get(ac, k, M->res[ctx]);
        int v = zigzag_dec(u);
        r[i] = v;
        prev_abs = v < 0 ? -v : v;
    }
    M->prev_abs = prev_abs;
}

static void emit_plan(ac_enc_t *ac, const chplan_t *p, const int32_t *x,
                      const int32_t *hist, int n, rice_mdl_t *M) {
    ac_put_bits(ac, (uint32_t)p->wasted, 4);
    ac_enc_bit(ac, p->is_lpc, AC_BYPASS);
    ac_put_bits(ac, (uint32_t)p->order, 4);
    ac_put_bits(ac, (uint32_t)p->pord, 2);
    if (p->is_lpc)
        for (int i = 0; i < p->order; i++)
            ac_put_bits(ac, (uint16_t)p->q[i], 16);
    int np = 1 << p->pord;
    for (int i = 0; i < np; i++)
        ac_put_bits(ac, (uint32_t)p->k[i], 4);

    int32_t xs[ABLK], hs[MAX_LPC], res[ABLK];
    for (int i = 0; i < n; i++) xs[i] = x[i] >> p->wasted;
    for (int i = 0; i < MAX_LPC; i++) hs[i] = hist[i] >> p->wasted;
    if (p->is_lpc) residuals_lpc(hs, xs, n, p->q, p->order, res);
    else           residuals_fixed(hs, xs, n, p->order, res);

    int plen = n >> p->pord;
    for (int i = 0; i < np; i++) {
        int start = i * plen;
        int len = (i == np - 1) ? (n - start) : plen;
        encode_res_part(ac, res + start, len, p->k[i], M);
    }
}

static void decode_plan(ac_dec_t *ac, int32_t *x, const int32_t *hist, int n,
                        rice_mdl_t *M) {
    int wasted = (int)ac_get_bits(ac, 4);
    int is_lpc = ac_dec_bit(ac, AC_BYPASS);
    int order = (int)ac_get_bits(ac, 4);
    int pord = (int)ac_get_bits(ac, 2);
    if (wasted > 15) wasted = 0;
    if (order > MAX_LPC) order = 0;
    if (pord > MAX_PORD) pord = 0;
    int16_t q[MAX_LPC];
    if (is_lpc)
        for (int i = 0; i < order; i++)
            q[i] = (int16_t)ac_get_bits(ac, 16);
    int np = 1 << pord;
    int kk[MAX_PART];
    memset(kk, 0, sizeof kk);
    for (int i = 0; i < np; i++)
        kk[i] = (int)ac_get_bits(ac, 4);

    int32_t res[ABLK];
    int plen = n >> pord;
    for (int i = 0; i < np; i++) {
        int start = i * plen;
        int len = (i == np - 1) ? (n - start) : plen;
        decode_res_part(ac, res + start, len, kk[i], M);
    }

    int32_t xs[ABLK], hs[MAX_LPC];
    for (int i = 0; i < MAX_LPC; i++) hs[i] = hist[i] >> wasted;
    for (int i = 0; i < n; i++) {
        int64_t p = 0;
        if (is_lpc && order > 0) {
            p = LPC_ROUND;
            for (int k = 1; k <= order; k++)
                p += (int64_t)q[k - 1] * samp_at(hs, xs, i - k);
            p >>= LPC_Q;
        } else {
            int32_t a = samp_at(hs, xs, i - 1);
            int32_t b = samp_at(hs, xs, i - 2);
            int32_t c = samp_at(hs, xs, i - 3);
            int32_t d = samp_at(hs, xs, i - 4);
            switch (order) {
                case 1: p = a; break;
                case 2: p = 2LL * a - b; break;
                case 3: p = 3LL * a - 3LL * b + c; break;
                case 4: p = 4LL * a - 6LL * b + 4LL * c - d; break;
                default: p = 0; break;
            }
        }
        xs[i] = (int32_t)(p + res[i]);
        x[i] = xs[i] << wasted;
    }
}

static void stereo_xform(int mode, const int32_t *L, const int32_t *R,
                         const int32_t *hL, const int32_t *hR, int n,
                         int32_t *C0, int32_t *C1, int32_t *h0, int32_t *h1) {
    if (mode == 1) { /* mid-side */
        for (int i = 0; i < n; i++) {
            C0[i] = (L[i] + R[i]) >> 1;
            C1[i] = L[i] - R[i];
        }
        for (int i = 0; i < MAX_LPC; i++) {
            h0[i] = (hL[i] + hR[i]) >> 1;
            h1[i] = hL[i] - hR[i];
        }
    } else if (mode == 2) { /* left + side */
        for (int i = 0; i < n; i++) {
            C0[i] = L[i];
            C1[i] = R[i] - L[i];
        }
        for (int i = 0; i < MAX_LPC; i++) {
            h0[i] = hL[i];
            h1[i] = hR[i] - hL[i];
        }
    } else { /* independent */
        memcpy(C0, L, (size_t)n * sizeof(int32_t));
        memcpy(C1, R, (size_t)n * sizeof(int32_t));
        memcpy(h0, hL, MAX_LPC * sizeof(int32_t));
        memcpy(h1, hR, MAX_LPC * sizeof(int32_t));
    }
}

static void stereo_ixform(int mode, const int32_t *C0, const int32_t *C1,
                          int n, int32_t *L, int32_t *R) {
    if (mode == 1) {
        for (int i = 0; i < n; i++) {
            int32_t m = C0[i], s = C1[i];
            L[i] = m + ((s + 1) >> 1);
            R[i] = L[i] - s;
        }
    } else if (mode == 2) {
        for (int i = 0; i < n; i++) {
            L[i] = C0[i];
            R[i] = C1[i] + L[i];
        }
    } else {
        memcpy(L, C0, (size_t)n * sizeof(int32_t));
        memcpy(R, C1, (size_t)n * sizeof(int32_t));
    }
}

static void encode_block_mono(ac_enc_t *ac, const int32_t *x, int n,
                              int32_t *hist, rice_mdl_t *M) {
    chplan_t p;
    plan_channel(x, hist, n, &p);
    emit_plan(ac, &p, x, hist, n, M);
    hist_update(hist, x, n);
}

static void encode_block_stereo(ac_enc_t *ac, const int32_t *L, const int32_t *R,
                                int n, int32_t *hL, int32_t *hR, rice_mdl_t *M) {
    int32_t C0[ABLK], C1[ABLK], h0[MAX_LPC], h1[MAX_LPC];
    chplan_t p0, p1;
    int best_mode = 0;
    int64_t best_bits = (int64_t)1 << 60;
    chplan_t bp0, bp1;
    memset(&bp0, 0, sizeof bp0);
    memset(&bp1, 0, sizeof bp1);

    for (int mode = 0; mode <= 2; mode++) {
        stereo_xform(mode, L, R, hL, hR, n, C0, C1, h0, h1);
        plan_channel(C0, h0, n, &p0);
        plan_channel(C1, h1, n, &p1);
        int64_t bits = p0.bits + p1.bits + 2;
        if (bits < best_bits) {
            best_bits = bits;
            best_mode = mode;
            bp0 = p0; bp1 = p1;
        }
    }

    ac_put_bits(ac, (uint32_t)best_mode, 2);
    stereo_xform(best_mode, L, R, hL, hR, n, C0, C1, h0, h1);
    emit_plan(ac, &bp0, C0, h0, n, M);
    emit_plan(ac, &bp1, C1, h1, n, M);
    hist_update(hL, L, n);
    hist_update(hR, R, n);
}

static void decode_block_mono(ac_dec_t *ac, int32_t *x, int n,
                              int32_t *hist, rice_mdl_t *M) {
    decode_plan(ac, x, hist, n, M);
    hist_update(hist, x, n);
}

static void stereo_hist(int mode, const int32_t *hL, const int32_t *hR,
                        int32_t *h0, int32_t *h1) {
    if (mode == 1) {
        for (int i = 0; i < MAX_LPC; i++) {
            h0[i] = (hL[i] + hR[i]) >> 1;
            h1[i] = hL[i] - hR[i];
        }
    } else if (mode == 2) {
        for (int i = 0; i < MAX_LPC; i++) {
            h0[i] = hL[i];
            h1[i] = hR[i] - hL[i];
        }
    } else {
        memcpy(h0, hL, MAX_LPC * sizeof(int32_t));
        memcpy(h1, hR, MAX_LPC * sizeof(int32_t));
    }
}

static void decode_block_stereo(ac_dec_t *ac, int32_t *L, int32_t *R, int n,
                                int32_t *hL, int32_t *hR, rice_mdl_t *M) {
    int mode = (int)ac_get_bits(ac, 2);
    if (mode > 2) mode = 0;
    int32_t C0[ABLK], C1[ABLK], h0[MAX_LPC], h1[MAX_LPC];
    stereo_hist(mode, hL, hR, h0, h1);
    decode_plan(ac, C0, h0, n, M);
    decode_plan(ac, C1, h1, n, M);
    stereo_ixform(mode, C0, C1, n, L, R);
    hist_update(hL, L, n);
    hist_update(hR, R, n);
}

int64_t zv_audio_encode(const int16_t *in_samples, int n_frames, int n_channels,
                         uint8_t *out_buf, int64_t out_cap)
{
    if (n_channels < 1 || n_channels > 2) return -1;
    if (n_frames < 0) return -1;
    int hdr = 3;
    if (out_cap < hdr + 16) return -1;

    int32_t *L = (int32_t*)malloc((size_t)n_frames * sizeof(int32_t));
    int32_t *R = NULL;
    if (n_channels == 2) R = (int32_t*)malloc((size_t)n_frames * sizeof(int32_t));
    if (!L || (n_channels == 2 && !R)) { free(L); free(R); return -1; }

    if (n_channels == 1) {
        for (int i = 0; i < n_frames; i++) L[i] = (int32_t)in_samples[i];
    } else {
        for (int i = 0; i < n_frames; i++) {
            L[i] = (int32_t)in_samples[i * 2];
            R[i] = (int32_t)in_samples[i * 2 + 1];
        }
    }

    out_buf[0] = 'Z'; out_buf[1] = 'A'; out_buf[2] = ZA_VER;

    bw_t bw; bw_init(&bw, out_buf + hdr, (size_t)out_cap - (size_t)hdr);
    ac_enc_t ac; ac_init_enc(&ac, &bw);
    rice_mdl_t M; rice_mdl_init(&M);
    int32_t histL[MAX_LPC], histR[MAX_LPC];
    memset(histL, 0, sizeof histL);
    memset(histR, 0, sizeof histR);

    for (int off = 0; off < n_frames; ) {
        int n = n_frames - off;
        if (n > ABLK) n = ABLK;
        if (n_channels == 1)
            encode_block_mono(&ac, L + off, n, histL, &M);
        else
            encode_block_stereo(&ac, L + off, R + off, n, histL, histR, &M);
        off += n;
    }
    ac_finish_enc(&ac);
    size_t written = bw_finish(&bw);

    free(L); if (R) free(R);
    return (int64_t)written + hdr;
}

int64_t zv_audio_decode(int16_t *out_samples, int n_frames, int n_channels,
                         const uint8_t *in_buf, int64_t in_size)
{
    if (n_channels < 1 || n_channels > 2) return -1;
    if (n_frames < 0) return -1;
    if (in_size < 0) return -1;

    int32_t *L = (int32_t*)malloc((size_t)n_frames * sizeof(int32_t));
    int32_t *R = NULL;
    if (n_channels == 2) R = (int32_t*)malloc((size_t)n_frames * sizeof(int32_t));
    if (!L || (n_channels == 2 && !R)) { free(L); free(R); return -1; }

    int ver = 0;
    if (in_size >= 3 && in_buf[0] == 'Z' && in_buf[1] == 'A')
        ver = in_buf[2];

    if (ver == ZA_VER) {
        int hdr = 3;
        br_t br; br_init(&br, in_buf + hdr, (size_t)in_size - (size_t)hdr);
        ac_dec_t ac; ac_init_dec(&ac, &br);
        rice_mdl_t M; rice_mdl_init(&M);
        int32_t histL[MAX_LPC], histR[MAX_LPC];
        memset(histL, 0, sizeof histL);
        memset(histR, 0, sizeof histR);
        for (int off = 0; off < n_frames; ) {
            int n = n_frames - off;
            if (n > ABLK) n = ABLK;
            if (n_channels == 1)
                decode_block_mono(&ac, L + off, n, histL, &M);
            else
                decode_block_stereo(&ac, L + off, R + off, n, histL, histR, &M);
            off += n;
        }
        if (n_channels == 1) {
            for (int i = 0; i < n_frames; i++) out_samples[i] = (int16_t)L[i];
        } else {
            for (int i = 0; i < n_frames; i++) {
                out_samples[i * 2]     = (int16_t)L[i];
                out_samples[i * 2 + 1] = (int16_t)R[i];
            }
        }
        free(L); if (R) free(R);
        return (int64_t)br.pos + hdr;
    }

    int v1 = (ver == ZA_VER1 && in_size >= 3 + n_channels);
    int hdr = v1 ? (3 + n_channels) : 0;
    int wL = v1 ? in_buf[3] : 0;
    int wR = (v1 && n_channels == 2) ? in_buf[4] : 0;
    if (wL > 15) wL = 0;
    if (wR > 15) wR = 0;

    br_t br; br_init(&br, in_buf + hdr, (size_t)in_size - (size_t)hdr);
    ac_dec_t ac; ac_init_dec(&ac, &br);

    if (v1) {
        decode_channel_v1(&ac, L, n_frames);
        if (n_channels == 2) decode_channel_v1(&ac, R, n_frames);
        if (wL) for (int i = 0; i < n_frames; i++) L[i] <<= wL;
        if (wR) for (int i = 0; i < n_frames; i++) R[i] <<= wR;
    } else {
        decode_channel(&ac, L, n_frames);
        if (n_channels == 2) decode_channel(&ac, R, n_frames);
    }

    if (n_channels == 1) {
        for (int i = 0; i < n_frames; i++) out_samples[i] = (int16_t)L[i];
    } else {
        for (int i = 0; i < n_frames; i++) {
            int l = L[i];
            int r = R[i] + l;
            out_samples[i * 2]     = (int16_t)l;
            out_samples[i * 2 + 1] = (int16_t)r;
        }
    }

    free(L); if (R) free(R);
    return (int64_t)br.pos + hdr;
}
