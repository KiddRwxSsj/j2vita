#include <kni.h>
#include <sni.h>
#include <commonKNIMacros.h>
#include <gxapi_graphics.h>
#include <imgapi_image.h>
#include <gxj_putpixel.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zlib.h>

/*
 * MascotCapsule Micro3D v3 software renderer. Figures (MBAC), action tables
 * (MTRA) and textures (8 bit BMP) are parsed into int[] blobs owned by Java,
 * triangles are rasterized straight into the bound Graphics buffer.
 */

/* ---------- byte / bit reader ---------- */

typedef struct {
    const uint8_t *d;
    int len, pos, err;
    uint32_t cache;
    int cached;
} Rd;

static int rd_u8(Rd *r) {
    if (r->pos >= r->len) { r->err = 1; return 0; }
    return r->d[r->pos++];
}

static int rd_u16(Rd *r) {
    int a = rd_u8(r);
    return a | rd_u8(r) << 8;
}

static int rd_s16(Rd *r) {
    return (int16_t)rd_u16(r);
}

static int rd_s32(Rd *r) {
    int a = rd_u16(r);
    return a | rd_u16(r) << 16;
}

static int rd_ubits(Rd *r, int n) {
    int v;
    if (n <= 0) return 0;
    if (n > 24) { r->err = 1; return 0; }
    while (r->cached < n) {
        r->cache |= (uint32_t)rd_u8(r) << r->cached;
        r->cached += 8;
    }
    v = (int)(r->cache & ((1u << n) - 1));
    r->cache >>= n;
    r->cached -= n;
    return v;
}

static int rd_sbits(Rd *r, int n) {
    int v = rd_ubits(r, n);
    return (v << (32 - n)) >> (32 - n);
}

static void rd_align(Rd *r) {
    r->cache = 0;
    r->cached = 0;
}

/* ---------- figure blob ---------- */

enum {
    F_MAGIC, F_PATTERN, F_NUM_PATTERNS, F_NV, F_NP, F_NB, F_HASN,
    F_OFF_V, F_OFF_N, F_OFF_P, F_OFF_B, F_OFF_XV, F_OFF_XN, F_HDR = 16
};
#define FIG_MAGIC 0x4d334446

enum { PF_TRANSP = 1, PF_DOUBLE = 2, PF_LIGHT = 4, PF_COLOR = 16 };

typedef struct {
    uint16_t v[4];
    uint8_t uv[8];
    uint8_t quad, flags, face, blend;
    uint32_t pattern;
    uint32_t color;
} Poly;

typedef struct {
    int len, parent;
    float m[12];
} Bone;

typedef char poly_size_check[sizeof(Poly) == 28 ? 1 : -1];

typedef struct {
    int version, vf, nf, pf;
    int nv, t3, t4, nb, c3, c4, ntex, npat, ncol;
} MbacHead;

static int mbac_head(Rd *r, MbacHead *h) {
    if (rd_u8(r) != 'M' || rd_u8(r) != 'B') return 0;
    h->version = rd_u8(r);
    if (rd_u8(r) != 0 || h->version < 2 || h->version > 5) return 0;
    if (h->version > 3) {
        h->vf = rd_u8(r);
        h->nf = rd_u8(r);
        h->pf = rd_u8(r);
        if (rd_u8(r) != 1) return 0;
    } else {
        h->vf = 1;
        h->nf = 0;
        h->pf = 1;
    }
    h->nv = rd_u16(r);
    h->t3 = rd_u16(r);
    h->t4 = rd_u16(r);
    h->nb = rd_u16(r);
    if (h->pf < 3) {
        h->c3 = h->c4 = h->ncol = 0;
        h->ntex = h->npat = 1;
    } else {
        h->c3 = rd_u16(r);
        h->c4 = rd_u16(r);
        h->ntex = rd_u16(r);
        h->npat = rd_u16(r);
        h->ncol = rd_u16(r);
    }
    if (r->err || h->nv > 21845 || h->ntex > 16 || h->npat > 33 || h->npat < 1 || h->ncol > 256)
        return 0;
    return 1;
}

static int fig_words(const MbacHead *h) {
    int np = h->t3 + h->t4 + h->c3 + h->c4;
    int n = F_HDR + h->nv * 3 * 2 + np * 7 + h->nb * 14;
    if (h->nf) n += h->nv * 3 * 2;
    return n;
}

static void set_material(Poly *p, int m) {
    p->flags = 0;
    if (m & 1) p->flags |= PF_TRANSP;
    if (m & 16) p->flags |= PF_DOUBLE;
    if (m & 32) p->flags |= PF_LIGHT;
    p->blend = (m >> 1) & 3;
}

static void fig_transform(int32_t *fig, const float *act, int actBones);

static int mbac_load(Rd *r, int32_t *fig) {
    static const int sizes[4] = {8, 10, 13, 16};
    static const int pool[8] = {0, 0, 4096, 0, 0, -4096, 0, 0};
    MbacHead h;
    int pat[33][17][2];
    int i, j, k, np, npt;
    float *v, *nrm = NULL;
    Poly *polys;
    Bone *bones;
    int sum = 0;

    if (!mbac_head(r, &h)) return 0;
    np = h.t3 + h.t4 + h.c3 + h.c4;
    npt = h.t3 + h.t4;

    fig[F_MAGIC] = FIG_MAGIC;
    fig[F_PATTERN] = 0;
    fig[F_NUM_PATTERNS] = h.npat;
    fig[F_NV] = h.nv;
    fig[F_NP] = np;
    fig[F_NB] = h.nb;
    fig[F_HASN] = h.nf != 0;
    k = F_HDR;
    fig[F_OFF_V] = k; k += h.nv * 3;
    fig[F_OFF_XV] = k; k += h.nv * 3;
    if (h.nf) {
        fig[F_OFF_N] = k; k += h.nv * 3;
        fig[F_OFF_XN] = k; k += h.nv * 3;
    } else {
        fig[F_OFF_N] = fig[F_OFF_XN] = 0;
    }
    fig[F_OFF_P] = k; k += np * 7;
    fig[F_OFF_B] = k;

    v = (float *)(fig + fig[F_OFF_V]);
    if (h.nf) nrm = (float *)(fig + fig[F_OFF_N]);
    polys = (Poly *)(fig + fig[F_OFF_P]);
    bones = (Bone *)(fig + fig[F_OFF_B]);
    memset(polys, 0, np * sizeof(Poly));

    memset(pat, 0, sizeof(pat));
    if (h.version == 5) {
        for (i = 0; i < h.npat; i++)
            for (j = 0; j <= h.ntex; j++) {
                pat[i][j][0] = rd_u16(r);
                pat[i][j][1] = rd_u16(r);
            }
    } else {
        pat[0][0][0] = h.c3;
        pat[0][0][1] = h.c4;
        pat[0][1][0] = h.t3;
        pat[0][1][1] = h.t4;
    }

    if (h.vf == 1) {
        for (i = 0; i < h.nv * 3; i++)
            v[i] = (float)rd_s16(r);
    } else if (h.vf == 2) {
        i = 0;
        while (i < h.nv && !r->err) {
            int chunk = rd_ubits(r, 8);
            int size = sizes[chunk >> 6];
            int cnt = (chunk & 63) + 1;
            if (i + cnt > h.nv) return 0;
            for (j = 0; j < cnt; j++, i++) {
                v[i * 3] = (float)rd_sbits(r, size);
                v[i * 3 + 1] = (float)rd_sbits(r, size);
                v[i * 3 + 2] = (float)rd_sbits(r, size);
            }
        }
    } else {
        return 0;
    }
    rd_align(r);

    if (h.nf == 1) {
        for (i = 0; i < h.nv * 3; i++)
            nrm[i] = rd_s16(r) / 4096.0f;
    } else if (h.nf == 2) {
        for (i = 0; i < h.nv; i++) {
            int x = rd_ubits(r, 7), y, z;
            if (x == 64) {
                int t = rd_ubits(r, 3);
                if (t > 5) return 0;
                z = pool[t];
                y = pool[t + 1];
                x = pool[t + 2];
            } else {
                int dq;
                x = (x << 25) >> 19;
                y = (rd_ubits(r, 7) << 25) >> 19;
                dq = 4096 * 4096 - x * x - y * y;
                z = dq > 0 ? (int)(sqrtf((float)dq) + 0.5f) : 0;
                if (rd_ubits(r, 1)) z = -z;
            }
            nrm[i * 3] = x / 4096.0f;
            nrm[i * 3 + 1] = y / 4096.0f;
            nrm[i * 3 + 2] = z / 4096.0f;
        }
    } else if (h.nf != 0) {
        return 0;
    }
    rd_align(r);

    if (h.c3 + h.c4) {
        int mb = rd_u8(r), vb = rd_u8(r), cb = rd_u8(r), cib = rd_u8(r);
        uint32_t colors[256];
        rd_u8(r);
        for (i = 0; i < h.ncol; i++) {
            int cr = rd_ubits(r, cb), cg = rd_ubits(r, cb), cbl = rd_ubits(r, cb);
            colors[i] = (uint32_t)(cr << 16 | cg << 8 | cbl);
        }
        for (i = 0; i < h.c3 + h.c4; i++) {
            Poly *p = &polys[npt + i];
            int m = rd_ubits(r, mb) << 1, c;
            p->quad = i >= h.c3;
            for (j = 0; j < (p->quad ? 4 : 3); j++) {
                p->v[j] = (uint16_t)rd_ubits(r, vb);
                if (p->v[j] >= h.nv) return 0;
            }
            c = rd_ubits(r, cib);
            set_material(p, m);
            p->flags |= PF_COLOR;
            p->color = c < h.ncol ? colors[c] : 0xffffff;
        }
    }

    if (npt) {
        int mb = 0, vb = 0, ub = 7;
        if (h.pf == 2) {
            mb = rd_u8(r);
            vb = rd_u8(r);
        } else if (h.pf == 3) {
            mb = rd_ubits(r, 8);
            vb = rd_ubits(r, 8);
            ub = rd_ubits(r, 8);
            rd_ubits(r, 8);
        }
        for (i = 0; i < npt; i++) {
            Poly *p = &polys[i];
            int nvx;
            p->quad = i >= h.t3;
            nvx = p->quad ? 4 : 3;
            if (h.pf == 1) {
                int m = rd_u16(r);
                for (j = 0; j < nvx; j++) {
                    p->v[j] = (uint16_t)rd_u16(r);
                    if (p->v[j] >= h.nv) return 0;
                }
                for (j = 0; j < nvx * 2; j++)
                    p->uv[j] = (uint8_t)rd_u8(r);
                set_material(p, (m & 4) << 2 | (m & 2) >> 1);
            } else {
                int m = rd_ubits(r, mb);
                for (j = 0; j < nvx; j++) {
                    p->v[j] = (uint16_t)rd_ubits(r, vb);
                    if (p->v[j] >= h.nv) return 0;
                }
                for (j = 0; j < nvx * 2; j++)
                    p->uv[j] = (uint8_t)rd_ubits(r, ub);
                set_material(p, m);
            }
        }
    }
    rd_align(r);
    if (r->err) return 0;

    {
        int c3 = 0, c4 = h.c3, t3 = 0, t4 = h.t3;
        for (i = 0; i < h.npat; i++) {
            uint32_t pb = i == 0 ? 0 : 1u << i;
            for (k = 0; k < pat[i][0][0] && c3 < h.c3; k++)
                polys[npt + c3++].pattern = pb;
            for (k = 0; k < pat[i][0][1] && c4 < h.c3 + h.c4; k++)
                polys[npt + c4++].pattern = pb;
            for (j = 0; j < h.ntex; j++) {
                for (k = 0; k < pat[i][j + 1][0] && t3 < h.t3; k++) {
                    polys[t3].pattern = pb;
                    polys[t3++].face = (uint8_t)j;
                }
                for (k = 0; k < pat[i][j + 1][1] && t4 < npt; k++) {
                    polys[t4].pattern = pb;
                    polys[t4++].face = (uint8_t)j;
                }
            }
        }
    }

    for (i = 0; i < h.nb; i++) {
        Bone *b = &bones[i];
        b->len = rd_u16(r);
        b->parent = rd_s16(r);
        if (b->parent < -1 || b->parent >= i) b->parent = -1;
        for (j = 0; j < 12; j++)
            b->m[j] = (j & 3) == 3 ? (float)rd_s16(r) : rd_s16(r) / 4096.0f;
        sum += b->len;
    }
    if (r->err || sum != h.nv) return 0;

    fig_transform(fig, NULL, 0);
    return 1;
}

