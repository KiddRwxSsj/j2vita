#include <string.h>
#include <math.h>
#include <stdio.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/display.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>
#include <vitaGL.h>
#include "j2vita.h"

void javacall_log_flush(void);
void javacall_printf(const char *fmt, ...);
void javacall_log_batch(void);

#define OVL_W 480
#define OVL_H 272

static uint16_t frame[GAME_MAX_W * GAME_MAX_H];
static uint32_t overlay[OVL_W * OVL_H];
static uint32_t kp_px[KP_W * KP_H];
static uint32_t hud_px[HUD_W * HUD_H];
static volatile int hud_dirty = 1;
/* performance OSD, drawn 1:1 in the top right corner */
#define OSD_W 140
#define OSD_H 88
static uint32_t osd_px[OSD_W * OSD_H];
static int osd_w;

static int game_w = 480, game_h = 272;
static volatile int frame_dirty, size_dirty, fps_shown, frames_in, overlay_dirty;
static SceKernelLwMutexWork lock;
static SceUID wake_sema, resume_sema;
static volatile int waiting_resume;

/* base rect x/y/w/h is what the Screen mode gives for the nearest quarter turn;
 * the game is drawn as a qw x qh quad centred on cx/cy and turned by angle */
typedef struct { int x, y, w, h; float cx, cy, qw, qh, angle; } Layout;

static int quarter(int angle) { return ((angle + 45) / 90) & 3; }

static void layout(Layout *l) {
    int vw, vh, q = j2v_in_game ? quarter(j2v_cfg.rotate) : 0;
    float z = j2v_in_game ? j2v_cfg.zoom / 100.0f : 1.0f;
    vw = q & 1 ? game_h : game_w;
    vh = q & 1 ? game_w : game_h;
    if (j2v_in_game && j2v_cfg.scale == SCALE_STRETCH) {
        l->w = SCREEN_W;
        l->h = SCREEN_H;
    } else if (j2v_in_game && j2v_cfg.scale == SCALE_INTEGER) {
        int s = SCREEN_W / vw < SCREEN_H / vh ? SCREEN_W / vw : SCREEN_H / vh;
        if (s < 1)
            s = 1;
        l->w = vw * s;
        l->h = vh * s;
    } else if (SCREEN_W * vh <= SCREEN_H * vw) {
        l->w = SCREEN_W;
        l->h = vh * SCREEN_W / vw;
    } else {
        l->h = SCREEN_H;
        l->w = vw * SCREEN_H / vh;
    }
    if (l->w > SCREEN_W) l->w = SCREEN_W;
    if (l->h > SCREEN_H) l->h = SCREEN_H;
    l->x = (SCREEN_W - l->w) / 2;
    l->y = (SCREEN_H - l->h) / 2;
    l->qw = (q & 1 ? l->h : l->w) * z;
    l->qh = (q & 1 ? l->w : l->h) * z;
    l->cx = SCREEN_W / 2 + (j2v_in_game ? j2v_cfg.off_x : 0);
    l->cy = SCREEN_H / 2 + (j2v_in_game ? j2v_cfg.off_y : 0);
    l->angle = j2v_in_game ? (float)j2v_cfg.rotate : 0;
}

int video_touch_to_game(int tx, int ty, int *gx, int *gy) {
    Layout l;
    float a, dx, dy, u, v;
    int x, y;
    layout(&l);
    a = l.angle * (float)M_PI / 180.0f;
    dx = tx * (float)SCREEN_W / 1920 - l.cx;
    dy = ty * (float)SCREEN_H / 1088 - l.cy;
    /* undo the turn, y points down so positive angles are clockwise */
    u = dx * cosf(a) + dy * sinf(a) + l.qw / 2;
    v = -dx * sinf(a) + dy * cosf(a) + l.qh / 2;
    if (u < 0 || v < 0 || u >= l.qw || v >= l.qh)
        return 0;
    x = (int)(u * game_w / l.qw);
    y = (int)(v * game_h / l.qh);
    *gx = x < 0 ? 0 : x >= game_w ? game_w - 1 : x;
    *gy = y < 0 ? 0 : y >= game_h ? game_h - 1 : y;
    return 1;
}

