/*
 * J2Vita audio backend for VitaMidiPlayer / VitaSfxPlayer on top of SDL2_mixer.
 * MIDI goes through SDL_mixer's built-in Timidity (one music stream at a time,
 * like most handsets), WAV goes to mixer channels, AMR is accepted but silent.
 * Media times are in microseconds, as MIDP expects.
 */

#include <kni.h>
#include <commonKNIMacros.h>
#include <javacall_logging.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_RATE 44100
#define TIMIDITY_CFG "app0:/timidity/timidity.cfg"

#define KIND_MIDI 0
#define KIND_WAV  1
#define KIND_AMR  2

typedef struct {
    int kind;
    unsigned char *data;
    Mix_Music *music;
    Mix_Chunk *chunk;
    int channel;
    int level;
    Uint32 started;
    Uint32 played_ms;
} Sound;

static int audio_state; /* 0 untried, 1 ok, -1 failed */
static Sound *current_music;
static Sint16 *tone_pcm;
static Mix_Chunk *tone_chunk;

static int audio_init(void) {
    if (audio_state)
        return audio_state > 0;
    audio_state = -1;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        javacall_printf("[audio] SDL audio init failed: %s\n", SDL_GetError());
        return 0;
    }
    Mix_SetTimidityCfg(TIMIDITY_CFG);
    Mix_Init(MIX_INIT_MID);
    if (Mix_OpenAudio(AUDIO_RATE, AUDIO_S16SYS, 2, 1024) != 0) {
        javacall_printf("[audio] Mix_OpenAudio failed: %s\n", Mix_GetError());
        return 0;
    }
    Mix_AllocateChannels(16);
    Mix_ReserveChannels(1); /* channel 0 is the tone buzzer */
    audio_state = 1;
    return 1;
}

static Sound *from_handle(jint h) { return (Sound *)(intptr_t)h; }

static int music_active(Sound *s) {
    return s == current_music && Mix_PlayingMusic();
}

static int sound_playing(Sound *s) {
    if (!s || audio_state <= 0)
        return 0;
    if (s->kind == KIND_MIDI)
        return music_active(s);
    return s->channel >= 0 && Mix_Playing(s->channel) &&
           Mix_GetChunk(s->channel) == s->chunk;
}

static void apply_volume(Sound *s) {
    int v = s->level * MIX_MAX_VOLUME / 100;
    if (s->kind == KIND_MIDI) {
        if (s == current_music)
            Mix_VolumeMusic(v);
    } else if (s->chunk) {
        Mix_VolumeChunk(s->chunk, v);
    }
}

static void sound_stop(Sound *s) {
    if (!s || audio_state <= 0)
        return;
    if (sound_playing(s))
        s->played_ms += SDL_GetTicks() - s->started;
    if (s->kind == KIND_MIDI) {
        if (s == current_music) {
            Mix_HaltMusic();
            current_music = NULL;
        }
    } else if (s->channel >= 0) {
        if (Mix_GetChunk(s->channel) == s->chunk)
            Mix_HaltChannel(s->channel);
        s->channel = -1;
    }
}

/* J2ME loop count: -1 forever, n = play n times. SDL_mixer wants extra repeats. */
static int sdl_loops(jint n) {
    if (n < 0) return -1;
    return n > 1 ? (int)n - 1 : 0;
}

static int sound_start(Sound *s, jint loops) {
    if (!s || audio_state <= 0)
        return 0;
    if (s->kind == KIND_AMR)
        return 1;
    if (s->kind == KIND_MIDI) {
        if (!s->music)
            return 0;
        current_music = s;
        apply_volume(s);
        if (Mix_PlayMusic(s->music, sdl_loops(loops)) != 0) {
            current_music = NULL;
            return 0;
        }
    } else {
        apply_volume(s);
        s->channel = Mix_PlayChannel(-1, s->chunk, sdl_loops(loops));
        if (s->channel < 0)
            return 0;
    }
    s->started = SDL_GetTicks();
    return 1;
}

static jlong sound_time_us(Sound *s) {
    Uint32 ms;
    if (!s)
        return -1;
    ms = s->played_ms;
    if (sound_playing(s))
        ms += SDL_GetTicks() - s->started;
    return (jlong)ms * 1000;
}

