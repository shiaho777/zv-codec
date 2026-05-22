
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
        int Y, Co, Cg;
        rgb_to_ycocgr(rgb[i*3], rgb[i*3+1], rgb[i*3+2], &Y, &Co, &Cg);
        ycocg[i*3]   = (int16_t)((Y  + 1) >> 1);
        ycocg[i*3+1] = (int16_t)((Co + (Co >= 0 ? 2 : -2)) >> 2);
        ycocg[i*3+2] = (int16_t)((Cg + (Cg >= 0 ? 2 : -2)) >> 2);
    }
}

void ycocgr_to_rgb_frame(const int16_t *ycocg, uint8_t *rgb, int w, int h) {
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
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

static void deinterleave_planes(const int16_t *in, int16_t *Y, int16_t *Co, int16_t *Cg,
                                int w, int h) {
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
        Y [i] = in[i*3 + 0];
        Co[i] = in[i*3 + 1];
        Cg[i] = in[i*3 + 2];
    }
}

static void interleave_planes(const int16_t *Y, const int16_t *Co, const int16_t *Cg,
                              int16_t *out, int w, int h) {
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
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

#define NSPAT 6

static inline void spat_preds(int a, int b, int c, int d, int preds[NSPAT]) {
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
    for (int p = 0; p < NSPAT; p++) s->As[ctx][p] += abs_s[p];
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
            int b = top ? top[x] : a;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d = (top && x < w - 1) ? top[x + 1] : b;

            int q1 = q_grad(d - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);

            int spreds[NSPAT];
            spat_preds(a, b, c, d, spreds);
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
            int b = top ? top[x] : a;
            int c = (top && x > 0) ? top[x - 1] : a;
            int d = (top && x < w - 1) ? top[x + 1] : b;

            int q1 = q_grad(d - b);
            int q2 = q_grad(b - c);
            int q3 = q_grad(c - a);
            int flip;
            int ctx = ctx_index(q1, q2, q3, &flip);

            int spreds[NSPAT];
            spat_preds(a, b, c, d, spreds);

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
