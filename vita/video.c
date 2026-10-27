#include <string.h>
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
/* performance OSD, drawn 1:1 in the top right corner */
#define OSD_W 112
#define OSD_H 72
static uint32_t osd_px[OSD_W * OSD_H];
static int osd_w, osd_h;

static int game_w = 480, game_h = 272;
static volatile int frame_dirty, size_dirty, fps_shown, frames_in, overlay_dirty;
static SceKernelLwMutexWork lock;
static SceUID wake_sema, resume_sema;
static volatile int waiting_resume;

typedef struct { int x, y, w, h, rot; } Layout;

static void layout(Layout *l) {
    int vw, vh;
    l->rot = j2v_in_game && j2v_cfg.rotate;
    vw = l->rot ? game_h : game_w;
    vh = l->rot ? game_w : game_h;
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
}

int video_touch_to_game(int tx, int ty, int *gx, int *gy) {
    Layout l;
    int sx = tx * SCREEN_W / 1920, sy = ty * SCREEN_H / 1088, dx, dy, x, y;
    layout(&l);
    if (sx < l.x || sy < l.y || sx >= l.x + l.w || sy >= l.y + l.h)
        return 0;
    dx = sx - l.x;
    dy = sy - l.y;
    if (l.rot) {
        x = dy * game_w / l.h;
        y = game_h - 1 - dx * game_h / l.w;
    } else {
        x = dx * game_w / l.w;
        y = dy * game_h / l.h;
    }
    *gx = x < 0 ? 0 : x >= game_w ? game_w - 1 : x;
    *gy = y < 0 ? 0 : y >= game_h ? game_h - 1 : y;
    return 1;
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

static void quad(const Layout *l, int rot) {
    float x0 = (float)l->x, y0 = (float)l->y;
    float x1 = (float)(l->x + l->w), y1 = (float)(l->y + l->h);
    glBegin(GL_QUADS);
    if (rot) {
        glTexCoord2f(0, 1); glVertex3f(x0, y0, 0);
        glTexCoord2f(0, 0); glVertex3f(x1, y0, 0);
        glTexCoord2f(1, 0); glVertex3f(x1, y1, 0);
        glTexCoord2f(1, 1); glVertex3f(x0, y1, 0);
    } else {
        glTexCoord2f(0, 0); glVertex3f(x0, y0, 0);
        glTexCoord2f(1, 0); glVertex3f(x1, y0, 0);
        glTexCoord2f(1, 1); glVertex3f(x1, y1, 0);
        glTexCoord2f(0, 1); glVertex3f(x0, y1, 0);
    }
    glEnd();
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
 * A game that leaves the VM idle part of the time is running at its own pace. */
static void build_osd(int fps, int load) {
    char buf[24];
    int lines = j2v_cfg.show_fps == OSD_FULL ? 4 : 1, i;
    uint32_t speed = load >= 97 ? 0xFF6060FF : load >= 85 ? 0xFF40D0FF : 0xFF70E070;
    osd_w = lines == 1 ? 64 : OSD_W;
    osd_h = lines * 16 + 6;
    for (i = 0; i < OSD_W * OSD_H; i++)
        osd_px[i] = 0;
    for (i = 0; i < osd_h * OSD_W; i++)
        if (i % OSD_W < osd_w)
            osd_px[i] = 0xB0000000;
    snprintf(buf, sizeof(buf), "%3d FPS", fps > 999 ? 999 : fps);
    text_draw(osd_px, OSD_W, OSD_H, 4, 3, buf, 0xFFFFFFFF);
    if (lines == 1)
        return;
    snprintf(buf, sizeof(buf), "Load %3d%%", load);
    text_draw(osd_px, OSD_W, OSD_H, 4, 19, buf, speed);
    text_draw(osd_px, OSD_W, OSD_H, 4, 35, load >= 97 ? "CPU bound" : "Full speed", speed);
    snprintf(buf, sizeof(buf), "%dx%d %d", game_w, game_h, scePowerGetArmClockFrequency());
    text_draw(osd_px, OSD_W, OSD_H, 4, 51, buf, 0xFFC0C0C0);
}

static int render_thread(SceSize args, void *argp) {
    GLuint tex_game, tex_ovl, tex_fps;
    int tex_w = 0, tex_h = 0, have_frame = 0, kp_shown = 0;
    SceUInt64 fps_t0 = sceKernelGetProcessTimeWide(), idle_t0 = j2v_vm_idle_us;
    (void)args; (void)argp;

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
    glBindTexture(GL_TEXTURE_2D, tex_ovl);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, OVL_W, OVL_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, overlay);
    set_filter(0);
    glBindTexture(GL_TEXTURE_2D, tex_fps);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, OSD_W, OSD_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, osd_px);
    set_filter(0);

    for (;;) {
        Layout l;
        int menu, keypad, n;
        SceUInt t = 50000;
        SceUInt64 now;

        sceKernelWaitSema(wake_sema, 1, &t);
        menu = menu_is_open();
        keypad = !menu && keypad_is_open();

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
            if (j2v_in_game && j2v_cfg.show_fps) {
                build_osd(n, load);
                glBindTexture(GL_TEXTURE_2D, tex_fps);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, OSD_W, OSD_H, GL_RGBA, GL_UNSIGNED_BYTE, osd_px);
            }
            fps_shown = 1;
        } else if (!frame_dirty && !size_dirty && !menu && !fps_shown && !(keypad && overlay_dirty) &&
                   kp_shown == keypad) {
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
            set_filter(j2v_in_game && j2v_cfg.smooth);
            quad(&l, l.rot);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (menu || keypad) {
            Layout full = {0, 0, SCREEN_W, SCREEN_H, 0};
            /* the keypad overlay only changes when the player touches it */
            if (menu || overlay_dirty || !kp_shown) {
                overlay_dirty = 0;
                if (menu)
                    menu_draw(overlay, OVL_W, OVL_H);
                else
                    keypad_draw(overlay, OVL_W, OVL_H);
                glBindTexture(GL_TEXTURE_2D, tex_ovl);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, OVL_W, OVL_H, GL_RGBA, GL_UNSIGNED_BYTE, overlay);
            }
            glBindTexture(GL_TEXTURE_2D, tex_ovl);
            quad(&full, 0);
        }
        kp_shown = keypad;
        if (!menu && j2v_in_game && j2v_cfg.show_fps && osd_w) {
            Layout f = {SCREEN_W - osd_w - 4, 4, OSD_W, OSD_H, 0};
            glBindTexture(GL_TEXTURE_2D, tex_fps);
            quad(&f, 0);
        }
        vglSwapBuffers(GL_FALSE);
    }
    return 0;
}

void video_start(void) {
    SceUID th;
    javacall_printf("video: vglInit\n");
    vglInit(0x100000);
    vglWaitVblankStart(GL_TRUE);
    javacall_printf("video: ready\n");
    sceKernelCreateLwMutex(&lock, "j2v_video", 0, 0, NULL);
    wake_sema = sceKernelCreateSema("j2v_wake", 0, 0, 1, NULL);
    resume_sema = sceKernelCreateSema("j2v_resume", 0, 0, 1, NULL);
    th = sceKernelCreateThread("j2v_render", render_thread, 0x10000100, 0x10000, 0, 0, NULL);
    if (th >= 0)
        sceKernelStartThread(th, 0, NULL);
}