static void place(int px, int py, int scale, int bw, int bh, int def_x, int def_y,
                  int *x, int *y, int *w, int *h) {
    *w = bw * scale / 100;
    *h = bh * scale / 100;
    *x = px < 0 ? def_x - (def_x > SCREEN_W / 2 ? *w : 0) : px;
    *y = py < 0 ? def_y - (def_y > SCREEN_H / 2 ? *h : *h / 2) : py;
    /* keep a grip of it on screen */
    if (*x > SCREEN_W - 40) *x = SCREEN_W - 40;
    if (*y > SCREEN_H - 40) *y = SCREEN_H - 40;
    if (*x < 40 - *w) *x = 40 - *w;
    if (*y < 40 - *h) *y = 40 - *h;
}

void video_keypad_rect(int *x, int *y, int *w, int *h) {
    place(j2v_cfg.kp_x, j2v_cfg.kp_y, j2v_cfg.kp_scale, KP_W, KP_H, SCREEN_W - 12, SCREEN_H / 2, x, y, w, h);
}

void video_hud_rect(int *x, int *y, int *w, int *h) {
    place(j2v_cfg.hud_x, j2v_cfg.hud_y, j2v_cfg.hud_scale, HUD_W, HUD_H, SCREEN_W - 12, SCREEN_H - 12, x, y, w, h);
}

void hud_changed(void) {
    hud_dirty = 1;
    sceKernelSignalSema(wake_sema, 1);
}

void video_set_game_size(int w, int h) {
    if (w <= 0 || h <= 0 || w > GAME_MAX_W || h > GAME_MAX_H)
        return;
    sceKernelLockLwMutex(&lock, 1, NULL);
    game_w = w;
    game_h = h;
    memset(frame, 0, sizeof(frame));
    size_dirty = 1;
    sceKernelUnlockLwMutex(&lock, 1);
}

void video_submit(const uint16_t *src, int y0, int y1, int src_is_rows) {
    sceKernelLockLwMutex(&lock, 1, NULL);
    if (y0 < 0) y0 = 0;
    if (y1 > game_h) y1 = game_h;
    if (y1 > y0) {
        const uint16_t *s = src_is_rows ? src : src + y0 * game_w;
        uint16_t *d = frame + y0 * game_w;
        int i, n = (y1 - y0) * game_w;
        /* the GPU samples 565 textures as BGR on hardware (Vita3K gets this wrong) */
        for (i = 0; i < n; i++) {
            uint16_t p = s[i];
            d[i] = (uint16_t)((p >> 11) | (p & 0x07E0) | (p << 11));
        }
        frame_dirty = 1;
        frames_in++;
    }
    sceKernelUnlockLwMutex(&lock, 1);
    sceKernelSignalSema(wake_sema, 1);
}

/* called by the VM thread after a flush, this is what freezes the game behind the menu */
void video_wait_unpaused(void) {
    if (!menu_is_open())
        return;
    waiting_resume = 1;
    while (menu_is_open()) {
        SceUInt t = 100000;
        sceKernelWaitSema(resume_sema, 1, &t);
    }
    waiting_resume = 0;
}

void video_redraw(void) {
    overlay_dirty = 1;
    sceKernelSignalSema(wake_sema, 1);
    if (!menu_is_open() && waiting_resume)
        sceKernelSignalSema(resume_sema, 1);
}

extern unsigned char page_0000_00ff[];

void text_draw(uint32_t *fb, int fw, int fh, int x, int y, const char *s, uint32_t color) {
    for (; *s; s++, x += 8) {
        const unsigned char *g = page_0000_00ff + 8 + (unsigned char)*s * 16;
        int r, c;
        for (r = 0; r < 16; r++) {
            if (y + r < 0 || y + r >= fh)
                continue;
            for (c = 0; c < 8; c++)
                if ((g[r] & (0x80 >> c)) && x + c >= 0 && x + c < fw)
                    fb[(y + r) * fw + x + c] = color;
        }
    }
}

static void rect(float x0, float y0, float x1, float y1) {
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x0, y0, 0);
    glTexCoord2f(1, 0); glVertex3f(x1, y0, 0);
    glTexCoord2f(1, 1); glVertex3f(x1, y1, 0);
    glTexCoord2f(0, 1); glVertex3f(x0, y1, 0);
    glEnd();
}

static void game_quad(const Layout *l) {
    glPushMatrix();
    glTranslatef(l->cx, l->cy, 0);
    glRotatef(l->angle, 0, 0, 1);
    rect(-l->qw / 2, -l->qh / 2, l->qw / 2, l->qh / 2);
    glPopMatrix();
}

static void layer(GLuint tex, int x, int y, int w, int h, float alpha) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(1, 1, 1, alpha);
    rect((float)x, (float)y, (float)(x + w), (float)(y + h));
    glColor4f(1, 1, 1, 1);
}