/* out = l * r for 3x4 matrices */
static void mat_mul(float *out, const float *l, const float *r) {
    float t[12];
    int i;
    for (i = 0; i < 3; i++) {
        const float *a = l + i * 4;
        t[i * 4] = a[0] * r[0] + a[1] * r[4] + a[2] * r[8];
        t[i * 4 + 1] = a[0] * r[1] + a[1] * r[5] + a[2] * r[9];
        t[i * 4 + 2] = a[0] * r[2] + a[1] * r[6] + a[2] * r[10];
        t[i * 4 + 3] = a[0] * r[3] + a[1] * r[7] + a[2] * r[11] + a[3];
    }
    memcpy(out, t, sizeof(t));
}

static void fig_transform(int32_t *fig, const float *act, int actBones) {
    int nb = fig[F_NB], i, j;
    const float *src = (const float *)(fig + fig[F_OFF_V]);
    float *dst = (float *)(fig + fig[F_OFF_XV]);
    const float *ns = fig[F_HASN] ? (const float *)(fig + fig[F_OFF_N]) : NULL;
    float *nd = fig[F_HASN] ? (float *)(fig + fig[F_OFF_XN]) : NULL;
    Bone *bones = (Bone *)(fig + fig[F_OFF_B]);
    float *mats = (float *)malloc((nb ? nb : 1) * 12 * sizeof(float));
    if (!mats) return;
    for (i = 0; i < nb; i++) {
        float *m = mats + i * 12;
        if (bones[i].parent < 0)
            memcpy(m, bones[i].m, sizeof(bones[i].m));
        else
            mat_mul(m, mats + bones[i].parent * 12, bones[i].m);
        if (act && i < actBones)
            mat_mul(m, m, act + i * 12);
        for (j = 0; j < bones[i].len; j++, src += 3, dst += 3) {
            float x = src[0], y = src[1], z = src[2];
            dst[0] = m[0] * x + m[1] * y + m[2] * z + m[3];
            dst[1] = m[4] * x + m[5] * y + m[6] * z + m[7];
            dst[2] = m[8] * x + m[9] * y + m[10] * z + m[11];
            if (ns) {
                x = ns[0]; y = ns[1]; z = ns[2];
                nd[0] = m[0] * x + m[1] * y + m[2] * z;
                nd[1] = m[4] * x + m[5] * y + m[6] * z;
                nd[2] = m[8] * x + m[9] * y + m[10] * z;
                ns += 3;
                nd += 3;
            }
        }
    }
    free(mats);
}

/* ---------- action table blob ---------- */

enum { A_MAGIC, A_NUM_ACTIONS, A_NUM_BONES, A_VERSION, A_LEN, A_HDR = 8 };
#define ACT_MAGIC 0x4d334441

/* key/value track: count, then count * (u16 key, n * s16), linear between keys */
static void track_eval(Rd *r, int n, float kf, float *out) {
    int cnt = rd_u16(r), i, j, done = 0, have = 0, pk = 0;
    int pv[3] = {0, 0, 0};
    for (i = 0; i < cnt; i++) {
        int key = rd_u16(r), val[3];
        for (j = 0; j < n; j++) val[j] = rd_s16(r);
        if (done) continue;
        if (key <= kf) {
            pk = key;
            for (j = 0; j < n; j++) pv[j] = val[j];
            have = 1;
        } else {
            float t = have ? (kf - pk) / (float)(key - pk) : 1.0f;
            for (j = 0; j < n; j++) out[j] = pv[j] + (val[j] - pv[j]) * t;
            done = 1;
        }
    }
    if (!done)
        for (j = 0; j < n; j++) out[j] = (float)pv[j];
}

static void track_skip(Rd *r, int n) {
    int cnt = rd_u16(r);
    r->pos += cnt * (2 + n * 2);
    if (r->pos > r->len) r->err = 1;
}

/* turns the matrix so its z axis points along (x,y,z) */
static void bone_rotate(float *m, float x, float y, float z) {
    float l = sqrtf(x * x + y * y + z * z), xx, yy;
    if (l <= 0) { x = 0; y = 0; z = 1; } else { x /= l; y /= l; z /= l; }
    xx = x * x;
    yy = y * y;
    if (xx > 0 || yy > 0) {
        float a = (1 - z) / (xx + yy), b = -a * x * y;
        m[0] = z + yy * a; m[1] = b; m[2] = x;
        m[4] = b; m[5] = z + xx * a; m[6] = y;
        m[8] = -x; m[9] = -y;
    } else {
        m[0] = 1; m[1] = 0; m[2] = 0;
        m[4] = 0; m[5] = z; m[6] = 0;
        m[8] = 0; m[9] = 0;
    }
    m[10] = z;
}

static void bone_roll(float *m, float a) {
    float s = sinf(a), c = cosf(a);
    int i;
    for (i = 0; i < 3; i++) {
        float m0 = m[i * 4], m1 = m[i * 4 + 1];
        m[i * 4] = m0 * c + m1 * s;
        m[i * 4 + 1] = m1 * c - m0 * s;
    }
}

#define ROLL_RAD (3.14159265f / 2048.0f)

/* reads one bone action; with m != NULL also evaluates it at frame kf */
static int bone_action(Rd *r, float *m, float kf) {
    float t[3];
    int type = rd_u8(r), i;
    switch (type) {
    case 0:
        for (i = 0; i < 12; i++) {
            int v = rd_s16(r);
            if (m) m[i] = (i & 3) == 3 ? (float)v : v / 4096.0f;
        }
        break;
    case 1:
        if (m) {
            memset(m, 0, 12 * sizeof(float));
            m[0] = m[5] = m[10] = 1;
        }
        break;
    case 2:
    case 6:
        if (!m) {
            track_skip(r, 3);
            if (type == 2) track_skip(r, 3);
            track_skip(r, 3);
            track_skip(r, 1);
            break;
        }
        {
            float tr[3], sc[3] = {4096, 4096, 4096}, ro;
            track_eval(r, 3, kf, tr);
            if (type == 2) track_eval(r, 3, kf, sc);
            track_eval(r, 3, kf, t);
            bone_rotate(m, t[0], t[1], t[2]);
            track_eval(r, 1, kf, &ro);
            bone_roll(m, ro * ROLL_RAD);
            m[3] = tr[0]; m[7] = tr[1]; m[11] = tr[2];
            if (type == 2)
                for (i = 0; i < 3; i++) {
                    m[i * 4] *= sc[0] / 4096.0f;
                    m[i * 4 + 1] *= sc[1] / 4096.0f;
                    m[i * 4 + 2] *= sc[2] / 4096.0f;
                }
        }
        break;
    case 3: {
        float tr[3];
        int ro;
        tr[0] = (float)rd_s16(r);
        tr[1] = (float)rd_s16(r);
        tr[2] = (float)rd_s16(r);
        if (!m) {
            track_skip(r, 3);
            rd_s16(r);
            break;
        }
        track_eval(r, 3, kf, t);
        ro = rd_s16(r);
        bone_rotate(m, t[0], t[1], t[2]);
        bone_roll(m, ro * ROLL_RAD);
        m[3] = tr[0]; m[7] = tr[1]; m[11] = tr[2];
        break;
    }
    case 4:
    case 5:
        if (!m) {
            track_skip(r, 3);
            if (type == 4) track_skip(r, 1);
            break;
        }
        track_eval(r, 3, kf, t);
        bone_rotate(m, t[0], t[1], t[2]);
        if (type == 4) {
            float ro;
            track_eval(r, 1, kf, &ro);
            bone_roll(m, ro * ROLL_RAD);
        }
        m[3] = m[7] = m[11] = 0;
        break;
    default:
        r->err = 1;
        return 0;
    }
    return !r->err;
}

/* walks the table; offs gets the byte offset of every action when not NULL */
static int mtra_scan(Rd *r, int *version, int *nact, int *nbones, int32_t *offs) {
    int i, b;
    if (rd_u8(r) != 'M' || rd_u8(r) != 'T') return 0;
    *version = rd_u8(r);
    if (rd_u8(r) != 0 || *version < 2 || *version > 5) return 0;
    *nact = rd_u16(r);
    *nbones = rd_u16(r);
    for (i = 0; i < 8; i++) rd_u16(r);
    rd_s32(r);
    for (i = 0; i < *nact; i++) {
        if (offs) offs[i] = r->pos;
        rd_u16(r);
        for (b = 0; b < *nbones; b++)
            if (!bone_action(r, NULL, 0)) return 0;
        if (*version >= 5) {
            int cnt = rd_u16(r);
            r->pos += cnt * 6;
        }
        if (r->err || r->pos > r->len) return 0;
    }
    return 1;
}

