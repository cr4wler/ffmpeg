#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "avcodec.h"

#define SUBFRAMES 4

typedef struct DssSpEncContext {
    AVCodecContext *avctx;
    int32_t working_buffer[SUBFRAMES][72];
    // what else will we need?
} DssSpEncContext;

static av_cold int dss_sp_encode_init(AVCodecContext *avctx)
{
    DssSpEncContext *p = avctx->priv_data;
    avctx->channel_layout = AV_CH_LAYOUT_MONO;
    avctx->channels = 1;
    avctx->frame_size = 42; // what do we set this to? prolly 42 if its bytes?
    avctx->initial_padding = 0; // file header needs to be VERSION (first byte) * 512 bytes long, but is this file header?! prolly not... audio header is 6 bytes every 512 bytes
    avctx->bit_rate = 13700; // is this accurate (enough)?
    avctx->sample_rate = 11025;
    avctx->sample_fmt = AV_SAMPLE_FMT_S16;
    // what else?

    return 0;
}

static int dss_sp_encode_frame(AVCodecContext *avctx, AVPacket *avpkt, const AVFrame *frame, int *got_packet_ptr)
{
    // do stuff
    


    return 0;
}

static av_cold int dss_sp_encode_end(AVCodecContext *avctx)
{
    // do stuff? prolly not... maybe clean up.
    
    return 0;
}

AVCodec ff_dss_sp_encoder = {
    .name           = "dss_sp",
    .long_name      = NULL_IF_CONFIG_SMALL("Digital Speech Standard (Standard Play)"),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_DSS_SP,
    .priv_data_size = sizeof(DssSpEncContext),
    .init           = dss_sp_encode_init,
    .encode2        = dss_sp_encode_frame,
    .close          = dss_sp_encode_end,
//    .supported_samplerates = { 11025 },
//    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE | FF_CODEC_CAP_INIT_CLEANUP,
    .capabilities   = AV_CODEC_CAP_DR1,
//    .sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_S16 }
//    .priv_class     = &dss_sp_enc_class,
};
