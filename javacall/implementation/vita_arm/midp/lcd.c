#include "javacall_lcd.h"
#include <stdint.h>
#include <string.h>

/* implemented by the frontend in vita/video.c */
void video_set_game_size(int w, int h);
void video_submit(const uint16_t *src, int y0, int y1, int src_is_rows);
void video_wait_unpaused(void);

#define MAX_W 640
#define MAX_H 640

static int scr_w = 480, scr_h = 272;
static int enable_flush = 1;
static javacall_pixel screenbuf[MAX_W * MAX_H] __attribute__((aligned(64)));

javacall_result javacall_lcd_init(void) {
    memset(screenbuf, 0, sizeof(screenbuf));
    video_set_game_size(scr_w, scr_h);
    return JAVACALL_OK;
}

javacall_result javacall_lcd_finalize(void) {
    return JAVACALL_OK;
}

javacall_pixel* javacall_lcd_get_screen(javacall_lcd_screen_type screenType,
                                        int* screenWidth, int* screenHeight,
                                        javacall_lcd_color_encoding_type* colorEncoding) {
    (void)screenType;
    if (screenWidth) *screenWidth = scr_w;
    if (screenHeight) *screenHeight = scr_h;
    if (colorEncoding) *colorEncoding = JAVACALL_LCD_COLOR_RGB565;
    return screenbuf;
}

static javacall_result flush(const javacall_pixel *buf, int y0, int y1, int rows) {
    if (enable_flush)
        video_submit((const uint16_t *)buf, y0, y1, rows);
    video_wait_unpaused();
    return JAVACALL_OK;
}

javacall_result javacall_lcd_flush(void) {
    return flush(screenbuf, 0, scr_h, 0);
}

javacall_result javacall_lcd_flush_partial(int ystart, int yend) {
    return flush(screenbuf, ystart, yend, 0);
}

/* buf may only hold h rows, never read past that */
javacall_bool javacall_lcd_direct_flush(javacall_pixel* buf, int h) {
    if (!buf)
        return JAVACALL_FALSE;
    if (h > scr_h)
        h = scr_h;
    if (h > 0)
        flush(buf, 0, h, 1);
    return JAVACALL_TRUE;
}

javacall_result javacall_lcd_set_full_screen_mode(javacall_bool useFullScreen) {
    (void)useFullScreen;
    return JAVACALL_OK;
}

javacall_bool javacall_lcd_reverse_orientation(void) { return JAVACALL_FALSE; }
javacall_bool javacall_lcd_get_reverse_orientation(void) { return JAVACALL_FALSE; }
int javacall_lcd_get_screen_width(void) { return scr_w; }
int javacall_lcd_get_screen_height(void) { return scr_h; }

void javacall_set_new_screen_size(int w, int h) {
    if (w <= 0 || h <= 0 || w > MAX_W || h > MAX_H || (w == scr_w && h == scr_h))
        return;
    scr_w = w;
    scr_h = h;
    memset(screenbuf, 0, sizeof(screenbuf));
    video_set_game_size(w, h);
    javanotify_rotation();
}

void javacall_lcd_enable_flush(int enable) { enable_flush = enable; }

void javacall_lcd_show_splash(void) {
    memset(screenbuf, 0, sizeof(screenbuf));
    javacall_lcd_flush();
}
