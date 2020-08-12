#include <string.h>
#include "internal.h"
#include "avio.h"
#include "avformat.h"

#define DSS_SP_AUDIO_HEADER_ARRAY_SIZE 42
#define DSS_G723_1_AUDIO_HEADER_ARRAY_SIZE 12
#define DSS_BLOCK_SIZE 512
#define DSS_ACODEC_DSS_SP 0x0
#define DSS_ACODEC_G723_1 0x2

typedef struct DssMuxContext {
    uint16_t chunk_bytes_written;
    uint8_t audio_header_index;
    uint8_t codec;
    uint8_t version;
	uint8_t *author;
	uint8_t *creation_ts;
	uint8_t *lastmodified_ts;
	uint8_t *runtime;
	uint8_t *comment;
	uint32_t runtime_ms; // at offset 512 for version 2, but offset 530 for version 3!
} DssMuxContext;

static const AVCodecTag codec_dss_tags[] = {
    { AV_CODEC_ID_DSS_SP,                 1 },
    { AV_CODEC_ID_G723_1,                 2 },
    { AV_CODEC_ID_NONE,                   0 }
};

static const uint8_t dss_sp_audio_headers[DSS_SP_AUDIO_HEADER_ARRAY_SIZE][6] = {
    {  15,   3,  13, 255,   0, 255 },
    { 143,  16,  12, 255,   0, 255 },
    { 143,   9,  13, 255,   0, 255 },
    {  15,  23,  12, 255,   0, 255 },
    {  15,  16,  12, 255,   0, 255 },
    {  15,   9,  13, 255,   0, 255 },
    { 143,  22,  12, 255,   0, 255 },
    { 143,  15,  12, 255,   0, 255 },
    { 143,   8,  13, 255,   0, 255 },
    {  15,  22,  12, 255,   0, 255 },
    {  15,  15,  12, 255,   0, 255 },
    {  15,   8,  13, 255,   0, 255 },
    { 143,  21,  12, 255,   0, 255 },
    { 143,  14,  12, 255,   0, 255 },
    { 143,   7,  13, 255,   0, 255 },
    {  15,  21,  12, 255,   0, 255 },
    {  15,  14,  12, 255,   0, 255 },
    {  15,   7,  13, 255,   0, 255 },
    { 143,  20,  12, 255,   0, 255 },
    { 143,  13,  12, 255,   0, 255 },
    { 143,   6,  13, 255,   0, 255 },
    {  15,  20,  12, 255,   0, 255 },
    {  15,  13,  12, 255,   0, 255 },
    {  15,   6,  13, 255,   0, 255 },
    { 143,  19,  12, 255,   0, 255 },
    { 143,  12,  12, 255,   0, 255 },
    { 143,   5,  13, 255,   0, 255 },
    {  15,  19,  12, 255,   0, 255 },
    {  15,  12,  12, 255,   0, 255 },
    {  15,   5,  13, 255,   0, 255 },
    { 143,  18,  12, 255,   0, 255 },
    { 143,  11,  12, 255,   0, 255 },
    { 143,   4,  13, 255,   0, 255 },
    {  15,  18,  12, 255,   0, 255 },
    {  15,  11,  12, 255,   0, 255 },
    {  15,   4,  13, 255,   0, 255 },
    { 143,  17,  12, 255,   0, 255 },
    { 143,  10,  12, 255,   0, 255 },
    { 143,   3,  13, 255,   0, 255 },
    {  15,  17,  12, 255,   0, 255 },
    {  15,  10,  12, 255,   0, 255 }
};

static const uint8_t dss_g723_1_audio_headers[DSS_G723_1_AUDIO_HEADER_ARRAY_SIZE][6] = {
    {  15,   3,  22, 255,   2, 255 },
    {  15,  14,  21, 255,   2, 255 },
    {  15,  13,  21, 255,   2, 255 },
    {  15,  12,  21, 255,   2, 255 },
    {  15,  11,  21, 255,   2, 255 },
    {  15,  10,  21, 255,   2, 255 },
    {  15,   9,  21, 255,   2, 255 },
    {  15,   8,  21, 255,   2, 255 },
    {  15,   7,  21, 255,   2, 255 },
    {  15,   6,  21, 255,   2, 255 },
    {  15,   5,  21, 255,   2, 255 },
    {  15,   4,  21, 255,   2, 255 }
};

static int dss_query_codec(enum AVCodecID id, int std_compliance) {
    // this is never getting called anyway... what is it good for?
    if(id == AV_CODEC_ID_DSS_SP || id == AV_CODEC_ID_G723_1) {
        return 1;
    }
    return 0;
}

static int dss_init(AVFormatContext *s) {
    DssMuxContext *dss = s->priv_data;
    if(s->streams) {
        if(s->streams[0]) {
            if(s->streams[0]->codecpar) {
                if(s->streams[0]->codecpar->codec_id) {
                    if(s->streams[0]->codecpar->codec_id == AV_CODEC_ID_DSS_SP) {
                        dss->codec = DSS_ACODEC_DSS_SP;
                    } else if (s->streams[0]->codecpar->codec_id == AV_CODEC_ID_G723_1) {
                        dss->codec = DSS_ACODEC_G723_1;
                    } else {
                        return AVERROR(EINVAL);
                    }
                }
            }
        }
    }

    //init values!
    dss->author = "";
    dss->creation_ts = "";
    dss->lastmodified_ts = "";
    dss->runtime = "";
    dss->comment = "";
    dss->runtime_ms = 0;

    return 0;
}

