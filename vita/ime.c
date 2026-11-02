#include <psp2/ime_dialog.h>
#include <psp2/common_dialog.h>
#include <string.h>

#include "javacall_logging.h"
#include "j2vita.h"

#define MAX_TEXT 64

static volatile int requested, running;
static SceWChar16 title[MAX_TEXT + 1] = {'T', 'y', 'p', 'e', ' ', 't', 'e', 'x', 't', 0};
static SceWChar16 text[MAX_TEXT + 1];
static SceWChar16 initial[MAX_TEXT + 1];
/* ime_ask: the text is handed back whole instead of typed into the game */
static int ask_mode;
static volatile int ask_state; /* IME_ASK_* */
static SceWChar16 answer[MAX_TEXT + 1];

/* typed characters waiting to be sent, filled by the render thread, drained by input */
static int queue[MAX_TEXT];
static volatile int q_head, q_tail;

void ime_open(void) {
    static const SceWChar16 def[] = {'T', 'y', 'p', 'e', ' ', 't', 'e', 'x', 't', 0};
    if (running || requested)
        return;
    ask_mode = 0;
    memcpy(title, def, sizeof(def));
    initial[0] = 0;
    requested = 1;
    video_redraw();
}

void ime_ask(const SceWChar16 *t, const SceWChar16 *init) {
    int i;
    if (running || requested)
        return;
    for (i = 0; i < MAX_TEXT && t[i]; i++) title[i] = t[i];
    title[i] = 0;
    for (i = 0; i < MAX_TEXT && init[i]; i++) initial[i] = init[i];
    initial[i] = 0;
    ask_mode = 1;
    ask_state = IME_ASK_BUSY;
    requested = 1;
    video_redraw();
}

int ime_ask_state(void) { return ask_state; }
const SceWChar16 *ime_answer(void) { return answer; }

int ime_busy(void) {
    return requested || running;
}

static void start(void) {
    SceImeDialogParam p;
    sceImeDialogParamInit(&p);
    p.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH;
    p.languagesForced = SCE_FALSE;
    p.type = SCE_IME_TYPE_DEFAULT;
    /* phone games tend to only know lower case letters (Sims 3 name entry) */
    p.option = ask_mode ? 0 : SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
    p.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
    p.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
    p.title = title;
    p.maxTextLength = MAX_TEXT;
    p.initialText = initial;
    p.inputTextBuffer = text;
    memset(text, 0, sizeof(text));
    running = sceImeDialogInit(&p) >= 0;
    if (!running) {
        javacall_printf("ime: sceImeDialogInit failed\n");
        if (ask_mode)
            ask_state = IME_ASK_CANCEL;
    }
}

static void finish(void) {
    SceImeDialogResult r;
    int i;
    memset(&r, 0, sizeof(r));
    sceImeDialogGetResult(&r);
    sceImeDialogTerm();
    if (ask_mode) {
        memcpy(answer, text, sizeof(answer));
        ask_state = r.button == SCE_IME_DIALOG_BUTTON_ENTER ? IME_ASK_OK : IME_ASK_CANCEL;
    } else if (r.button == SCE_IME_DIALOG_BUTTON_ENTER) {
        for (i = 0; i < MAX_TEXT && text[i]; i++) {
            int next = (q_tail + 1) % MAX_TEXT;
            if (next == q_head)
                break;
            queue[q_tail] = text[i];
            q_tail = next;
        }
    }
    running = 0;
}

/* render thread, every frame: 1 while the dialog is up and needs vglSwapBuffers(GL_TRUE) */
int ime_poll(void) {
    if (requested) {
        requested = 0;
        start();
    }
    if (!running)
        return 0;
    if (sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_FINISHED) {
        finish();
        return 0;
    }
    return 1;
}

/* input thread, every tick: next character to press, 0 when the queue is empty */
int ime_next_char(void) {
    int c;
    if (q_head == q_tail)
        return 0;
    c = queue[q_head];
    q_head = (q_head + 1) % MAX_TEXT;
    return c;
}