/* touch ux0:data/j2vita/shot.req to get the next shown frame as raw RGBA in shot.raw */
static void dump_screen(void) {
    SceDisplayFrameBuf fb;
    SceIoStat st;
    SceUID f;
    int y;
    if (sceIoGetstat(J2V_DATA "/shot.req", &st) < 0)
        return;
    memset(&fb, 0, sizeof(fb));
    fb.size = sizeof(fb);
    if (sceDisplayGetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0 || !fb.base ||
        (f = sceIoOpen(J2V_DATA "/shot.raw", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777)) < 0) {
        sceIoRemove(J2V_DATA "/shot.req");
        return;
    }
    for (y = 0; y < (int)fb.height; y++)
        sceIoWrite(f, (uint32_t *)fb.base + y * fb.pitch, fb.width * 4);
    sceIoClose(f);
    sceIoRemove(J2V_DATA "/shot.req");
}

static void set_filter(int smooth) {
    GLint f = smooth ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
}

/* fps: game frames in the last second; load: % of that second the VM was busy.
 * A game that leaves the VM idle a good part of the second waits on its own
 * frame timer, so that frame rate is its limit; while it keeps the CPU busy the
 * last limit seen is shown next to the frame rate. */
#define PACED_LOAD 88
static int caps[4], cap_n, cap_i;

static void build_osd(int fps, int load) {
    char mode[24];
    int paced, cap = 0, i;
    for (i = 0; i < cap_n; i++)
        if (caps[i] > cap)
            cap = caps[i];
    /* a busy VM at the game's own limit is still the game's limit */
    paced = load < PACED_LOAD || (cap > 0 && fps >= cap);
    if (paced && fps >= 5) {
        caps[cap_i] = fps;
        cap_i = (cap_i + 1) & 3;
        if (cap_n < 4)
            cap_n++;
        if (fps > cap)
            cap = fps;
    }
    snprintf(mode, sizeof(mode), "%dx%d %dMHz", game_w, game_h, scePowerGetArmClockFrequency());
    osd_w = osd_draw(osd_px, OSD_W, OSD_H, fps > 999 ? 999 : fps, cap, load, paced,
                     j2v_cfg.show_fps == OSD_FULL, mode);
}