/* ---------- texture blob ---------- */

enum { T_MAGIC, T_W, T_H, T_PAL, T_PIX = T_PAL + 256 };
#define TEX_MAGIC 0x4d334454

static int bmp_head(Rd *r, int *w, int *h, int *flip, int *raster, int *pal, int *palEntry) {
    int dib;
    if (rd_u8(r) != 'B' || rd_u8(r) != 'M') return 0;
    r->pos = 10;
    *raster = rd_s32(r);
    dib = rd_s32(r);
    if (dib == 12) {
        *w = rd_u16(r);
        *h = rd_u16(r);
        rd_u16(r);
        if (rd_u16(r) != 8) return 0;
        *flip = 1;
        *palEntry = *raster - (14 + dib) >= 1024 ? 4 : 3;
    } else if (dib >= 40) {
        *w = rd_s32(r);
        *h = rd_s32(r);
        *flip = *h > 0;
        if (*h < 0) *h = -*h;
        rd_u16(r);
        if (rd_u16(r) != 8) return 0;
        if (rd_s32(r) != 0) return 0;
        *palEntry = 4;
    } else {
        return 0;
    }
    *pal = 14 + dib;
    if (r->err || *w <= 0 || *h <= 0 || *w > 1024 || *h > 1024) return 0;
    if (*raster + ((*w + 3) & ~3) * *h > r->len) return 0;
    return 1;
}

/* either palettized (micro3d) or ARGB (m3g); rs/rt wrap sizes that are not powers of two */
typedef struct {
    int w, h, wmask, hmask, rs, rt;
    const uint16_t *pal;
    const uint8_t *pix;
    const uint32_t *argb;
} Tex;

static int tex_get(const int32_t *blob, Tex *t) {
    if (!blob || blob[T_MAGIC] != TEX_MAGIC) return 0;
    t->w = blob[T_W];
    t->h = blob[T_H];
    t->wmask = (t->w & (t->w - 1)) == 0 ? t->w - 1 : -1;
    t->hmask = (t->h & (t->h - 1)) == 0 ? t->h - 1 : -1;
    t->pal = (const uint16_t *)(blob + T_PAL);
    t->pix = (const uint8_t *)(blob + T_PIX);
    t->argb = NULL;
    t->rs = t->rt = 0;
    return 1;
}

/* ---------- rasterizer ---------- */

typedef struct {
    gxj_pixel_type *px;
    int stride;
    int cx1, cy1, cx2, cy2;
} Target;

static float *zbuf;
static int zcap, zstride, zh;

typedef struct {
    float x, y, z;
    float u, v, s;
} VV;

/* d is the depth key, bigger is nearer, same scale as GL window depth flipped */
typedef struct {
    float x, y, w, u, v, s, d;
} SV;

typedef struct {
    int persp;
    float cx, cy, kx, ky, near, far, zc0, zc1;
    float m[12];
    int attr;
    int lit;
    float amb, dir, lx, ly, lz;
    int toon, toonHi, toonLo;
} View;

enum { ATTR_LIGHT = 1, ATTR_TOON = 4, ATTR_SEMI = 8 };

static void project(const View *vw, const VV *a, SV *o) {
    if (vw->persp) {
        float iz = 1.0f / a->z;
        o->x = vw->cx + a->x * vw->kx * iz;
        o->y = vw->cy + a->y * vw->ky * iz;
        o->w = iz;
        o->d = vw->zc0 + vw->zc1 * iz;
        o->u = a->u * iz;
        o->v = a->v * iz;
    } else {
        o->x = vw->cx + a->x * vw->kx;
        o->y = vw->cy + a->y * vw->ky;
        o->w = 1;
        o->d = vw->zc0 + vw->zc1 * a->z;
        o->u = a->u;
        o->v = a->v;
    }
    o->s = a->s;
}

static inline gxj_pixel_type rgb565(uint32_t c) {
    return (gxj_pixel_type)GXJ_RGB24TORGB16(c);
}

static inline gxj_pixel_type shade565(gxj_pixel_type c, int s) {
    int a = ((c & 0x1f) * s) >> 8, b = (((c >> 5) & 0x3f) * s) >> 8, d = (((c >> 11) & 0x1f) * s) >> 8;
    return (gxj_pixel_type)(a | b << 5 | d << 11);
}

static inline gxj_pixel_type blend565(gxj_pixel_type d, gxj_pixel_type s, int mode, int a) {
    int r, g, b;
    switch (mode) {
    case 4:
        r = (d & 0x1f) + ((((s & 0x1f) - (d & 0x1f)) * a) >> 8);
        g = (d & 0x7e0) + ((((s & 0x7e0) - (d & 0x7e0)) * a) >> 8);
        b = (d & 0xf800) + ((((s & 0xf800) - (d & 0xf800)) * a) >> 8);
        return (gxj_pixel_type)((r & 0x1f) | (g & 0x7e0) | (b & 0xf800));
    case 5:
        r = (d & 0x1f) + (((s & 0x1f) * a) >> 8);
        g = (d & 0x7e0) + ((((s & 0x7e0) * a) >> 8) & 0x7e0);
        b = (d & 0xf800) + ((((s & 0xf800) * a) >> 8) & 0xf800);
        if (r > 0x1f) r = 0x1f;
        if (g > 0x7e0) g = 0x7e0;
        if (b > 0xf800) b = 0xf800;
        return (gxj_pixel_type)(r | g | b);
    case 6:
    case 7: {
        int sh = mode == 6 ? 0 : 1;
        r = (((d & 0x1f) * (s & 0x1f)) >> 5) << sh;
        g = ((((d >> 5) & 0x3f) * ((s >> 5) & 0x3f)) >> 6) << sh;
        b = ((((d >> 11) & 0x1f) * ((s >> 11) & 0x1f)) >> 5) << sh;
        if (r > 0x1f) r = 0x1f;
        if (g > 0x3f) g = 0x3f;
        if (b > 0x1f) b = 0x1f;
        return (gxj_pixel_type)(r | g << 5 | b << 11);
    }
    case 1:
        return (gxj_pixel_type)(((d & 0xf7de) >> 1) + ((s & 0xf7de) >> 1));
    case 2:
        r = (d & 0x1f) + (s & 0x1f);
        g = (d & 0x7e0) + (s & 0x7e0);
        b = (d & 0xf800) + (s & 0xf800);
        if (r > 0x1f) r = 0x1f;
        if (g > 0x7e0) g = 0x7e0;
        if (b > 0xf800) b = 0xf800;
        return (gxj_pixel_type)(r | g | b);
    case 3:
        r = (d & 0x1f) - (s & 0x1f);
        g = (d & 0x7e0) - (s & 0x7e0);
        b = (d & 0xf800) - (s & 0xf800);
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
        return (gxj_pixel_type)(r | g | b);
    }
    return s;
}

typedef struct {
    const Tex *tex;
    gxj_pixel_type color;
    int colorkey;
    int blend;
    int zwrite;
    int ztest;
    int aref;
    int shaded;
    int persp;
} Fill;

static inline int wrap(int v, int size, int mask, int rep) {
    if (mask >= 0) return v & mask;
    if (rep) { v %= size; return v < 0 ? v + size : v; }
    return v < 0 ? 0 : v >= size ? size - 1 : v;
}

#define SPAN 8

static void raster_tri(const Target *t, const SV *p0, const SV *p1, const SV *p2, const Fill *f) {
    const SV *a = p0, *b = p1, *c = p2, *tmp;
    float area, dwdx, dwdy, dudx, dudy, dvdx, dvdy, dsdx, dsdy, dddx, dddy;
    float e1x, e1y, e2x, e2y;
    int y, ys, ye;

    if (a->y > b->y) { tmp = a; a = b; b = tmp; }
    if (b->y > c->y) { tmp = b; b = c; c = tmp; }
    if (a->y > b->y) { tmp = a; a = b; b = tmp; }

    e1x = b->x - a->x; e1y = b->y - a->y;
    e2x = c->x - a->x; e2y = c->y - a->y;
    area = e1x * e2y - e2x * e1y;
    if (area > -1e-6f && area < 1e-6f) return;
    area = 1.0f / area;
#define GRAD(F, DX, DY) do { float d1 = b->F - a->F, d2 = c->F - a->F; \
        DX = (d1 * e2y - d2 * e1y) * area; DY = (d2 * e1x - d1 * e2x) * area; } while (0)
    GRAD(w, dwdx, dwdy);
    GRAD(u, dudx, dudy);
    GRAD(v, dvdx, dvdy);
    GRAD(s, dsdx, dsdy);
    GRAD(d, dddx, dddy);
