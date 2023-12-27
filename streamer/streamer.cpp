#include "streamer.hpp"

#include <string>
#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

namespace streamer
{

#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    printf("pts:%d dts:%d duration:%d stream_index:%d timebase:%d/%d\n",
           pkt->pts,
           pkt->dts,
           pkt->duration,
           pkt->stream_index, time_base->num, time_base->den);
}

static int encode_and_write_frame(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx, AVStream *st,AVFrame *frame,int64_t frame_duration)
{
    AVPacket pkt = {0};
    av_init_packet(&pkt);

    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending frame to codec context!\n");
        return ret;
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            break;
        }

        /*if(pkt.pts != AV_NOPTS_VALUE)
            av_packet_rescale_ts(&pkt, codec_ctx->time_base, st->time_base);
        else{*/
            pkt.pts = frame->pts;
            pkt.dts = pkt.pts;
            pkt.duration = frame_duration;
        //}
        pkt.stream_index = st->index;
        
        log_packet(fmt_ctx, &pkt);
        av_interleaved_write_frame(fmt_ctx, &pkt);        
        av_packet_unref(&pkt);
    }

    return 0;
}


static int set_options_and_open_encoder(AVFormatContext *fctx, AVStream *stream, AVCodecContext *codec_ctx, AVCodec *codec,
                                        std::string codec_profile, double width, double height,
                                        int fps, int bitrate, AVCodecID codec_id)
{
    const AVRational dst_fps = {fps, 1};

    codec_ctx->codec_tag = 0;
    codec_ctx->codec_id = codec_id;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->gop_size = 12;
    codec_ctx->pix_fmt = STREAM_PIX_FMT;
    codec_ctx->framerate = dst_fps;
    codec_ctx->time_base = av_inv_q(dst_fps);
    codec_ctx->bit_rate = bitrate;
    if (fctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    stream->time_base = codec_ctx->time_base; //will be set afterwards by avformat_write_header to 1/1000

    int ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Could not initialize stream codec parameters!\n");
        return 1;
    }

    AVDictionary *codec_options = nullptr;
    av_dict_set(&codec_options, "profile", codec_profile.c_str(), 0);
    av_dict_set(&codec_options, "preset", "fast", 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);

    // open video encoder
    ret = avcodec_open2(codec_ctx, codec, &codec_options);
    if (ret < 0)
    {
        fprintf(stderr, "Could not open video encoder!\n");
        return 1;
    }
    av_dict_free(&codec_options);
    return 0;
}

static int set_options_and_open_encoder_audio(AVFormatContext *fctx, AVStream *stream, AVCodecContext *codec_ctx, AVCodec *codec,
                                        int channel, int samplerate, AVCodecID codec_id)
{

    //codec_ctx->codec_tag = 0;
    codec_ctx->codec_id = codec_id;
    codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_ctx->sample_rate = samplerate;
    codec_ctx->channels = channel;
    codec_ctx->channel_layout = av_get_default_channel_layout(channel);
    codec_ctx->bit_rate  = 64000;
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP; //AV_SAMPLE_FMT_S16;
    codec_ctx->time_base = (AVRational){ 1, samplerate };
    if (fctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    stream->time_base = codec_ctx->time_base;

    int ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Could not initialize stream codec parameters!\n");
        return 1;
    }

    // open audio encoder
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Could not open audio encoder!\n");
        return 1;
    }
    return 0;
}


Streamer::Streamer()
{
    format_ctx = nullptr;
    out_codec = nullptr;
    out_stream = nullptr;
    out_codec_ctx = nullptr;
    out_codec_audio_ctx = nullptr;
    rtmp_server_conn = false;
    av_register_all();
    inv_stream_timebase = 30.0;
    fifo = NULL;
    network_init_ok = !avformat_network_init();
}


void Streamer::cleanup()
{
    if(out_codec_ctx) {
        avcodec_close(out_codec_ctx);
        avcodec_free_context(&out_codec_ctx);
    }
    if(out_codec_audio_ctx) {
        avcodec_close(out_codec_audio_ctx);
        avcodec_free_context(&out_codec_audio_ctx);
    }

    if(format_ctx) {
        if(format_ctx->pb) {
            avio_close(format_ctx->pb);
        }
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }
    if (fifo)
        av_audio_fifo_free(fifo);
}


Streamer::~Streamer()
{
    cleanup();
    avformat_network_deinit();
}



void Streamer::stream_frame(const uint8_t *data)
{
    if(can_stream()) {
        const int stride[] = {static_cast<int>(config.src_width*3)};
        sws_scale(scaler.ctx, &data, stride, 0, config.src_height, picture.frame->data, picture.frame->linesize);
        int64_t duration = av_rescale_q(1, out_codec_ctx->time_base, out_stream->time_base);
        picture.frame->pts += duration;
        encode_and_write_frame(out_codec_ctx, format_ctx,out_stream, picture.frame, duration);
    }
}


void Streamer::stream_frame(const uint8_t *data, int64_t frame_duration)
{
    if(can_stream()) {
        const int stride[] = {static_cast<int>(config.src_width*3)};
        sws_scale(scaler.ctx, &data, stride, 0, config.src_height, picture.frame->data, picture.frame->linesize);
        picture.frame->pts += frame_duration; //time of frame in milliseconds
        encode_and_write_frame(out_codec_ctx, format_ctx,out_stream, picture.frame, frame_duration);
    }
}