static int render_thread(SceSize args, void *argp) {
    GLuint tex_game, tex_ovl, tex_fps, tex_kp, tex_hud;
    int tex_w = 0, tex_h = 0, have_frame = 0, kp_shown = 0, hud_shown = 0;
    SceUInt64 fps_t0 = sceKernelGetProcessTimeWide(), idle_t0 = j2v_vm_idle_us;
    (void)args; (void)argp;

    /* runs next to the VM boot, nothing draws before the first frame anyway */
    javacall_printf("video: vglInit\n");
    vglInit(0x100000);
    vglWaitVblankStart(GL_TRUE);
    javacall_printf("video: ready\n");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SCREEN_W, SCREEN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glClearColor(0, 0, 0, 1);

    glGenTextures(1, &tex_game);
    glGenTextures(1, &tex_ovl);
    glGenTextures(1, &tex_fps);
    glGenTextures(1, &tex_kp);
    glGenTextures(1, &tex_hud);
    glBindTexture(GL_TEXTURE_2D, tex_kp);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, KP_W, KP_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, kp_px);
    set_filter(1);
    glBindTexture(GL_TEXTURE_2D, tex_hud);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HUD_W, HUD_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, hud_px);
    set_filter(1);
    glBindTexture(GL_TEXTURE_2D, tex_ovl);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, OVL_W, OVL_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, overlay);
    set_filter(0);
    glBindTexture(GL_TEXTURE_2D, tex_fps);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, OSD_W, OSD_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, osd_px);
    set_filter(0);

    for (;;) {
        Layout l;
        int menu, keypad, n, ime, adjust, hud;
        SceUInt t = ime_busy() ? 16000 : 50000;
        SceUInt64 now;

        sceKernelWaitSema(wake_sema, 1, &t);
        ime = ime_poll();
        menu = menu_is_open();
        adjust = !menu && adjust_is_open();
        keypad = !menu && keypad_is_open();
        hud = !menu && j2v_in_game && j2v_cfg.hud;

        now = sceKernelGetProcessTimeWide();
        if (now - fps_t0 >= 1000000) {
            uint64_t idle = j2v_vm_idle_us, span = now - fps_t0;
            int load = idle - idle_t0 >= span ? 0 : (int)(100 - (idle - idle_t0) * 100 / span);
            sceKernelLockLwMutex(&lock, 1, NULL);
            n = frames_in;
            frames_in = 0;
            sceKernelUnlockLwMutex(&lock, 1);
            fps_t0 = now;
            idle_t0 = idle;
            javacall_log_flush();
            dump_screen();
            if (!j2v_in_game)
                cap_n = 0;
            if (j2v_in_game && j2v_cfg.show_fps) {
                build_osd(n, load);
                glBindTexture(GL_TEXTURE_2D, tex_fps);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, OSD_W, OSD_H, GL_RGBA, GL_UNSIGNED_BYTE, osd_px);
            }
            fps_shown = 1;
        } else if (!frame_dirty && !size_dirty && !menu && !ime && !fps_shown && !adjust &&
                   !overlay_dirty && !(hud && hud_dirty) && kp_shown == keypad && hud_shown == hud) {
            continue;
        }
        fps_shown = 0;

        sceKernelLockLwMutex(&lock, 1, NULL);
        if (size_dirty || frame_dirty) {
            glBindTexture(GL_TEXTURE_2D, tex_game);
            if (size_dirty || tex_w != game_w || tex_h != game_h) {
                tex_w = game_w;
                tex_h = game_h;
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_w, tex_h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_w, tex_h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
            }
            size_dirty = frame_dirty = 0;
            if (!have_frame)
                javacall_log_batch();
            have_frame = 1;
        }
        layout(&l);
        sceKernelUnlockLwMutex(&lock, 1);

        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_BLEND);
        if (have_frame) {
            glBindTexture(GL_TEXTURE_2D, tex_game);
            set_filter(j2v_in_game && (j2v_cfg.smooth || (j2v_cfg.rotate % 90) != 0));
            game_quad(&l);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (hud) {
            int x, y, w, h;
            if (hud_dirty || !hud_shown) {
                hud_dirty = 0;
                hud_draw(hud_px);
                glBindTexture(GL_TEXTURE_2D, tex_hud);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HUD_W, HUD_H, GL_RGBA, GL_UNSIGNED_BYTE, hud_px);
            }
            video_hud_rect(&x, &y, &w, &h);
            layer(tex_hud, x, y, w, h, 0.9f);
        }
        if (keypad) {
            int x, y, w, h;
            /* the keypad only changes when the player touches it */
            if (overlay_dirty || !kp_shown) {
                keypad_draw(kp_px);
                glBindTexture(GL_TEXTURE_2D, tex_kp);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, KP_W, KP_H, GL_RGBA, GL_UNSIGNED_BYTE, kp_px);
            }
            video_keypad_rect(&x, &y, &w, &h);
            layer(tex_kp, x, y, w, h, j2v_cfg.kp_alpha / 100.0f);
        }
        if (menu || adjust) {
            if (menu)
                menu_draw(overlay, OVL_W, OVL_H);
            else
                adjust_draw(overlay, OVL_W, OVL_H);
            glBindTexture(GL_TEXTURE_2D, tex_ovl);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, OVL_W, OVL_H, GL_RGBA, GL_UNSIGNED_BYTE, overlay);
            layer(tex_ovl, 0, 0, SCREEN_W, SCREEN_H, 1.0f);
        }
        overlay_dirty = 0;
        hud_shown = hud;
        kp_shown = keypad;
        if (!menu && j2v_in_game && j2v_cfg.show_fps && osd_w) {
            layer(tex_fps, SCREEN_W - osd_w - 4, 4, OSD_W, OSD_H, 1.0f);
        }
        /* the system keyboard is drawn on top by vitaGL while it is up */
        vglSwapBuffers(ime ? GL_TRUE : GL_FALSE);
    }
    return 0;
}

void video_start(void) {
    SceUID th;
    sceKernelCreateLwMutex(&lock, "j2v_video", 0, 0, NULL);
    wake_sema = sceKernelCreateSema("j2v_wake", 0, 0, 1, NULL);
    resume_sema = sceKernelCreateSema("j2v_resume", 0, 0, 1, NULL);
    th = sceKernelCreateThread("j2v_render", render_thread, 0x10000100, 0x40000, J2V_HELPER_CPUS, 0, NULL);
    if (th < 0 || sceKernelStartThread(th, 0, NULL) < 0)
        javacall_printf("j2v_render thread failed: 0x%08x\n", th);
}