#undef GRAD

    ys = (int)ceilf(a->y - 0.5f);
    ye = (int)ceilf(c->y - 0.5f);
    if (ys < t->cy1) ys = t->cy1;
    if (ye > t->cy2) ye = t->cy2;

    for (y = ys; y < ye; y++) {
        float fy = y + 0.5f, xl, xr, ox, oy, w, u, v, s, dz;
        int x, xs, xe;
        gxj_pixel_type *dst;
        float *zp;

        xl = a->x + (fy - a->y) * (e2x / e2y);
        if (fy < b->y) {
            if (b->y - a->y <= 0) continue;
            xr = a->x + (fy - a->y) * (e1x / e1y);
        } else {
            if (c->y - b->y <= 0) continue;
            xr = b->x + (fy - b->y) * ((c->x - b->x) / (c->y - b->y));
        }
        if (xl > xr) { float q = xl; xl = xr; xr = q; }
        xs = (int)ceilf(xl - 0.5f);
        xe = (int)ceilf(xr - 0.5f);
        if (xs < t->cx1) xs = t->cx1;
        if (xe > t->cx2) xe = t->cx2;
        if (xs >= xe) continue;

        ox = xs + 0.5f - a->x;
        oy = fy - a->y;
        w = a->w + ox * dwdx + oy * dwdy;
        u = a->u + ox * dudx + oy * dudy;
        v = a->v + ox * dvdx + oy * dvdy;
        s = a->s + ox * dsdx + oy * dsdy;
        dz = a->d + ox * dddx + oy * dddy;
        dst = t->px + y * t->stride;
        zp = zbuf + y * zstride;

        if (f->tex) {
            const Tex *tx = f->tex;
            x = xs;
            while (x < xe) {
                int n = xe - x, iu, iv, du, dv, k;
                float u0, v0, u1, v1;
                if (n > SPAN) n = SPAN;
                if (f->persp) {
                    float iw = 1.0f / w, w1 = w + dwdx * n, iw1 = 1.0f / w1;
                    u0 = u * iw; v0 = v * iw;
                    u1 = (u + dudx * n) * iw1;
                    v1 = (v + dvdx * n) * iw1;
                } else {
                    u0 = u; v0 = v;
                    u1 = u + dudx * n;
                    v1 = v + dvdx * n;
                }
                iu = (int)(u0 * 65536.0f);
                iv = (int)(v0 * 65536.0f);
                du = (int)((u1 - u0) * 65536.0f) / n;
                dv = (int)((v1 - v0) * 65536.0f) / n;
                for (k = 0; k < n; k++, x++) {
                    float wz = dz + dddx * k;
                    if (!f->ztest || wz > zp[x]) {
                        int tu = wrap(iu >> 16, tx->w, tx->wmask, tx->rs);
                        int tv = wrap(iv >> 16, tx->h, tx->hmask, tx->rt), a = 256;
                        gxj_pixel_type c;
                        if (tx->argb) {
                            uint32_t c32 = tx->argb[tv * tx->w + tu];
                            a = (int)(c32 >> 24);
                            if (a < f->aref) goto next;
                            a += a >> 7;
                            c = rgb565(c32 & 0xffffff);
                        } else {
                            int idx = tx->pix[tv * tx->w + tu];
                            if (!idx && f->colorkey) goto next;
                            c = tx->pal[idx];
                        }
                        if (f->shaded) c = shade565(c, (int)(s + dsdx * k));
                        dst[x] = f->blend ? blend565(dst[x], c, f->blend, a) : c;
                        if (f->zwrite) zp[x] = wz;
                    }
                next:
                    iu += du;
                    iv += dv;
                }
                w += dwdx * n;
                dz += dddx * n;
                u += dudx * n;
                v += dvdx * n;
                s += dsdx * n;
            }
        } else {
            for (x = xs; x < xe; x++, dz += dddx, s += dsdx) {
                if (!f->ztest || dz > zp[x]) {
                    gxj_pixel_type c = f->color;
                    if (f->shaded) c = shade565(c, (int)s);
                    dst[x] = f->blend ? blend565(dst[x], c, f->blend, 256) : c;
                    if (f->zwrite) zp[x] = dz;
                }
            }
        }
    }
}

/* clips against the near plane, culls, then fans into triangles */
/* cull 0 none, 1 drops area <= 0 (micro3d front faces), 2 drops area >= 0 */
static void draw_poly(const Target *t, const View *vw, const VV *in, int n, int cull, const Fill *f) {
    VV clip[8];
    SV sv[8];
    int i, m = 0;
    float area;

    if (vw->persp) {
        int far = 0;
        for (i = 0; i < n; i++)
            if (in[i].z > vw->far) far++;
        if (far == n) return;
        for (i = 0; i < n; i++) {
            const VV *a = &in[i], *b = &in[(i + 1) % n];
            int ain = a->z >= vw->near, bin = b->z >= vw->near;
            if (ain) clip[m++] = *a;
            if (ain != bin) {
                float k = (vw->near - a->z) / (b->z - a->z);
                VV *o = &clip[m++];
                o->x = a->x + (b->x - a->x) * k;
                o->y = a->y + (b->y - a->y) * k;
                o->z = vw->near;
                o->u = a->u + (b->u - a->u) * k;
                o->v = a->v + (b->v - a->v) * k;
                o->s = a->s + (b->s - a->s) * k;
            }
        }
        if (m < 3) return;
    } else {
        memcpy(clip, in, n * sizeof(VV));
        m = n;
    }
    for (i = 0; i < m; i++)
        project(vw, &clip[i], &sv[i]);

    area = 0;
    for (i = 1; i + 1 < m; i++)
        area += (sv[i].x - sv[0].x) * (sv[i + 1].y - sv[0].y) - (sv[i + 1].x - sv[0].x) * (sv[i].y - sv[0].y);
    if ((cull == 1 && area <= 0) || (cull == 2 && area >= 0)) return;

    for (i = 1; i + 1 < m; i++)
        raster_tri(t, &sv[0], &sv[i], &sv[i + 1], f);
}

static int target_get(jobject g, Target *t, gxj_screen_buffer *tmp) {
    java_imagedata *img;
    gxj_screen_buffer *sb;
    java_graphics *jg = GXAPI_GET_GRAPHICS_PTR(g);
    img = jg->img != NULL ? jg->img->imageData : NULL;
    sb = img == NULL ? &gxj_system_screen_buffer : gxj_get_image_screen_buffer_impl(img, tmp, NULL);
    if (sb == NULL || sb->pixelData == NULL) return 0;
    t->px = sb->pixelData;
    t->stride = sb->width;
    t->cx1 = jg->clipX1 < 0 ? 0 : jg->clipX1;
    t->cy1 = jg->clipY1 < 0 ? 0 : jg->clipY1;
    t->cx2 = jg->clipX2 > sb->width ? sb->width : jg->clipX2;
    t->cy2 = jg->clipY2 > sb->height ? sb->height : jg->clipY2;
    if (t->cx1 >= t->cx2 || t->cy1 >= t->cy2) return 0;
    if (zbuf == NULL || zstride != sb->width || zh != sb->height) {
        int need = sb->width * sb->height;
        if (need > zcap) {
            free(zbuf);
            zbuf = (float *)malloc(need * sizeof(float));
            zcap = zbuf ? need : 0;
            if (!zbuf) return 0;
        }
        zstride = sb->width;
        zh = sb->height;
        memset(zbuf, 0, need * sizeof(float));
    }
    return 1;
}

enum { E_CX, E_CY, E_PROJ, E_P0, E_P1, E_P2, E_P3, E_MTX, E_ATTR = 19, E_PATTERN,
       E_AMB, E_DIR, E_LX, E_LY, E_LZ, E_TOON, E_TOON_HI, E_TOON_LO, E_LEN };

static void view_setup(View *vw, const jint *env, jobject g, const Target *t) {
    java_graphics *jg = GXAPI_GET_GRAPHICS_PTR(g);
    /* the projection spans the whole target, not the clip */
    float w = (float)t->stride, h = (float)zh;
    int i;
    vw->cx = (float)(env[E_CX] + jg->transX);
    vw->cy = (float)(env[E_CY] + jg->transY);
    for (i = 0; i < 12; i++)
        vw->m[i] = (i & 3) == 3 ? (float)env[E_MTX + i] : env[E_MTX + i] / 4096.0f;
    vw->persp = env[E_PROJ] >= 2;
    switch (env[E_PROJ]) {
    case 0:
        vw->kx = env[E_P0] / 4096.0f;
        vw->ky = env[E_P1] / 4096.0f;
        break;
    case 1:
        vw->kx = env[E_P0] > 0 ? w / env[E_P0] : 1;
        vw->ky = env[E_P1] > 0 ? h / env[E_P1] : vw->kx;
        break;
    case 2: {
        int ang = env[E_P2] < 2 ? 2 : env[E_P2] > 2046 ? 2046 : env[E_P2];
        vw->kx = vw->ky = (w * 0.5f) / tanf(ang * 3.14159265f / 4096.0f);
        break;
    }
    default:
        vw->kx = env[E_P2] > 0 ? vw->near * 4096.0f * w / env[E_P2] : 1;
        vw->ky = env[E_P3] > 0 ? vw->near * 4096.0f * h / env[E_P3] : vw->kx;
        break;
    }
    vw->zc0 = 0.5f;
    vw->zc1 = -1.0f / 131072.0f;
    if (vw->persp) {
        vw->near = (float)env[E_P0];
        vw->far = (float)env[E_P1];
        if (vw->near < 1) vw->near = 1;
        if (vw->far <= vw->near) vw->far = vw->near + 1;
        /* 1 - GL window depth, which is linear in 1/z */
        vw->zc0 = 0.5f - 0.5f * (vw->far + vw->near) / (vw->far - vw->near);
        vw->zc1 = vw->far * vw->near / (vw->far - vw->near);
    }
    vw->attr = env[E_ATTR];
    vw->lit = (vw->attr & ATTR_LIGHT) != 0;
    if (vw->lit) {
        float lx = (float)env[E_LX], ly = (float)env[E_LY], lz = (float)env[E_LZ];
        float l = sqrtf(lx * lx + ly * ly + lz * lz);
        if (l <= 0) l = 1;
        vw->lx = lx / l; vw->ly = ly / l; vw->lz = lz / l;
        vw->amb = (env[E_AMB] < 0 ? 0 : env[E_AMB] > 4096 ? 4096 : env[E_AMB]) / 4096.0f;
        vw->dir = (env[E_DIR] < 0 ? 0 : env[E_DIR] > 16384 ? 16384 : env[E_DIR]) / 4096.0f;
    }
    vw->toon = (vw->attr & ATTR_TOON) ? env[E_TOON] : -1;
    vw->toonHi = env[E_TOON_HI];
    vw->toonLo = env[E_TOON_LO];
}

static float light_at(const View *vw, const float *n) {
    float nx = vw->m[0] * n[0] + vw->m[1] * n[1] + vw->m[2] * n[2];
    float ny = vw->m[4] * n[0] + vw->m[5] * n[1] + vw->m[6] * n[2];
    float nz = vw->m[8] * n[0] + vw->m[9] * n[1] + vw->m[10] * n[2];
    float l = sqrtf(nx * nx + ny * ny + nz * nz), d, s;
    if (l <= 0) return 255;
    d = (nx * vw->lx + ny * vw->ly + nz * vw->lz) / l;
    if (d < 0) d = 0;
    s = vw->amb + vw->dir * d;
    if (s > 1) s = 1;
    if (vw->toon >= 0)
        return s * 255 < vw->toon ? (float)vw->toonLo : (float)vw->toonHi;
    return s * 255;
}


/* ---------- KNI ---------- */