static void write_next_audio_header(AVFormatContext *s) {
    DssMuxContext *dss = s->priv_data;
    AVIOContext *pb = s->pb;

    if(dss->codec == DSS_ACODEC_DSS_SP) {
        for(int i = 0; i < 6; i++) {
            avio_w8(pb, dss_sp_audio_headers[dss->audio_header_index][i]);
        }
        dss->audio_header_index = (dss->audio_header_index + 1) % DSS_SP_AUDIO_HEADER_ARRAY_SIZE;
    } else if (dss->codec == DSS_ACODEC_G723_1) {
        for(int i = 0; i < 6; i++) {
            avio_w8(pb, dss_g723_1_audio_headers[dss->audio_header_index][i]);
        }
        dss->audio_header_index = (dss->audio_header_index + 1) % DSS_G723_1_AUDIO_HEADER_ARRAY_SIZE;
    }
}

static int dss_write_header(AVFormatContext *s) {
    /* we do not really care about author, creation date or other stuff...
     * all we care about is a working output file. so we only write
     * the bare necessities
     */

    AVIOContext *pb = s->pb;
    DssMuxContext *dss = s->priv_data;

    dss->chunk_bytes_written = 0;

    dss->version = 2; // set to version 2 for simple, fairly well known header format

    avio_w8(pb, dss->version);
    dss->chunk_bytes_written += 1;
    avio_write(pb, "dss", 3);
    dss->chunk_bytes_written += 3;
    avio_write(pb, "\0\0\0\0\0\0\0\0", 8); // no clue what belongs here
    dss->chunk_bytes_written += 8;
    avio_write(pb, dss->author, 16); // author/hardware tag
    dss->chunk_bytes_written += 16;
    avio_wl32(pb, 0);
    dss->chunk_bytes_written += 4;
    avio_write(pb, "\0\0\0\0\0\0", 6); // it's FE FF FE FF F7 FF in the sample files, but i've seen .. FB FF (version 8) too. zeros seem to be fine as well.
    dss->chunk_bytes_written += 6;
    avio_write(pb, dss->creation_ts, 12); // creation date
    avio_write(pb, dss->lastmodified_ts, 12); // last modified
    dss->chunk_bytes_written += 24;
    avio_write(pb, dss->runtime, 6); // runtime as a string
    dss->chunk_bytes_written += 6;
    for(; dss->chunk_bytes_written < DSS_BLOCK_SIZE * dss->version; dss->chunk_bytes_written++) {
        if(dss->chunk_bytes_written < DSS_BLOCK_SIZE) {
            avio_w8(pb, 255);
        } else {
            if(dss->chunk_bytes_written == 675) {
                avio_w8(pb, dss->codec);
            } else if (dss->chunk_bytes_written < 675) {
                avio_w8(pb, 0);
            } else {
                avio_w8(pb, 255);
            }
        }
    }

    avio_flush(pb);
    if(dss->chunk_bytes_written != avio_size(pb)) {
        return AVERROR(EIO); // what kind of error?
    }

    dss->chunk_bytes_written = 0;

    return 0;
}

static int dss_write_packet(AVFormatContext *s, AVPacket *pkt) {
    DssMuxContext *dss = s->priv_data;
    AVIOContext *pb = s->pb;

    if(dss->chunk_bytes_written == 0) {
        write_next_audio_header(s);
        dss->chunk_bytes_written += 6;
    }

    if(dss->chunk_bytes_written + pkt->size > DSS_BLOCK_SIZE)     {
        avio_write(pb, pkt->data, DSS_BLOCK_SIZE - dss->chunk_bytes_written);
        write_next_audio_header(s);
        avio_write(pb, (pkt->data + (DSS_BLOCK_SIZE - dss->chunk_bytes_written)), pkt->size - (DSS_BLOCK_SIZE - dss->chunk_bytes_written));
        dss->chunk_bytes_written = 6 + pkt->size - (DSS_BLOCK_SIZE - dss->chunk_bytes_written);
    } else {
        avio_write(pb, pkt->data, pkt->size);
        dss->chunk_bytes_written += pkt->size;
    }

    return 0;
}

static int dss_write_trailer(AVFormatContext *s) {
    AVIOContext *pb = s->pb;
    DssMuxContext *dss = s->priv_data;

    if(dss->chunk_bytes_written != 0) {
        for(; dss->chunk_bytes_written < DSS_BLOCK_SIZE; dss->chunk_bytes_written++) {
            if(dss->codec == DSS_ACODEC_DSS_SP) {
                avio_w8(pb, 0);
            } else {
                avio_w8(pb, 255);
            }
        }
    }

    return 0;
}

AVOutputFormat ff_dss_muxer = {
    .name = "dss",
    .long_name = NULL_IF_CONFIG_SMALL("Digital Speech Standard"),
    .extensions = "dss",
    .priv_data_size = sizeof(DssMuxContext),
    .audio_codec = AV_CODEC_ID_DSS_SP,
    .init = dss_init,
    .write_header = dss_write_header,
    .write_packet = dss_write_packet,
    .write_trailer = dss_write_trailer,
    .query_codec = dss_query_codec,
    .codec_tag = (const AVCodecTag* const []) { codec_dss_tags, 0}
};
