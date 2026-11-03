#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <zlib.h>
#include "javacall_keypress.h"
#include "j2vita.h"

void javacall_printf(const char *fmt, ...);

static char jar_path[256];

/* last ux0: path in the launch hint is the game's JAR */
void autokeys_set_jar(const char *hint) {
    const char *p = NULL, *q = hint;
    jar_path[0] = 0;
    while (q && (q = strstr(q, "ux0:")) != NULL)
        p = q++;
    if (p)
        snprintf(jar_path, sizeof(jar_path), "%s", p);
}

static unsigned rd16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }

static unsigned rd16be(const unsigned char *p) { return p[0] << 8 | p[1]; }
static int rd32be(const unsigned char *p) { return (int)((unsigned)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]); }

/* key code constants the game compares against: code + 32 for -32..95 */
#define KC 128
typedef struct { int use[KC]; int game_action, classes; } Counts;

static void count(Counts *c, int v) {
    if (v >= -32 && v < KC - 32)
        c->use[v + 32]++;
}

/* bytes per opcode, 0 = variable (switches, wide) or invalid */
static unsigned op_size(unsigned op) {
    if (op == 0x10 || op == 0x12 || (op >= 0x15 && op <= 0x19) || (op >= 0x36 && op <= 0x3a) ||
        op == 0xa9 || op == 0xbc)
        return 2;
    if (op == 0x11 || op == 0x13 || op == 0x14 || op == 0x84 || (op >= 0x99 && op <= 0xa8) ||
        (op >= 0xb2 && op <= 0xb8) || op == 0xbb || op == 0xbd || op == 0xc0 || op == 0xc1 ||
        op == 0xc6 || op == 0xc7)
        return 3;
    if (op == 0xc5)
        return 4;
    if (op == 0xb9 || op == 0xba || op == 0xc8 || op == 0xc9)
        return 5;
    return op == 0xaa || op == 0xab || op == 0xc4 || op > 0xc9 ? 0 : 1;
}

/* a constant counts when it is compared, a switch case or stored in an array */
static int is_use(unsigned char next) {
    return next == 0x9f || next == 0xa0 || next == 0x4f || (next >= 0x54 && next <= 0x56);
}

static void scan_code(const unsigned char *b, unsigned n, const int *cp_int, unsigned cp_n, Counts *c) {
    unsigned i = 0;
    while (i < n) {
        unsigned op = b[i], len = op_size(op);
        int v = 0, has = 0;
        if (op == 0xaa || op == 0xab) {
            unsigned a = (i + 4) & ~3u, k;
            if (a + 12 > n)
                return;
            if (op == 0xaa) {
                int def = rd32be(b + a), lo = rd32be(b + a + 4), hi = rd32be(b + a + 8);
                if (hi < lo || a + 12 + 4u * (unsigned)(hi - lo + 1) > n)
                    return;
                for (k = 0; k <= (unsigned)(hi - lo); k++)
                    if (rd32be(b + a + 12 + 4 * k) != def)
                        count(c, lo + (int)k);
                i = a + 12 + 4 * (unsigned)(hi - lo + 1);
            } else {
                int pairs = rd32be(b + a + 4);
                if (pairs < 0 || a + 8 + 8u * (unsigned)pairs > n)
                    return;
                for (k = 0; k < (unsigned)pairs; k++)
                    count(c, rd32be(b + a + 8 + 8 * k));
                i = a + 8 + 8 * (unsigned)pairs;
            }
            continue;
        }
        if (op == 0xc4)
            len = i + 1 < n && b[i + 1] == 0x84 ? 6 : 4;
        if (!len || i + len > n)
            return;
        if (op >= 0x02 && op <= 0x08) { v = (int)op - 3; has = 1; }
        else if (op == 0x10) { v = (signed char)b[i + 1]; has = 1; }
        else if (op == 0x11) { v = (short)rd16be(b + i + 1); has = 1; }
        else if (op == 0x12 || op == 0x13) {
            unsigned idx = op == 0x12 ? b[i + 1] : rd16be(b + i + 1);
            if (idx < cp_n && cp_int[idx] != 0x7fffffff) { v = cp_int[idx]; has = 1; }
        }
        if (has && i + len < n && is_use(b[i + len]))
            count(c, v);
        i += len;
    }
}

