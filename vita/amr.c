#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>

/* AMR-NB / AMR-WB (storage format with #! header) to 16 bit stereo at out_rate.
 * Returns a malloc'd buffer and its frame count, NULL when it cannot decode. */
short *amr_to_pcm(const unsigned char *d, int len, int out_rate, int *out_frames) {
    static const unsigned char nb[16] = {13, 14, 16, 18, 20, 21, 27, 32, 6, 1, 1, 1, 1, 1, 1, 1};
    static const unsigned char wb[16] = {18, 24, 33, 37, 41, 47, 51, 59, 61, 6, 1, 1, 1, 1, 1, 1};
    const unsigned char *sz;
    const AVCodec *codec;
    AVCodecContext *ctx = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    float *mono = NULL;
    short *out = NULL;
    int p, rate, n = 0, cap = 0, i;

    if (len >= 9 && !memcmp(d, "#!AMR-WB\n", 9)) {
        sz = wb; p = 9; rate = 16000;
        codec = avcodec_find_decoder(AV_CODEC_ID_AMR_WB);
    } else if (len >= 6 && !memcmp(d, "#!AMR\n", 6)) {
        sz = nb; p = 6; rate = 8000;
        codec = avcodec_find_decoder(AV_CODEC_ID_AMR_NB);
    } else {
        return NULL;
    }
    if (!codec || !(ctx = avcodec_alloc_context3(codec)))
        return NULL;
    ctx->sample_rate = rate;
    av_channel_layout_default(&ctx->ch_layout, 1);
    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    if (!pkt || !frame || avcodec_open2(ctx, codec, NULL) < 0)
        goto done;

    while (p < len) {
        int fs = sz[(d[p] >> 3) & 15];
        if (p + fs > len) break;
        pkt->data = (uint8_t *)d + p;
        pkt->size = fs;
        p += fs;
        if (avcodec_send_packet(ctx, pkt) < 0) continue;
        while (avcodec_receive_frame(ctx, frame) == 0) {
            int ch = frame->ch_layout.nb_channels > 0 ? frame->ch_layout.nb_channels : 1, k;
            int planar = av_sample_fmt_is_planar(frame->format);
            if (frame->format != AV_SAMPLE_FMT_FLT && frame->format != AV_SAMPLE_FMT_FLTP) continue;
            if (n + frame->nb_samples > cap) {
                float *m;
                cap = (n + frame->nb_samples) * 2;
                m = realloc(mono, cap * sizeof(float));
                if (!m) goto done;
                mono = m;
            }
            for (i = 0; i < frame->nb_samples; i++) {
                float v = 0;
                for (k = 0; k < ch; k++)
                    v += planar ? ((float *)frame->extended_data[k])[i] : ((float *)frame->data[0])[i * ch + k];
                mono[n + i] = v / ch;
            }
            n += frame->nb_samples;
        }
    }
    if (n > 0) {
        /* linear resample to the mixer rate, duplicated to both channels */
        int frames = (int)((long long)n * out_rate / rate);
        out = malloc((size_t)frames * 2 * sizeof(short));
        if (out) {
            for (i = 0; i < frames; i++) {
                double pos = (double)i * rate / out_rate;
                int a = (int)pos, b = a + 1 < n ? a + 1 : a;
                float f = (float)(pos - a), v = mono[a] + (mono[b] - mono[a]) * f;
                int s = (int)(v * 32767.0f);
                s = s > 32767 ? 32767 : s < -32768 ? -32768 : s;
                out[i * 2] = out[i * 2 + 1] = (short)s;
            }
            *out_frames = frames;
        }
    }
done:
    free(mono);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    return out;
}
