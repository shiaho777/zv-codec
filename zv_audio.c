
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

int64_t zv_audio_encode(const int16_t *in_samples, int n_frames, int n_channels,
                         uint8_t *out_buf, int64_t out_cap)
{
    if (n_channels < 1 || n_channels > 2) return -1;

    int32_t *L = (int32_t*)malloc((size_t)n_frames * sizeof(int32_t));
    int32_t *R = NULL;
    if (n_channels == 2) R = (int32_t*)malloc((size_t)n_frames * sizeof(int32_t));

    if (n_channels == 1) {
        for (int i = 0; i < n_frames; i++) L[i] = (int32_t)in_samples[i];
    } else {
        for (int i = 0; i < n_frames; i++) {
            int l = in_samples[i*2];
            int r = in_samples[i*2 + 1];
            L[i] = l;
            R[i] = r - l;
        }
    }

    bw_t bw; bw_init(&bw, out_buf, (size_t)out_cap);
    ac_enc_t ac; ac_init_enc(&ac, &bw);

    encode_channel(&ac, L, n_frames);
    if (n_channels == 2) encode_channel(&ac, R, n_frames);

    ac_finish_enc(&ac);
    size_t written = bw_finish(&bw);

    free(L); if (R) free(R);
    return (int64_t)written;
}

int64_t zv_audio_decode(int16_t *out_samples, int n_frames, int n_channels,
                         const uint8_t *in_buf, int64_t in_size)
{
    if (n_channels < 1 || n_channels > 2) return -1;

    int32_t *L = (int32_t*)malloc((size_t)n_frames * sizeof(int32_t));
    int32_t *R = NULL;
    if (n_channels == 2) R = (int32_t*)malloc((size_t)n_frames * sizeof(int32_t));

    br_t br; br_init(&br, in_buf, (size_t)in_size);
    ac_dec_t ac; ac_init_dec(&ac, &br);

    decode_channel(&ac, L, n_frames);
    if (n_channels == 2) decode_channel(&ac, R, n_frames);

    if (n_channels == 1) {
        for (int i = 0; i < n_frames; i++) out_samples[i] = (int16_t)L[i];
    } else {
        for (int i = 0; i < n_frames; i++) {
            int l = L[i];
            int r = R[i] + l;
            out_samples[i*2]     = (int16_t)l;
            out_samples[i*2 + 1] = (int16_t)r;
        }
    }

    free(L); if (R) free(R);
    return (int64_t)br.pos;
}