void Streamer::stream_frame_audio(const uint8_t *data)
{
    if(can_stream()) {
        //memcpy(audioframe.frame->data[0], data, 640*4);
        //memset(audioframe.frame->data[0], 0.3, audioframe.frame->nb_samples*4);
        /* Store the new samples in the FIFO buffer. */
        if (av_audio_fifo_write(fifo, (void **)&data,320) < 320) {  //20ms
            fprintf(stderr, "Could not write data to FIFO\n");
            return;
        }
        if (av_audio_fifo_size(fifo) >= audioframe.frame->nb_samples) {
            av_audio_fifo_read(fifo, (void **)audioframe.frame->data, audioframe.frame->nb_samples);
            int64_t duration = av_rescale_q(audioframe.frame->nb_samples, out_codec_audio_ctx->time_base, out_stream_audio->time_base);
            audioframe.frame->pts += duration;
            encode_and_write_frame(out_codec_audio_ctx, format_ctx, out_stream_audio, audioframe.frame, duration);
        }
    }
}

void Streamer::stream_frame_audio(const uint8_t *data, int64_t frame_duration)
{
    if(can_stream()) {
        memcpy(audioframe.frame->data[0], data, audioframe.frame->nb_samples*2);
        audioframe.frame->pts += frame_duration; //time of frame in milliseconds
        encode_and_write_frame(out_codec_audio_ctx, format_ctx,out_stream_audio, audioframe.frame,frame_duration);
    }
}


void Streamer::enable_av_debug_log()
{
    av_log_set_level(AV_LOG_DEBUG);
}


int Streamer::init(const StreamerConfig &streamer_config)
{
    init_ok = false;
    cleanup();

    config = streamer_config;

    if(!network_init_ok) {
        return 1;
    }

    //initialize format context for output with flv and no filename
    avformat_alloc_output_context2(&format_ctx, nullptr, "flv", nullptr);
    if(!format_ctx) {
        return 1;
    }

    //AVIOContext for accessing the resource indicated by url
    if (!(format_ctx->oformat->flags & AVFMT_NOFILE)) {
        int avopen_ret  = avio_open2(&format_ctx->pb, config.server.c_str(),
                                     AVIO_FLAG_WRITE, nullptr, nullptr);
        if (avopen_ret < 0)  {
            fprintf(stderr, "failed to open stream output context, stream will not work\n");
            return 1;
        }
        rtmp_server_conn = true;
    }

    //use selected codec
    //---------video codec---------------------------------------
    AVCodecID codec_id = AV_CODEC_ID_H264;
    out_codec = avcodec_find_encoder(codec_id);
    if (!(out_codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        return 1;
    }

    out_stream = avformat_new_stream(format_ctx, out_codec);
    if (!out_stream) {
        fprintf(stderr, "Could not allocate stream\n");
        return 1;
    }

    out_codec_ctx = avcodec_alloc_context3(out_codec);

    if(set_options_and_open_encoder(format_ctx, out_stream, out_codec_ctx, out_codec, config.profile,
                                    config.dst_width, config.dst_height, config.fps, config.bitrate, codec_id)) {
        return 1;
    }

    out_stream->codecpar->extradata_size = out_codec_ctx->extradata_size;
    out_stream->codecpar->extradata = static_cast<uint8_t*>(av_mallocz(out_codec_ctx->extradata_size));
    memcpy(out_stream->codecpar->extradata, out_codec_ctx->extradata, out_codec_ctx->extradata_size);
    printf("videocodec capability=%x,extradata_size=%d\n",out_codec->capabilities,out_codec_ctx->extradata_size);

    //---------audio codec---------------------------------------
    codec_id = AV_CODEC_ID_AAC;
    AVCodec *out_codec_audio = avcodec_find_encoder(codec_id);
    if (!(out_codec_audio)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        return 1;
    }

    out_stream_audio = avformat_new_stream(format_ctx, out_codec_audio);
    if (!out_stream_audio) {
        fprintf(stderr, "Could not allocate stream\n");
        return 1;
    }

    out_codec_audio_ctx = avcodec_alloc_context3(out_codec_audio);
    if(set_options_and_open_encoder_audio(format_ctx, out_stream_audio, out_codec_audio_ctx, out_codec_audio,
                                    config.audio_channel, config.sample_rate, codec_id)) {
        return 1;
    }
    out_stream_audio->codecpar->extradata_size = out_codec_audio_ctx->extradata_size;
    out_stream_audio->codecpar->extradata = static_cast<uint8_t*>(av_mallocz(out_codec_audio_ctx->extradata_size));
    memcpy(out_stream_audio->codecpar->extradata, out_codec_audio_ctx->extradata, out_codec_audio_ctx->extradata_size);

    printf("audio frame_size=%d\n",out_codec_audio_ctx->frame_size);
    printf("audiocodec capability=%x,extradata_size=%d\n",out_codec_audio->capabilities,out_codec_audio_ctx->extradata_size);

    //----------------------------------------------------------------
    av_dump_format(format_ctx, 0, config.server.c_str(), 1);

    picture.init(out_codec_ctx->pix_fmt, config.dst_width, config.dst_height);
    scaler.init(out_codec_ctx, config.src_width, config.src_height,config.dst_width, config.dst_height, SWS_BILINEAR);
    audioframe.init(out_codec_audio_ctx->sample_fmt, out_codec_audio_ctx->frame_size, out_codec_audio_ctx->channel_layout);
    if (!(fifo = av_audio_fifo_alloc(out_codec_audio_ctx->sample_fmt,
                                      out_codec_audio_ctx->channels, 1))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }

    if (avformat_write_header(format_ctx, nullptr) < 0)
    {
        fprintf(stderr, "Could not write header!\n");
        return 1;
    }

    printf("videostream time base = %d / %d, audiostream time base = %d / %d \n", 
    out_stream->time_base.num, out_stream->time_base.den,out_stream_audio->time_base.num, out_stream_audio->time_base.den);

    inv_stream_timebase = (double)out_stream->time_base.den/(double)out_stream->time_base.num;

    init_ok = true;
    return 0;
}

} // namespace streamer