static void scan_class(const unsigned char *b, size_t n, Counts *c) {
    unsigned cp_n, i, code_idx = 0, k, cnt;
    int *cp_int;
    size_t o = 10;
    if (n < 10 || rd32be(b) != (int)0xCAFEBABE)
        return;
    cp_n = rd16be(b + 8);
    if (!(cp_int = malloc(cp_n * sizeof(int) + 1)))
        return;
    for (i = 1; i < cp_n && o < n; i++) {
        unsigned tag = b[o];
        cp_int[i] = 0x7fffffff;
        if (tag == 1) {
            unsigned l = rd16be(b + o + 1);
            if (l == 4 && !memcmp(b + o + 3, "Code", 4)) code_idx = i;
            if (l == 13 && !memcmp(b + o + 3, "getGameAction", 13)) c->game_action++;
            o += 3 + l;
        } else if (tag == 3) { cp_int[i] = rd32be(b + o + 1); o += 5; }
        else if (tag == 4 || tag == 9 || tag == 10 || tag == 11 || tag == 12 || tag == 18) o += 5;
        else if (tag == 5 || tag == 6) { o += 9; cp_int[++i] = 0x7fffffff; }
        else if (tag == 7 || tag == 8 || tag == 16) o += 3;
        else if (tag == 15) o += 4;
        else break;
    }
    c->classes++;
    o += 6;
    if (o + 2 <= n) o += 2 + 2u * rd16be(b + o);
    /* fields, then methods */
    for (k = 0; k < 2 && o + 2 <= n; k++) {
        unsigned m;
        cnt = rd16be(b + o);
        o += 2;
        for (m = 0; m < cnt && o + 8 <= n; m++) {
            unsigned a, acnt = rd16be(b + o + 6);
            o += 8;
            for (a = 0; a < acnt && o + 6 <= n; a++) {
                unsigned name = rd16be(b + o), alen = (unsigned)rd32be(b + o + 2);
                if (k == 1 && name == code_idx && o + 14 <= n) {
                    unsigned clen = (unsigned)rd32be(b + o + 10);
                    if (o + 14 + clen <= n)
                        scan_code(b + o + 14, clen, cp_int, cp_n, c);
                }
                o += 6 + alen;
            }
        }
    }
    free(cp_int);
}

/* the biggest picture as large as a screen preset is usually a full screen splash;
 * square ones are left out, those are mostly sprite sheets */
static void png_screen(const unsigned char *b, unsigned n, int *best, int *area) {
    int w, h, i;
    if (n < 24 || memcmp(b, "\x89PNG", 4))
        return;
    w = rd32be(b + 16);
    h = rd32be(b + 20);
    for (i = 0; w != h && w * h > *area && i < j2v_size_count; i++)
        if (j2v_sizes[i].w == w && j2v_sizes[i].h == h) {
            *best = i;
            *area = w * h;
        }
}

/* classes go to c, or with png set only the PNG headers are read */
static int scan_jar(const char *path, Counts *c, int *png) {
    int area = 0;
    FILE *f = fopen(path, "rb");
    unsigned char *jar = NULL, *p;
    long len;
    unsigned i, count, off;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 22 || len > 16 * 1024 * 1024 || !(jar = malloc(len)) || fread(jar, 1, len, f) != (size_t)len) {
        fclose(f);
        free(jar);
        return 0;
    }
    fclose(f);
    /* end of central directory record, searched backwards past any comment */
    for (p = jar + len - 22; p >= jar && rd32(p) != 0x06054b50; p--)
        ;
    if (p < jar) {
        free(jar);
        return 0;
    }
    count = rd16(p + 10);
    off = rd32(p + 16);
    for (i = 0; i < count && off + 46 <= (unsigned)len; i++) {
        const unsigned char *e = jar + off;
        unsigned method = rd16(e + 10), csize = rd32(e + 20), usize = rd32(e + 24);
        unsigned nlen = rd16(e + 28), xlen = rd16(e + 30), clen = rd16(e + 32), loc = rd32(e + 42);
        if (rd32(e) != 0x02014b50)
            break;
        off += 46 + nlen + xlen + clen;
        if (png ? nlen < 5 || strncasecmp((const char *)e + 46 + nlen - 4, ".png", 4)
                : nlen < 7 || memcmp(e + 46 + nlen - 6, ".class", 6))
            continue;
        if (loc + 30 > (unsigned)len)
            continue;
        {
            const unsigned char *l = jar + loc;
            unsigned data = loc + 30 + rd16(l + 26) + rd16(l + 28);
            if (data + csize > (unsigned)len || usize > 4 * 1024 * 1024)
                continue;
            if (png && method == 0) {
                png_screen(jar + data, csize, png, &area);
            } else if (png && method == 8) {
                unsigned char hdr[24];
                z_stream z;
                memset(&z, 0, sizeof(z));
                if (inflateInit2(&z, -MAX_WBITS) == Z_OK) {
                    z.next_in = jar + data;
                    z.avail_in = csize;
                    z.next_out = hdr;
                    z.avail_out = sizeof(hdr);
                    inflate(&z, Z_SYNC_FLUSH);
                    png_screen(hdr, z.total_out, png, &area);
                    inflateEnd(&z);
                }
            } else if (method == 0) {
                scan_class(jar + data, csize, c);
            } else if (method == 8) {
                unsigned char *out = malloc(usize ? usize : 1);
                z_stream z;
                memset(&z, 0, sizeof(z));
                if (out && inflateInit2(&z, -MAX_WBITS) == Z_OK) {
                    z.next_in = jar + data;
                    z.avail_in = csize;
                    z.next_out = out;
                    z.avail_out = usize;
                    if (inflate(&z, Z_FINISH) == Z_STREAM_END)
                        scan_class(out, z.total_out, c);
                    inflateEnd(&z);
                }
                free(out);
            }
        }
    }
    free(jar);
    return png ? area > 0 : c->classes > 0;
}