static void throw_oom(void) {
    KNI_ThrowNew("java/lang/OutOfMemoryError", "micro3d");
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_mascotcapsule_micro3d_v3_Render_figureSize) {
    jint len = KNI_GetParameterAsInt(3), off = KNI_GetParameterAsInt(2), n = -1;
    KNI_StartHandles(1);
    KNI_DeclareHandle(data);
    KNI_GetParameterAsObject(1, data);
    if (!KNI_IsNullHandle(data) && off >= 0 && len > 0 && off + len <= KNI_GetArrayLength(data)) {
        Rd r = {(const uint8_t *)JavaByteArray(data) + off, len, 0, 0, 0, 0};
        MbacHead h;
        if (mbac_head(&r, &h)) n = fig_words(&h);
    }
    KNI_EndHandles();
    KNI_ReturnInt(n);
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_mascotcapsule_micro3d_v3_Render_figureLoad) {
    jint len = KNI_GetParameterAsInt(3), off = KNI_GetParameterAsInt(2);
    KNI_StartHandles(2);
    KNI_DeclareHandle(data);
    KNI_DeclareHandle(blob);
    KNI_GetParameterAsObject(1, data);
    KNI_GetParameterAsObject(4, blob);
    if (!KNI_IsNullHandle(data) && !KNI_IsNullHandle(blob) && off >= 0 && len > 0 &&
        off + len <= KNI_GetArrayLength(data)) {
        Rd r = {(const uint8_t *)JavaByteArray(data) + off, len, 0, 0, 0, 0};
        MbacHead h;
        if (mbac_head(&r, &h) && fig_words(&h) <= KNI_GetArrayLength(blob)) {
            r.pos = 0;
            if (!mbac_load(&r, (int32_t *)JavaIntArray(blob))) {
                JavaIntArray(blob)[F_MAGIC] = 0;
                KNI_ThrowNew("java/lang/IllegalArgumentException", "bad MBAC");
            }
        }
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_mascotcapsule_micro3d_v3_Render_actionSize) {
    jint len = KNI_GetParameterAsInt(3), off = KNI_GetParameterAsInt(2), n = -1;
    KNI_StartHandles(1);
    KNI_DeclareHandle(data);
    KNI_GetParameterAsObject(1, data);
    if (!KNI_IsNullHandle(data) && off >= 0 && len > 0 && off + len <= KNI_GetArrayLength(data)) {
        Rd r = {(const uint8_t *)JavaByteArray(data) + off, len, 0, 0, 0, 0};
        int ver, na, nb;
        if (mtra_scan(&r, &ver, &na, &nb, NULL)) n = A_HDR + na + (len + 3) / 4;
    }
    KNI_EndHandles();
    KNI_ReturnInt(n);
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_mascotcapsule_micro3d_v3_Render_actionLoad) {
    jint len = KNI_GetParameterAsInt(3), off = KNI_GetParameterAsInt(2);
    KNI_StartHandles(2);
    KNI_DeclareHandle(data);
    KNI_DeclareHandle(blob);
    KNI_GetParameterAsObject(1, data);
    KNI_GetParameterAsObject(4, blob);
    if (!KNI_IsNullHandle(data) && !KNI_IsNullHandle(blob) && off >= 0 && len > 0 &&
        off + len <= KNI_GetArrayLength(data)) {
        const uint8_t *src = (const uint8_t *)JavaByteArray(data) + off;
        Rd r = {src, len, 0, 0, 0, 0};
        int ver, na, nb;
        int32_t *a = (int32_t *)JavaIntArray(blob);
        if (mtra_scan(&r, &ver, &na, &nb, NULL) && A_HDR + na + (len + 3) / 4 <= KNI_GetArrayLength(blob)) {
            r.pos = 0;
            mtra_scan(&r, &ver, &na, &nb, a + A_HDR);
            a[A_MAGIC] = ACT_MAGIC;
            a[A_NUM_ACTIONS] = na;
            a[A_NUM_BONES] = nb;
            a[A_VERSION] = ver;
            a[A_LEN] = len;
            memcpy(a + A_HDR + na, src, len);
        }
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_mascotcapsule_micro3d_v3_Render_actionFrames) {
    jint idx = KNI_GetParameterAsInt(2), n = 0;
    KNI_StartHandles(1);
    KNI_DeclareHandle(blob);
    KNI_GetParameterAsObject(1, blob);
    if (!KNI_IsNullHandle(blob)) {
        int32_t *a = (int32_t *)JavaIntArray(blob);
        if (a[A_MAGIC] == ACT_MAGIC && idx >= 0 && idx < a[A_NUM_ACTIONS]) {
            const uint8_t *d = (const uint8_t *)(a + A_HDR + a[A_NUM_ACTIONS]) + a[A_HDR + idx];
            n = d[0] | d[1] << 8;
        }
    }
    KNI_EndHandles();
    /* games compare this against setPosture frames, both 16.16 */
    KNI_ReturnInt(n << 16);
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_mascotcapsule_micro3d_v3_Render_posture) {
    jint frame = KNI_GetParameterAsInt(4), action = KNI_GetParameterAsInt(3);
    KNI_StartHandles(2);
    KNI_DeclareHandle(fh);
    KNI_DeclareHandle(ah);
    KNI_GetParameterAsObject(1, fh);
    KNI_GetParameterAsObject(2, ah);
    if (!KNI_IsNullHandle(fh) && !KNI_IsNullHandle(ah)) {
        int32_t *fig = (int32_t *)JavaIntArray(fh), *a = (int32_t *)JavaIntArray(ah);
        if (fig[F_MAGIC] == FIG_MAGIC && a[A_MAGIC] == ACT_MAGIC && action >= 0 && action < a[A_NUM_ACTIONS]) {
            int nb = a[A_NUM_BONES], b;
            float kf = (frame < 0 ? 0 : frame) / 65536.0f;
            float *mats = (float *)calloc(nb ? nb : 1, 12 * sizeof(float));
            Rd r = {(const uint8_t *)(a + A_HDR + a[A_NUM_ACTIONS]), a[A_LEN], a[A_HDR + action], 0, 0, 0};
            if (mats) {
                rd_u16(&r);
                for (b = 0; b < nb; b++)
                    bone_action(&r, mats + b * 12, kf);
                if (a[A_VERSION] >= 5) {
                    int cnt = rd_u16(&r), i, iframe = (frame < 0 ? 0 : frame) >> 16;
                    for (i = 0; i < cnt; i++) {
                        int fr = rd_u16(&r), p = rd_s32(&r);
                        if (fr <= iframe) fig[F_PATTERN] = p;
                    }
                }
                if (!r.err) fig_transform(fig, mats, nb);
                free(mats);
            } else {
                throw_oom();
            }
        }
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(com_mascotcapsule_micro3d_v3_Render_textureSize) {
    jint len = KNI_GetParameterAsInt(3), off = KNI_GetParameterAsInt(2), n = -1;
    KNI_StartHandles(1);
    KNI_DeclareHandle(data);
    KNI_GetParameterAsObject(1, data);
    if (!KNI_IsNullHandle(data) && off >= 0 && len > 0 && off + len <= KNI_GetArrayLength(data)) {
        Rd r = {(const uint8_t *)JavaByteArray(data) + off, len, 0, 0, 0, 0};
        int w, h, flip, raster, pal, pe;
        if (bmp_head(&r, &w, &h, &flip, &raster, &pal, &pe)) n = T_PIX + (w * h + 3) / 4;
    }
    KNI_EndHandles();
    KNI_ReturnInt(n);
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_mascotcapsule_micro3d_v3_Render_textureLoad) {
    jint len = KNI_GetParameterAsInt(3), off = KNI_GetParameterAsInt(2);
    KNI_StartHandles(2);
    KNI_DeclareHandle(data);
    KNI_DeclareHandle(blob);
    KNI_GetParameterAsObject(1, data);
    KNI_GetParameterAsObject(4, blob);
    if (!KNI_IsNullHandle(data) && !KNI_IsNullHandle(blob) && off >= 0 && len > 0 &&
        off + len <= KNI_GetArrayLength(data)) {
        const uint8_t *d = (const uint8_t *)JavaByteArray(data) + off;
        Rd r = {d, len, 0, 0, 0, 0};
        int w, h, flip, raster, pal, pe, x, y, i;
        if (bmp_head(&r, &w, &h, &flip, &raster, &pal, &pe) && T_PIX + (w * h + 3) / 4 <= KNI_GetArrayLength(blob)) {
            int32_t *t = (int32_t *)JavaIntArray(blob);
            uint16_t *p16 = (uint16_t *)(t + T_PAL);
            uint8_t *px = (uint8_t *)(t + T_PIX);
            int stride = (w + 3) & ~3;
            t[T_MAGIC] = TEX_MAGIC;
            t[T_W] = w;
            t[T_H] = h;
            for (i = 0; i < 256; i++) {
                int o = pal + i * pe;
                uint32_t c = o + 2 < len && o + 2 < raster ? (uint32_t)(d[o + 2] << 16 | d[o + 1] << 8 | d[o]) : 0;
                p16[i] = rgb565(c);
            }
            for (y = 0; y < h; y++) {
                const uint8_t *row = d + raster + (flip ? h - 1 - y : y) * stride;
                for (x = 0; x < w; x++) px[y * w + x] = row[x];
            }
        }
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_mascotcapsule_micro3d_v3_Render_bind) {
    KNI_StartHandles(1);
    KNI_DeclareHandle(g);
    KNI_GetParameterAsObject(1, g);
    if (!KNI_IsNullHandle(g)) {
        gxj_screen_buffer tmp;
        Target t;
        if (target_get(g, &t, &tmp))
            memset(zbuf, 0, zstride * zh * sizeof(float));
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}

static void xform(const View *vw, const float *p, VV *o) {
    const float *m = vw->m;
    o->x = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
    o->y = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
    o->z = m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11];
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_mascotcapsule_micro3d_v3_Render_figure) {
    KNI_StartHandles(5);
    KNI_DeclareHandle(g);
    KNI_DeclareHandle(fh);
    KNI_DeclareHandle(texs);
    KNI_DeclareHandle(envh);
    KNI_DeclareHandle(th);
    KNI_GetParameterAsObject(1, g);
    KNI_GetParameterAsObject(2, fh);
    KNI_GetParameterAsObject(3, texs);
    KNI_GetParameterAsObject(4, envh);
    do {
        gxj_screen_buffer tmp;
        Target t;
        View vw;
        Tex tex[16];
        int ntex = 0, i, j, np, pass;
        int32_t *fig;
        const jint *env;
        const float *xv, *xn;
        const Poly *polys;
        uint32_t pattern;
        VV *cache;
        int nv;

        if (KNI_IsNullHandle(g) || KNI_IsNullHandle(fh) || KNI_IsNullHandle(envh)) break;
        if (!target_get(g, &t, &tmp)) break;
        fig = (int32_t *)JavaIntArray(fh);
        if (fig[F_MAGIC] != FIG_MAGIC) break;
        env = JavaIntArray(envh);
        view_setup(&vw, env, g, &t);
        if (!KNI_IsNullHandle(texs)) {
            ntex = KNI_GetArrayLength(texs);
            if (ntex > 16) ntex = 16;
            for (i = 0; i < ntex; i++) {
                KNI_GetObjectArrayElement(texs, i, th);
                if (KNI_IsNullHandle(th) || !tex_get((const int32_t *)JavaIntArray(th), &tex[i]))
                    tex[i].w = 0;
            }
        }
        nv = fig[F_NV];
        np = fig[F_NP];
        xv = (const float *)(fig + fig[F_OFF_XV]);
        xn = fig[F_HASN] ? (const float *)(fig + fig[F_OFF_XN]) : NULL;
        polys = (const Poly *)(fig + fig[F_OFF_P]);
        pattern = (uint32_t)env[E_PATTERN];

        cache = (VV *)malloc((nv ? nv : 1) * sizeof(VV));
        if (!cache) { throw_oom(); break; }
        for (i = 0; i < nv; i++) {
            xform(&vw, xv + i * 3, &cache[i]);
            cache[i].s = vw.lit && xn ? light_at(&vw, xn + i * 3) : 255;
        }

        /* opaque first so blended faces see what is behind them */
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; i < np; i++) {
                const Poly *p = &polys[i];
                int blend = (vw.attr & ATTR_SEMI) ? p->blend : 0;
                Fill f;
                VV q[4];
                static const uint8_t order[2][3] = {{0, 1, 2}, {2, 1, 3}};
                int tri;
                if ((blend != 0) != pass) continue;
                if ((p->pattern & pattern) != p->pattern) continue;
                f.blend = blend;
                f.zwrite = blend == 0;
                f.ztest = 1;
                f.aref = 0;
                f.colorkey = (p->flags & PF_TRANSP) != 0;
                f.persp = vw.persp;
                f.shaded = vw.lit && (p->flags & PF_LIGHT) && xn;
                if (p->flags & PF_COLOR) {
                    f.tex = NULL;
                    f.color = rgb565(p->color);
                } else {
                    if (p->face >= ntex || tex[p->face].w == 0) continue;
                    f.tex = &tex[p->face];
                }
                for (tri = 0; tri < (p->quad ? 2 : 1); tri++) {
                    for (j = 0; j < 3; j++) {
                        int k = order[tri][j];
                        q[j] = cache[p->v[k]];
                        q[j].u = p->uv[k * 2];
                        q[j].v = p->uv[k * 2 + 1];
                        if (!f.shaded) q[j].s = 255;
                    }
                    draw_poly(&t, &vw, q, 3, (p->flags & PF_DOUBLE) ? 0 : 1, &f);
                }
            }
        }
        free(cache);
    } while (0);
    KNI_EndHandles();
    KNI_ReturnVoid();
}

enum {
    P_POINTS = 0x1000000, P_LINES = 0x2000000, P_TRIS = 0x3000000, P_QUADS = 0x4000000,
    P_SPRITES = 0x5000000, P_COLOR_MASK = 0xc00, P_COLOR_CMD = 0x400, P_NORMAL_MASK = 0x300,
    P_NORMAL_FACE = 0x200, P_NORMAL_VERTEX = 0x300, P_TEX_MASK = 0x3000, P_TEX = 0x3000,
    P_SPRITE_CMD = 0x1000, P_LIGHT = 1, P_COLORKEY = 16
};

static void draw_point(const Target *t, const View *vw, const VV *a, gxj_pixel_type c, int blend) {
    SV s;
    int x, y;
    if (vw->persp && (a->z < vw->near || a->z > vw->far)) return;
    project(vw, a, &s);
    x = (int)floorf(s.x);
    y = (int)floorf(s.y);
    if (x < t->cx1 || y < t->cy1 || x >= t->cx2 || y >= t->cy2) return;
    if (s.d <= zbuf[y * zstride + x]) return;
    t->px[y * t->stride + x] = blend ? blend565(t->px[y * t->stride + x], c, blend, 256) : c;
    if (!blend) zbuf[y * zstride + x] = s.d;
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_mascotcapsule_micro3d_v3_Render_primitives) {
    jint count = KNI_GetParameterAsInt(5), command = KNI_GetParameterAsInt(4);
    KNI_StartHandles(7);
    KNI_DeclareHandle(g);
    KNI_DeclareHandle(th);
    KNI_DeclareHandle(envh);
    KNI_DeclareHandle(vh);
    KNI_DeclareHandle(nh);
    KNI_DeclareHandle(tch);
    KNI_DeclareHandle(ch);
    KNI_GetParameterAsObject(1, g);
    KNI_GetParameterAsObject(2, th);
    KNI_GetParameterAsObject(3, envh);
    KNI_GetParameterAsObject(6, vh);
    KNI_GetParameterAsObject(7, nh);
    KNI_GetParameterAsObject(8, tch);
    KNI_GetParameterAsObject(9, ch);
    do {
        gxj_screen_buffer tmp;
        Target t;
        View vw;
        Tex tex;
        int hasTex = 0, type = command & 0x7000000, i, j, nvp;
        int vlen, tlen, clen;
        const jint *v, *tc, *col;
        Fill f;
        int blend;

        if (KNI_IsNullHandle(g) || KNI_IsNullHandle(envh) || KNI_IsNullHandle(vh)) break;
        if (!target_get(g, &t, &tmp)) break;
        view_setup(&vw, JavaIntArray(envh), g, &t);
        if (!KNI_IsNullHandle(th)) hasTex = tex_get((const int32_t *)JavaIntArray(th), &tex);
        v = JavaIntArray(vh);
        vlen = KNI_GetArrayLength(vh);
        tc = KNI_IsNullHandle(tch) ? NULL : JavaIntArray(tch);
        tlen = tc ? KNI_GetArrayLength(tch) : 0;
        col = KNI_IsNullHandle(ch) ? NULL : JavaIntArray(ch);
        clen = col ? KNI_GetArrayLength(ch) : 0;
        blend = (vw.attr & ATTR_SEMI) ? (command >> 5) & 3 : 0;
        f.blend = blend;
        f.zwrite = blend == 0;
        f.ztest = 1;
        f.aref = 0;
        f.colorkey = (command & P_COLORKEY) != 0;
        f.persp = vw.persp;
        f.shaded = 0;

        if (type == P_POINTS || type == P_LINES) {
            int per = type == P_POINTS ? 1 : 2;
            for (i = 0; i < count; i++) {
                VV a[2];
                float pv[3];
                gxj_pixel_type c;
                if ((i + 1) * per * 3 > vlen) break;
                if ((command & P_COLOR_MASK) == P_COLOR_CMD) c = clen ? rgb565(col[0]) : 0xffff;
                else if (command & P_COLOR_MASK) c = i < clen ? rgb565(col[i]) : 0xffff;
                else continue;
                for (j = 0; j < per; j++) {
                    pv[0] = (float)v[(i * per + j) * 3];
                    pv[1] = (float)v[(i * per + j) * 3 + 1];
                    pv[2] = (float)v[(i * per + j) * 3 + 2];
                    xform(&vw, pv, &a[j]);
                    a[j].u = a[j].v = 0;
                    a[j].s = 255;
                }
                if (per == 1) {
                    draw_point(&t, &vw, &a[0], c, blend);
                } else {
                    int k, steps = 64;
                    for (k = 0; k <= steps; k++) {
                        VV m;
                        float q = k / (float)steps;
                        m.x = a[0].x + (a[1].x - a[0].x) * q;
                        m.y = a[0].y + (a[1].y - a[0].y) * q;
                        m.z = a[0].z + (a[1].z - a[0].z) * q;
                        m.u = m.v = 0;
                        m.s = 255;
                        draw_point(&t, &vw, &m, c, blend);
                    }
                }
            }
            break;
        }

        if (type == P_SPRITES) {
            int params = command & P_TEX_MASK;
            int sw = 0, sh = 0, u0 = 0, v0 = 0, u1 = 0, v1 = 0, mode = 0;
            if (!hasTex || !params) break;
            f.tex = &tex;
            for (i = 0; i < count; i++) {
                VV c;
                float pv[3], hw = 0, hh = 0;
                SV s;
                if ((i + 1) * 3 > vlen) break;
                if (params != P_SPRITE_CMD || i == 0) {
                    int o = params == P_SPRITE_CMD ? 0 : i * 8;
                    if (o + 8 > tlen) break;
                    sw = tc[o]; sh = tc[o + 1];
                    u0 = tc[o + 3]; v0 = tc[o + 4]; u1 = tc[o + 5]; v1 = tc[o + 6];
                    mode = tc[o + 7];
                }
                pv[0] = (float)v[i * 3];
                pv[1] = (float)v[i * 3 + 1];
                pv[2] = (float)v[i * 3 + 2];
                xform(&vw, pv, &c);
                if (vw.persp && (c.z < vw.near || c.z > vw.far)) continue;
                switch (mode) {
                case 0:
                    hw = sw * 0.5f * vw.kx; hh = sh * 0.5f * vw.ky;
                    if (vw.persp) { hw /= c.z; hh /= c.z; }
                    break;
                case 1:
                    hw = sw * 0.5f; hh = sh * 0.5f;
                    if (vw.persp) { hw *= vw.near / c.z; hh *= vw.near / c.z; }
                    break;
                case 2:
                    hw = sw * 0.5f * vw.kx; hh = sh * 0.5f * vw.ky;
                    if (vw.persp) { hw /= vw.near; hh /= vw.near; }
                    break;
                default:
                    hw = sw * 0.5f; hh = sh * 0.5f;
                    break;
                }
                c.u = c.v = 0;
                c.s = 255;
                project(&vw, &c, &s);
                {
                    SV sv[4];
                    float uu[4] = {(float)u0, (float)u1, (float)u1, (float)u0};
                    float vv[4] = {(float)v0, (float)v0, (float)v1, (float)v1};
                    float xx[4] = {s.x - hw, s.x + hw, s.x + hw, s.x - hw};
                    float yy[4] = {s.y - hh, s.y - hh, s.y + hh, s.y + hh};
                    Fill sf = f;
                    sf.persp = 0;
                    for (j = 0; j < 4; j++) {
                        sv[j].x = xx[j]; sv[j].y = yy[j]; sv[j].w = s.w; sv[j].d = s.d;
                        sv[j].u = uu[j]; sv[j].v = vv[j]; sv[j].s = 255;
                    }
                    raster_tri(&t, &sv[0], &sv[1], &sv[2], &sf);
                    raster_tri(&t, &sv[0], &sv[2], &sv[3], &sf);
                }
            }
            break;
        }

        if (type != P_TRIS && type != P_QUADS) break;
        nvp = type == P_TRIS ? 3 : 4;
        if ((command & P_TEX_MASK) == P_TEX) {
            if (!hasTex) break;
            f.tex = &tex;
        } else if (command & P_COLOR_MASK) {
            f.tex = NULL;
        } else {
            break;
        }
        for (i = 0; i < count; i++) {
            VV q[4];
            if ((i + 1) * nvp * 3 > vlen) break;
            if (f.tex) {
                if ((i + 1) * nvp * 2 > tlen) break;
            } else if ((command & P_COLOR_MASK) == P_COLOR_CMD) {
                f.color = clen ? rgb565(col[0]) : 0xffff;
            } else {
                f.color = i < clen ? rgb565(col[i]) : 0xffff;
            }
            for (j = 0; j < nvp; j++) {
                float pv[3];
                pv[0] = (float)v[(i * nvp + j) * 3];
                pv[1] = (float)v[(i * nvp + j) * 3 + 1];
                pv[2] = (float)v[(i * nvp + j) * 3 + 2];
                xform(&vw, pv, &q[j]);
                q[j].u = f.tex ? (float)(tc[(i * nvp + j) * 2] & 0xff) : 0;
                q[j].v = f.tex ? (float)(tc[(i * nvp + j) * 2 + 1] & 0xff) : 0;
                q[j].s = 255;
            }
            draw_poly(&t, &vw, q, nvp, 0, &f);
        }
    } while (0);
    KNI_EndHandles();
    KNI_ReturnVoid();
}

/* ---------- M3G (JSR-184) on the same rasterizer ---------- */

enum {
    M_MV = 0, M_CX = 12, M_CY, M_KX, M_KY, M_NEAR, M_FAR, M_PERSP, M_CULL, M_BLEND, M_AREF,
    M_ZTEST, M_ZWRITE, M_PSCALE, M_PBIAS, M_TSCALE = 28, M_TBIAS, M_TEXMOD = 31, M_REPS, M_REPT,
    M_LIT, M_EMIS, M_AMB, M_DIFF, M_NLIGHTS, M_LIGHTS, M_COLOR = 51, M_VP = 52, M_TCOMPS = 56, M_SKIN, M_LEN
};

KNIEXPORT KNI_RETURNTYPE_INT
KNIDECL(javax_microedition_m3g_Native_inflate) {
    jint len = KNI_GetParameterAsInt(3), off = KNI_GetParameterAsInt(2), n = -1;
    KNI_StartHandles(2);
    KNI_DeclareHandle(src);
    KNI_DeclareHandle(dst);
    KNI_GetParameterAsObject(1, src);
    KNI_GetParameterAsObject(4, dst);
    if (!KNI_IsNullHandle(src) && !KNI_IsNullHandle(dst) && off >= 0 && len > 0 &&
        off + len <= KNI_GetArrayLength(src)) {
        uLongf out = (uLongf)KNI_GetArrayLength(dst);
        if (uncompress((Bytef *)JavaByteArray(dst), &out, (const Bytef *)JavaByteArray(src) + off, len) == Z_OK)
            n = (jint)out;
    }
    KNI_EndHandles();
    KNI_ReturnInt(n);
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(javax_microedition_m3g_Native_bind) {
    KNI_StartHandles(1);
    KNI_DeclareHandle(g);
    KNI_GetParameterAsObject(1, g);
    if (!KNI_IsNullHandle(g)) {
        gxj_screen_buffer tmp;
        Target t;
        if (target_get(g, &t, &tmp))
            memset(zbuf, 0, zstride * zh * sizeof(float));
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}

static void clip_to(Target *t, int x0, int y0, int x1, int y1) {
    if (x0 > t->cx1) t->cx1 = x0;
    if (y0 > t->cy1) t->cy1 = y0;
    if (x1 < t->cx2) t->cx2 = x1;
    if (y1 < t->cy2) t->cy2 = y1;
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(javax_microedition_m3g_Native_clear) {
    KNI_StartHandles(3);
    KNI_DeclareHandle(g);
    KNI_DeclareHandle(ah);
    KNI_DeclareHandle(th);
    KNI_GetParameterAsObject(1, g);
    KNI_GetParameterAsObject(2, ah);
    KNI_GetParameterAsObject(3, th);
    do {
        gxj_screen_buffer tmp;
        Target t;
        const jint *a;
        int x, y;
        if (KNI_IsNullHandle(g) || KNI_IsNullHandle(ah) || KNI_GetArrayLength(ah) < 13) break;
        if (!target_get(g, &t, &tmp)) break;
        a = JavaIntArray(ah);
        clip_to(&t, a[0], a[1], a[0] + a[2], a[1] + a[3]);
        if (t.cx1 >= t.cx2 || t.cy1 >= t.cy2) break;
        if (a[6])
            for (y = t.cy1; y < t.cy2; y++)
                memset(zbuf + y * zstride + t.cx1, 0, (t.cx2 - t.cx1) * sizeof(float));
        if (a[5]) {
            gxj_pixel_type bg = rgb565((uint32_t)a[4] & 0xffffff);
            const jint *tex = KNI_IsNullHandle(th) ? NULL : JavaIntArray(th);
            int tw = tex ? tex[0] : 0, tht = tex ? tex[1] : 0;
            int cw = a[11] > 0 ? a[11] : 1, ch = a[12] > 0 ? a[12] : 1;
            static int cols[1024];
            int ncol = t.cx2 - t.cx1;
            if (ncol > 1024) ncol = 1024;
            for (x = 0; x < ncol; x++) {
                int ix = a[9] + (int)((long long)(t.cx1 + x - a[0]) * cw / (a[2] > 0 ? a[2] : 1));
                if (tex && a[7]) ix = ((ix % tw) + tw) % tw;
                cols[x] = tex && ix >= 0 && ix < tw ? ix : -1;
            }
            for (y = t.cy1; y < t.cy2; y++) {
                gxj_pixel_type *dst = t.px + y * t.stride + t.cx1;
                int iy = a[10] + (int)((long long)(y - a[1]) * ch / (a[3] > 0 ? a[3] : 1));
                const jint *row;
                if (tex && a[8]) iy = ((iy % tht) + tht) % tht;
                if (!tex || iy < 0 || iy >= tht) {
                    for (x = 0; x < ncol; x++) dst[x] = bg;
                    continue;
                }
                row = tex + 2 + iy * tw;
                for (x = 0; x < ncol; x++) {
                    uint32_t c32;
                    if (cols[x] < 0) { dst[x] = bg; continue; }
                    c32 = (uint32_t)row[cols[x]];
                    dst[x] = (c32 >> 24) >= 128 ? rgb565(c32 & 0xffffff) : bg;
                }
            }
        }
    } while (0);
    KNI_EndHandles();
    KNI_ReturnVoid();
}

/* skinned vertices of the last mesh, reused by its other submeshes */
static float *skin_buf;
static int skin_cap, skin_serial;
static const jshort *skin_pos;

static void m3g_draw(Target *tp, const float *p, const jshort *pos, int nv, const jshort *nrm,
                     const jshort *tc, int tcLen, const jint *tris, int ntri, const jint *tx, int txLen,
                     const float *bones, int nb, const jint *infl) {
    Target t = *tp;
    View vw;
    Tex tex;
    Fill f;
    int i, k, tcomps = 2, hasTex = 0, cull, reuse;
    VV *cache;

    clip_to(&t, (int)p[M_VP], (int)p[M_VP + 1], (int)p[M_VP + 2], (int)p[M_VP + 3]);
    if (t.cx1 >= t.cx2 || t.cy1 >= t.cy2) return;

    memset(&vw, 0, sizeof(vw));
    for (i = 0; i < 12; i++) vw.m[i] = p[M_MV + i];
    vw.cx = p[M_CX];
    vw.cy = p[M_CY];
    vw.kx = p[M_KX];
    vw.ky = p[M_KY];
    vw.near = p[M_NEAR] > 0 ? p[M_NEAR] : 0.01f;
    vw.far = p[M_FAR] > vw.near ? p[M_FAR] : vw.near + 1;
    vw.persp = p[M_PERSP] != 0;
    if (vw.persp) {
        vw.zc0 = 0.5f - 0.5f * (vw.far + vw.near) / (vw.far - vw.near);
        vw.zc1 = vw.far * vw.near / (vw.far - vw.near);
    } else {
        vw.zc0 = vw.far / (vw.far - vw.near);
        vw.zc1 = -1.0f / (vw.far - vw.near);
    }
    cull = (int)p[M_CULL];

    if (tx && tc) {
        tcomps = (int)p[M_TCOMPS];
        if (tcomps < 2) tcomps = 2;
        if (txLen >= 2 && tx[0] > 0 && tx[1] > 0 && txLen >= 2 + tx[0] * tx[1] && tcLen >= nv * tcomps) {
            tex.w = tx[0];
            tex.h = tx[1];
            tex.argb = (const uint32_t *)(tx + 2);
            tex.pal = NULL;
            tex.pix = NULL;
            tex.rs = p[M_REPS] != 0;
            tex.rt = p[M_REPT] != 0;
            tex.wmask = tex.rs && (tex.w & (tex.w - 1)) == 0 ? tex.w - 1 : -1;
            tex.hmask = tex.rt && (tex.h & (tex.h - 1)) == 0 ? tex.h - 1 : -1;
            hasTex = 1;
        }
    }
    cache = (VV *)malloc((nv ? nv : 1) * sizeof(VV));
    if (!cache) { throw_oom(); return; }
    {
        int serial = (int)p[M_SKIN];
        reuse = bones && serial && serial == skin_serial && pos == skin_pos && nv <= skin_cap;
        if (bones && !reuse && nv > skin_cap) {
            free(skin_buf);
            skin_buf = (float *)malloc(nv * 6 * sizeof(float));
            skin_cap = skin_buf ? nv : 0;
        }
        if (bones && skin_buf) {
            skin_serial = serial;
            skin_pos = pos;
        } else {
            skin_serial = 0;
        }
    }
    for (i = 0; i < nv; i++) {
        float x = pos[i * 3] * p[M_PSCALE] + p[M_PBIAS];
        float y = pos[i * 3 + 1] * p[M_PSCALE] + p[M_PBIAS + 1];
        float z = pos[i * 3 + 2] * p[M_PSCALE] + p[M_PBIAS + 2];
        float n[3] = {0, 0, 0}, v[3];
        if (nrm) { n[0] = nrm[i * 3]; n[1] = nrm[i * 3 + 1]; n[2] = nrm[i * 3 + 2]; }
        if (reuse) {
            const float *c = skin_buf + i * 6;
            x = c[0]; y = c[1]; z = c[2];
            n[0] = c[3]; n[1] = c[4]; n[2] = c[5];
        } else if (bones) {
            float sx = 0, sy = 0, sz = 0, nx = 0, ny = 0, nz = 0, ws = 0;
            for (k = 0; k < 4; k++) {
                int e = infl[i * 4 + k], b;
                float w;
                const float *m;
                if (e < 0) continue;
                b = e >> 16;
                if (b >= nb) continue;
                w = (e & 0xffff) / 65535.0f;
                m = bones + b * 12;
                sx += w * (m[0] * x + m[1] * y + m[2] * z + m[3]);
                sy += w * (m[4] * x + m[5] * y + m[6] * z + m[7]);
                sz += w * (m[8] * x + m[9] * y + m[10] * z + m[11]);
                nx += w * (m[0] * n[0] + m[1] * n[1] + m[2] * n[2]);
                ny += w * (m[4] * n[0] + m[5] * n[1] + m[6] * n[2]);
                nz += w * (m[8] * n[0] + m[9] * n[1] + m[10] * n[2]);
                ws += w;
            }
            if (ws > 0) {
                x = sx / ws; y = sy / ws; z = sz / ws;
                n[0] = nx; n[1] = ny; n[2] = nz;
            }
            if (skin_serial) {
                float *c = skin_buf + i * 6;
                c[0] = x; c[1] = y; c[2] = z;
                c[3] = n[0]; c[4] = n[1]; c[5] = n[2];
            }
        }
        v[0] = x; v[1] = y; v[2] = z;
        xform(&vw, v, &cache[i]);
        if (hasTex) {
            cache[i].u = (tc[i * tcomps] * p[M_TSCALE] + p[M_TBIAS]) * tex.w;
            cache[i].v = (tc[i * tcomps + 1] * p[M_TSCALE] + p[M_TBIAS + 1]) * tex.h;
        } else {
            cache[i].u = cache[i].v = 0;
        }
        cache[i].s = 255;
        if (p[M_LIT] != 0 && nrm) {
            const float *m = vw.m;
            float ex = m[0] * n[0] + m[1] * n[1] + m[2] * n[2];
            float ey = m[4] * n[0] + m[5] * n[1] + m[6] * n[2];
            float ez = m[8] * n[0] + m[9] * n[1] + m[10] * n[2];
            float l = sqrtf(ex * ex + ey * ey + ez * ez), s = p[M_EMIS] + p[M_AMB];
            int j;
            if (l > 0) {
                ex /= l; ey /= l; ez /= l;
                for (j = 0; j < (int)p[M_NLIGHTS] && j < 4; j++) {
                    const float *L = p + M_LIGHTS + j * 3;
                    float d = ex * L[0] + ey * L[1] + ez * L[2];
                    if (d > 0) s += p[M_DIFF] * d;
                }
            }
            if (s > 1) s = 1;
            cache[i].s = s * 255;
        }
    }

    f.tex = hasTex ? &tex : NULL;
    f.color = rgb565((uint32_t)p[M_COLOR]);
    f.colorkey = 0;
    f.blend = (int)p[M_BLEND];
    f.aref = (int)p[M_AREF];
    f.ztest = p[M_ZTEST] != 0;
    f.zwrite = p[M_ZWRITE] != 0;
    f.persp = vw.persp;
    f.shaded = p[M_LIT] != 0 && nrm && (!hasTex || p[M_TEXMOD] != 0);

    for (i = 0; i < ntri; i++) {
        int a = tris[i * 3], b = tris[i * 3 + 1], c = tris[i * 3 + 2];
        VV q[3];
        if (a < 0 || b < 0 || c < 0 || a >= nv || b >= nv || c >= nv) continue;
        q[0] = cache[a];
        q[1] = cache[b];
        q[2] = cache[c];
        draw_poly(&t, &vw, q, 3, cull, &f);
    }
    free(cache);
}

KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(javax_microedition_m3g_Native_draw) {
    KNI_StartHandles(9);
    KNI_DeclareHandle(g);
    KNI_DeclareHandle(ph);
    KNI_DeclareHandle(posh);
    KNI_DeclareHandle(nh);
    KNI_DeclareHandle(tch);
    KNI_DeclareHandle(trh);
    KNI_DeclareHandle(th);
    KNI_DeclareHandle(bh);
    KNI_DeclareHandle(ih);
    KNI_GetParameterAsObject(1, g);
    KNI_GetParameterAsObject(2, ph);
    KNI_GetParameterAsObject(3, posh);
    KNI_GetParameterAsObject(4, nh);
    KNI_GetParameterAsObject(5, tch);
    KNI_GetParameterAsObject(6, trh);
    KNI_GetParameterAsObject(7, th);
    KNI_GetParameterAsObject(8, bh);
    KNI_GetParameterAsObject(9, ih);
    if (!KNI_IsNullHandle(g) && !KNI_IsNullHandle(ph) && !KNI_IsNullHandle(posh) && !KNI_IsNullHandle(trh) &&
        KNI_GetArrayLength(ph) >= M_LEN) {
        gxj_screen_buffer tmp;
        Target t;
        if (target_get(g, &t, &tmp)) {
            int nv = KNI_GetArrayLength(posh) / 3;
            int nb = KNI_IsNullHandle(bh) ? 0 : KNI_GetArrayLength(bh) / 12;
            int hasInfl = !KNI_IsNullHandle(ih) && KNI_GetArrayLength(ih) >= nv * 4;
            m3g_draw(&t, (const float *)JavaIntArray(ph), JavaShortArray(posh), nv,
                     !KNI_IsNullHandle(nh) && KNI_GetArrayLength(nh) >= nv * 3 ? JavaShortArray(nh) : NULL,
                     KNI_IsNullHandle(tch) ? NULL : JavaShortArray(tch),
                     KNI_IsNullHandle(tch) ? 0 : KNI_GetArrayLength(tch),
                     JavaIntArray(trh), KNI_GetArrayLength(trh) / 3,
                     KNI_IsNullHandle(th) ? NULL : JavaIntArray(th),
                     KNI_IsNullHandle(th) ? 0 : KNI_GetArrayLength(th),
                     nb && hasInfl ? (const float *)JavaIntArray(bh) : NULL, nb,
                     nb && hasInfl ? JavaIntArray(ih) : NULL);
        }
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}

/* local: per node t xyz, q xyzw, s xyz, has general, general 3x4; parents come first */
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(javax_microedition_m3g_Native_bones) {
    KNI_StartHandles(5);
    KNI_DeclareHandle(lh);
    KNI_DeclareHandle(ph);
    KNI_DeclareHandle(bh);
    KNI_DeclareHandle(rh);
    KNI_DeclareHandle(oh);
    KNI_GetParameterAsObject(1, lh);
    KNI_GetParameterAsObject(2, ph);
    KNI_GetParameterAsObject(3, bh);
    KNI_GetParameterAsObject(4, rh);
    KNI_GetParameterAsObject(5, oh);
    if (!KNI_IsNullHandle(lh) && !KNI_IsNullHandle(ph) && !KNI_IsNullHandle(bh) &&
        !KNI_IsNullHandle(rh) && !KNI_IsNullHandle(oh)) {
        int nn = KNI_GetArrayLength(ph), nb = KNI_GetArrayLength(bh), i, b;
        if (KNI_GetArrayLength(lh) >= nn * 23 && KNI_GetArrayLength(rh) >= nb * 12 &&
            KNI_GetArrayLength(oh) >= nb * 12) {
            const float *l = (const float *)JavaIntArray(lh);
            const jint *par = JavaIntArray(ph), *bn = JavaIntArray(bh);
            const float *rest = (const float *)JavaIntArray(rh);
            float *out = (float *)JavaIntArray(oh);
            float *w = (float *)malloc((nn ? nn : 1) * 12 * sizeof(float));
            if (w) {
                for (i = 0; i < nn; i++) {
                    const float *t = l + i * 23;
                    float x = t[3], y = t[4], z = t[5], q = t[6], m[12];
                    float n = x * x + y * y + z * z + q * q, s = n > 0 ? 2 / n : 0;
                    m[0] = (1 - s * (y * y + z * z)) * t[7];
                    m[1] = s * (x * y - q * z) * t[8];
                    m[2] = s * (x * z + q * y) * t[9];
                    m[3] = t[0];
                    m[4] = s * (x * y + q * z) * t[7];
                    m[5] = (1 - s * (x * x + z * z)) * t[8];
                    m[6] = s * (y * z - q * x) * t[9];
                    m[7] = t[1];
                    m[8] = s * (x * z - q * y) * t[7];
                    m[9] = s * (y * z + q * x) * t[8];
                    m[10] = (1 - s * (x * x + y * y)) * t[9];
                    m[11] = t[2];
                    if (t[10] != 0) mat_mul(m, m, t + 11);
                    if (par[i] >= 0 && par[i] < i) mat_mul(w + i * 12, w + par[i] * 12, m);
                    else memcpy(w + i * 12, m, sizeof(m));
                }
                for (b = 0; b < nb; b++) {
                    if (bn[b] >= 0 && bn[b] < nn) mat_mul(out + b * 12, w + bn[b] * 12, rest + b * 12);
                    else memcpy(out + b * 12, rest + b * 12, 12 * sizeof(float));
                }
                free(w);
            }
        }
    }
    KNI_EndHandles();
    KNI_ReturnVoid();
}