static void sound_free(Sound *s) {
    if (!s)
        return;
    sound_stop(s);
    if (s->music) Mix_FreeMusic(s->music);
    if (s->chunk) Mix_FreeChunk(s->chunk);
    free(s->data);
    free(s);
}

static jint sound_create(int kind) {
    Sound *s = NULL;
    jint result = 0;
    jint len;

    KNI_StartHandles(1);
    KNI_DeclareHandle(data);
    KNI_GetParameterAsObject(1, data);

    if (KNI_IsNullHandle(data) || !audio_init())
        goto out;
    len = KNI_GetArrayLength(data);
    if (len <= 0)
        goto out;
    s = (Sound *)calloc(1, sizeof(Sound));
    if (!s)
        goto out;
    s->kind = kind;
    s->channel = -1;
    s->level = 100;
    if (kind != KIND_AMR) {
        SDL_RWops *rw;
        s->data = (unsigned char *)malloc((size_t)len);
        if (!s->data)
            goto out;
        memcpy(s->data, JavaByteArray(data), (size_t)len);
        rw = SDL_RWFromConstMem(s->data, (int)len);
        if (kind == KIND_MIDI)
            s->music = Mix_LoadMUSType_RW(rw, MUS_MID, 1);
        else
            s->chunk = Mix_LoadWAV_RW(rw, 1);
        if (!s->music && !s->chunk) {
            javacall_printf("[audio] decode failed: %s\n", Mix_GetError());
            goto out;
        }
    }
    result = (jint)(intptr_t)s;
    s = NULL;
out:
    if (s)
        sound_free(s);
    KNI_EndHandles();
    return result;
}

static jint set_volume(void) {
    Sound *s = from_handle(KNI_GetParameterAsInt(1));
    jint level = KNI_GetParameterAsInt(2);
    if (!s)
        return -1;
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    s->level = (int)level;
    if (audio_state > 0)
        apply_volume(s);
    return level;
}

/* only rewinds are reliable with Timidity; anything else just reports the clock */
static jlong set_time(void) {
    Sound *s = from_handle(KNI_GetParameterAsInt(1));
    jlong t = KNI_GetParameterAsLong(2);
    if (!s)
        return -1;
    if (t <= 0) {
        int was = sound_playing(s);
        if (s->kind == KIND_MIDI && was)
            Mix_RewindMusic();
        else if (was)
            sound_stop(s);
        s->played_ms = 0;
        s->started = SDL_GetTicks();
        return 0;
    }
    return sound_time_us(s);
}

KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_media_VitaMidiPlayer_nCreate(void) {
    KNI_ReturnInt(sound_create(KIND_MIDI));
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_javax_microedition_media_VitaMidiPlayer_nStart(void) {
    Sound *s = from_handle(KNI_GetParameterAsInt(1));
    KNI_ReturnBoolean(sound_start(s, KNI_GetParameterAsInt(2)) ? KNI_TRUE : KNI_FALSE);
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_javax_microedition_media_VitaMidiPlayer_nStop(void) {
    Sound *s = from_handle(KNI_GetParameterAsInt(1));
    sound_stop(s);
    KNI_ReturnBoolean(s ? KNI_TRUE : KNI_FALSE);
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_javax_microedition_media_VitaMidiPlayer_nIsPlaying(void) {
    KNI_ReturnBoolean(sound_playing(from_handle(KNI_GetParameterAsInt(1))) ? KNI_TRUE : KNI_FALSE);
}

KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_media_VitaMidiPlayer_nClose(void) {
    sound_free(from_handle(KNI_GetParameterAsInt(1)));
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_media_VitaMidiPlayer_nSetVolume(void) {
    KNI_ReturnInt(set_volume());
}

KNIEXPORT KNI_RETURNTYPE_LONG
Java_javax_microedition_media_VitaMidiPlayer_nGetMediaTime(void) {
    KNI_ReturnLong(sound_time_us(from_handle(KNI_GetParameterAsInt(1))));
}

KNIEXPORT KNI_RETURNTYPE_LONG
Java_javax_microedition_media_VitaMidiPlayer_nGetDuration(void) {
    Sound *s = from_handle(KNI_GetParameterAsInt(1));
    double d = (s && s->music) ? Mix_MusicDuration(s->music) : -1.0;
    KNI_ReturnLong(d > 0 ? (jlong)(d * 1000000.0) : (jlong)-1);
}

KNIEXPORT KNI_RETURNTYPE_LONG
Java_javax_microedition_media_VitaMidiPlayer_nSetMediaTime(void) {
    KNI_ReturnLong(set_time());
}

KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_media_VitaSfxPlayer_nCreate(void) {
    KNI_ReturnInt(sound_create(KNI_GetParameterAsInt(2) == KIND_AMR ? KIND_AMR : KIND_WAV));
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_javax_microedition_media_VitaSfxPlayer_nStart(void) {
    Sound *s = from_handle(KNI_GetParameterAsInt(1));
    KNI_ReturnBoolean(sound_start(s, KNI_GetParameterAsInt(2)) ? KNI_TRUE : KNI_FALSE);
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_javax_microedition_media_VitaSfxPlayer_nStop(void) {
    Sound *s = from_handle(KNI_GetParameterAsInt(1));
    sound_stop(s);
    KNI_ReturnBoolean(s ? KNI_TRUE : KNI_FALSE);
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_javax_microedition_media_VitaSfxPlayer_nIsPlaying(void) {
    KNI_ReturnBoolean(sound_playing(from_handle(KNI_GetParameterAsInt(1))) ? KNI_TRUE : KNI_FALSE);
}

KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_media_VitaSfxPlayer_nClose(void) {
    sound_free(from_handle(KNI_GetParameterAsInt(1)));
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_media_VitaSfxPlayer_nSetVolume(void) {
    KNI_ReturnInt(set_volume());
}

KNIEXPORT KNI_RETURNTYPE_LONG
Java_javax_microedition_media_VitaSfxPlayer_nGetMediaTime(void) {
    KNI_ReturnLong(sound_time_us(from_handle(KNI_GetParameterAsInt(1))));
}

KNIEXPORT KNI_RETURNTYPE_LONG
Java_javax_microedition_media_VitaSfxPlayer_nGetDuration(void) {
    Sound *s = from_handle(KNI_GetParameterAsInt(1));
    jlong us = -1;
    if (s && s->chunk) {
        /* chunk is 16-bit stereo at AUDIO_RATE after Mix_LoadWAV conversion */
        us = (jlong)s->chunk->alen * 1000000 / (AUDIO_RATE * 4);
    }
    KNI_ReturnLong(us);
}

KNIEXPORT KNI_RETURNTYPE_LONG
Java_javax_microedition_media_VitaSfxPlayer_nSetMediaTime(void) {
    KNI_ReturnLong(set_time());
}

KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_javax_microedition_media_VitaSfxPlayer_nPlayTone(void) {
    jint note = KNI_GetParameterAsInt(1);
    jint duration = KNI_GetParameterAsInt(2);
    jint volume = KNI_GetParameterAsInt(3);
    Uint32 frames, i;
    double step;
    Sint16 amp;

    if (note < 0 || note > 127 || duration <= 0 || !audio_init())
        KNI_ReturnBoolean(KNI_FALSE);
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (duration > 5000) duration = 5000;

    /* one tone at a time, like the handset buzzer */
    if (tone_chunk) {
        Mix_HaltChannel(0);
        Mix_FreeChunk(tone_chunk);
        tone_chunk = NULL;
    }
    free(tone_pcm);

    frames = (Uint32)duration * AUDIO_RATE / 1000;
    tone_pcm = (Sint16 *)malloc(frames * 4);
    if (!tone_pcm)
        KNI_ReturnBoolean(KNI_FALSE);
    step = 440.0 * pow(2.0, (note - 69) / 12.0) / AUDIO_RATE;
    amp = (Sint16)(volume * 80);
    for (i = 0; i < frames; i++) {
        double ph = fmod(i * step, 1.0);
        Sint16 v = ph < 0.5 ? amp : (Sint16)-amp;
        /* short fade in/out so it doesn't click */
        Uint32 edge = i < 256 ? i : (frames - i < 256 ? frames - i : 256);
        v = (Sint16)(v * (int)edge / 256);
        tone_pcm[i * 2] = v;
        tone_pcm[i * 2 + 1] = v;
    }
    tone_chunk = Mix_QuickLoad_RAW((Uint8 *)tone_pcm, frames * 4);
    if (!tone_chunk || Mix_PlayChannel(0, tone_chunk, 0) < 0)
        KNI_ReturnBoolean(KNI_FALSE);
    KNI_ReturnBoolean(KNI_TRUE);
}