int autokeys_screen_size(void) {
    int best = -1;
    if (!jar_path[0] || !scan_jar(jar_path, NULL, &best))
        return -1;
    javacall_printf("screen: %dx%d image in the JAR\n", j2v_sizes[best].w, j2v_sizes[best].h);
    return best;
}

static int has_word(const char *s, const char *w) {
    size_t n = strlen(w);
    for (; *s; s++)
        if (!strncasecmp(s, w, n))
            return 1;
    return 0;
}

#define U(v) c.use[(v) + 32]

#define NEXTRA ((int)(sizeof(extras) / sizeof(extras[0])))

static int listed(const int *k, int n, int key) {
    while (n-- > 0)
        if (k[n] == key)
            return 1;
    return 0;
}

void cfg_autodetect(GameConfig *cfg) {
    /* extra phone keys a game may need, the most used go to these buttons in order */
    static const int extras[] = {'0', '1', '3', '7', '9', '*', '#'};
    static const int slot[4] = {BTN_SQUARE, BTN_TRIANGLE, BTN_REAR_L, BTN_REAR_R};
    /* left over buttons keep their default unless a found key took it */
    static const int spare[] = {'*', '#', '1', '3', '0', '7', '9'};
    Counts c;
    int nokia, moto, arrows, digits, pick[4], got[4], np = 0, ng = 0, i, j;
    memset(&c, 0, sizeof(c));
    if (!jar_path[0] || !scan_jar(jar_path, &c, NULL))
        return;
    nokia = U(-6) + U(-7);
    moto = U(-21) + U(-22);
    /* -1 is too common a constant to tell anything */
    arrows = U(-2) + U(-3) + U(-4);
    digits = U('2') + U('4') + U('6') + U('8');
    if (moto > 0 && (has_word(jar_path, "moto") ? !has_word(jar_path, "nokia") || moto > nokia
                                                 : moto > nokia && !has_word(jar_path, "nokia")))
        cfg->keyset = KEYSET_MOTOROLA;
    else
        cfg->keyset = KEYSET_NOKIA;
    /* no game actions and no arrow codes: the game only listens to 2/4/6/8 */
    cfg->numpad_dpad = !c.game_action && arrows < 3 && digits >= 4;
    /* Fire also counts as 5 for getGameAction, so 5 only when the game wants the raw '5' alone */
    cfg->map[BTN_CROSS] = U('5') && !U(-5) ? JAVACALL_KEY_5 : JAVACALL_KEY_SELECT;
    for (i = 0; i < NEXTRA; i++) {
        int n = U(extras[i]);
        if (!n || (np == 4 && n <= U(pick[3])))
            continue;
        for (j = np < 4 ? np++ : 3; j > 0 && U(pick[j - 1]) < n; j--)
            pick[j] = pick[j - 1];
        pick[j] = extras[i];
    }
    for (i = 0, j = 0; i < 4; i++) {
        int *m = &cfg->map[slot[i]];
        /* a button the player set to None stays off, rear touch included */
        if (*m == JAVACALL_KEY_INVALID)
            continue;
        if (j < np) {
            *m = pick[j++];
        } else if (listed(pick, np, *m) || listed(got, ng, *m)) {
            int k;
            for (k = 0; listed(pick, np, spare[k]) || listed(got, ng, spare[k]); k++)
                ;
            *m = spare[k];
        }
        got[ng++] = *m;
    }
    javacall_printf("autokeys: %d classes nokia %d moto %d arrows %d digits %d fire %d five %d gameaction %d"
                    " -> keys %d dpad %d cross %d square %d triangle %d rear %d %d\n",
                    c.classes, nokia, moto, arrows, digits, U(-5), U('5'), c.game_action,
                    cfg->keyset, cfg->numpad_dpad, cfg->map[BTN_CROSS], cfg->map[BTN_SQUARE],
                    cfg->map[BTN_TRIANGLE], cfg->map[BTN_REAR_L], cfg->map[BTN_REAR_R]);
}
