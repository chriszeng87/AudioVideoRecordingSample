#include "rtmp_sender.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
//#include <unistd.h>
#ifdef WIN32
#define snprintf _snprintf
#endif
#include <signal.h>
#ifdef __cplusplus
extern "C"{
#endif
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
#include "libavutil/samplefmt.h"
//#include <libavutil/mathematics.h>
#include "libavformat/avformat.h"
#include "libavformat/url.h"
#include "libavutil/time.h"
#include "libavutil/avutil.h"
#include "libavutil/mathematics.h"
extern int ff_rtmp_start_publish(void *opaque, int reconnect, uint64_t flag, int64_t ts_us);
extern int ff_rtmp_stop_publish(void *opaque);
//extern int ff_rtmp_send_user_cmd(void *opaque, const char *method, const char *user_data);
#ifdef __cplusplus
}
#endif

//#define PROFILE


static AVCodec stub_libx264_encoder;
static AVCodec stub_libfaac_encoder;

static void init_stub_codecs(){
    memset(&stub_libx264_encoder, 0, sizeof(AVCodec));
    stub_libx264_encoder.name             = "libx264";
    stub_libx264_encoder.type             = AVMEDIA_TYPE_VIDEO;
    stub_libx264_encoder.id               = AV_CODEC_ID_H264;
//    avcodec_register(&stub_libx264_encoder);
    memset(&stub_libfaac_encoder, 0, sizeof(AVCodec));
    stub_libfaac_encoder.name           = "libfaac";
    stub_libfaac_encoder.type           = AVMEDIA_TYPE_AUDIO;
    stub_libfaac_encoder.id             = AV_CODEC_ID_AAC;
//    avcodec_register(&stub_libfaac_encoder);

}

/* 5 seconds stream duration */
#define STREAM_DURATION   20000.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
#define MAX_INT 0x7fffffff

#if (defined ANDROID) || (defined WIN32)
    extern AVRational avtimebase;
# else
    AVRational avtimebase = {1, AV_TIME_BASE};
#endif



#undef AV_TIME_BASE_Q
#define AV_TIME_BASE_Q avtimebase

class SmartMuxer{
public:
    SmartMuxer(const char *file, const char *f) : 
        m_status(INITED),
        m_audio_codec(AV_CODEC_ID_NONE),
        m_audio_sample_rate(0),
        m_audio_bit_rate(0),
        m_audio_channels(0),
        m_audio_time_base(av_d2q(0.0, MAX_INT)),
        m_audio_packet_format(0),
        m_audio_codec_extra_data_size(0),
        m_audio_codec_extra_data(NULL),
        m_a_bitstream_filters(NULL),
        m_video_codec(AV_CODEC_ID_NONE),
        m_video_width(0),
        m_video_height(0),
        m_video_bitrate(0),
        m_video_frame_rate(av_d2q(0.0, MAX_INT)),
        m_video_time_base(av_d2q(0.0, MAX_INT)),
        m_video_codec_extra_data_size(0),
        m_video_codec_extra_data(NULL),
        filename(av_strdup(file)),
        format(av_strdup(f)),
        fmt(NULL),
        oc(NULL),
        audio_st(NULL), 
        video_st(NULL),
        audio_st_idx(-1),
        video_st_idx(-1),
        audio_codec(NULL),
        video_codec(NULL),
        audio_pts(0.0),
        video_pts(0.0),
        frame_count(0),
#ifdef PROFILE
        m_total_time(0),
        m_total_size(0),
        m_max_time_use(0),
#endif

		pb(NULL),
        m_rtmp_options(NULL){
        
        signal(SIGPIPE, SIG_IGN);
        init_stub_codecs();
		m_usercommand_error_invoke = NULL;
		m_usercommand_result_invoke = NULL;
    }

    ~SmartMuxer(){
        stop();
        if(filename){
            av_free(filename);
        }
        if(format){
            av_free(format);
        }
    }


    int set_audio_params( int codec,
                                int ch,
                                int smp_rate,
                                int bit_rate,
                                AVRational time_base,
                                int bits_per_coded_sample, 
                                int pkt_fmt,
                                int extra_size = 0,
                                const uint8_t *extra_data = NULL){
        if(m_status != PREPARED){
            return -1;
        }

        switch(codec) {
            //no distinction between S16 and S8 PCM codec flags
        case FLV_CODECID_PCM:
        case FLV_CODECID_PCM_LE:
            m_audio_codec = bits_per_coded_sample == 8 ? AV_CODEC_ID_PCM_U8 :
                                AV_CODEC_ID_PCM_S16LE;
            break;
        case FLV_CODECID_AAC:
            m_audio_codec =  AV_CODEC_ID_AAC;
            break;
        case FLV_CODECID_ADPCM:
            m_audio_codec =  AV_CODEC_ID_ADPCM_SWF;
            break;
        case FLV_CODECID_SPEEX:
            m_audio_codec =  AV_CODEC_ID_SPEEX;
            break;
        case FLV_CODECID_MP3:
            m_audio_codec =  AV_CODEC_ID_MP3;
            break;
        case FLV_CODECID_NELLYMOSER_8KHZ_MONO:
        case FLV_CODECID_NELLYMOSER_16KHZ_MONO:
        case FLV_CODECID_NELLYMOSER:
            m_audio_codec =  AV_CODEC_ID_NELLYMOSER;
            break;
        case FLV_CODECID_PCM_MULAW:
            m_audio_codec =  AV_CODEC_ID_PCM_MULAW;
            break;
        case FLV_CODECID_PCM_ALAW:
            m_audio_codec =  AV_CODEC_ID_PCM_ALAW;
            break;
        default:
            return -1;
        }

        m_audio_sample_rate = smp_rate;
        m_audio_bit_rate = smp_rate;
        m_audio_channels = ch;
        m_audio_time_base = time_base;
        m_audio_packet_format = pkt_fmt;
        if(extra_size != 0 && extra_data!= NULL){
            return set_extra_data(1, extra_size, extra_data);
        }
        
        return 0;
    }
    
    int set_video_params( int codec,
                                int32_t width,
                                int32_t height,
                                int32_t bitrate,
                                AVRational frame_rate,
                                AVRational time_base,
                                int video_pkt_fmt,
                                int extra_size = 0,
                                const uint8_t *extra_data = NULL){
        if(m_status != PREPARED){
            return -1;
        }
        switch (codec) {
        case FLV_CODECID_H263:
            m_video_codec = AV_CODEC_ID_FLV1;
            break;
        case FLV_CODECID_SCREEN:
            m_video_codec = AV_CODEC_ID_FLASHSV;
            break;
        case FLV_CODECID_SCREEN2:
            m_video_codec = AV_CODEC_ID_FLASHSV2;
            break;
        case FLV_CODECID_VP6:
            m_video_codec = AV_CODEC_ID_VP6F;
            break;
        case FLV_CODECID_VP6A:
            m_video_codec = AV_CODEC_ID_VP6A;
            break;
        case FLV_CODECID_H264:
            m_video_codec = AV_CODEC_ID_H264;
            break;
        default:
            return -1;
        }

        m_video_time_base = time_base;
        m_video_width = width;
        m_video_height = height;
        m_video_bitrate = bitrate;
        m_video_frame_rate = frame_rate;
        return set_extra_data(0, extra_size, extra_data);
    }

    int set_extra_data(bool is_audio, int extra_size, const uint8_t *extra_data){
        if(extra_size == 0 || extra_data == NULL){
            av_log(NULL, AV_LOG_ERROR, "There is no extra data!\n");
            return 0;
        }
        uint8_t **extra;
        int *size;
        if(is_audio){
            extra = &m_audio_codec_extra_data;
            size = &m_audio_codec_extra_data_size;
        }else{
            extra = &m_video_codec_extra_data;
            size = &m_video_codec_extra_data_size;
        }

        *extra = (uint8_t*)av_realloc(*extra, extra_size);
        if (!*extra) {
            return AVERROR(ENOMEM);
        }
        memcpy(*extra, extra_data, extra_size);
        *size = extra_size;
        return 0;
    }

    int prepare(int debug){
        int ret;
        if(m_status != INITED){
            return -1;
        }
        
        /* Initialize libavcodec, and register all codecs and formats. */
        av_register_all();
        avformat_network_init();
        av_log_set_level(AV_LOG_INFO);


        /* allocate the output media context */
        avformat_alloc_output_context2(&oc, NULL, format, filename);
        if (!oc) {
            oc = NULL;
            return -1;
        }
        fmt = oc->oformat;
        if(oc != NULL){
            avformat_free_context(oc);
            oc = NULL;
        }

        /* open the output file, if needed */
        if (!(fmt->flags & AVFMT_NOFILE)) {
            ret = avio_open2(&pb, filename, AVIO_FLAG_WRITE, NULL, NULL);
            if (ret < 0) {
                pb = NULL;
                fprintf(stderr, "Could not open '%s': %d\n", filename, ret);
                goto FAIL;
            }
        }
        m_status = PREPARED;
        return 0;
FAIL:
        if(pb != NULL){
            avio_close(pb);
            pb = NULL;
        }
        return ret;

    }

    int get_fd() const{
        if(pb == NULL || pb->opaque == NULL){
            return -1;
        }
        URLContext *s = (URLContext *)(pb->opaque);
        if(s->prot == NULL || s->prot->url_get_file_handle == NULL){
            return -1;
        }
        return s->prot->url_get_file_handle(s);
    }


    int start_publish(uint64_t flag, int64_t ts_us){
        int ret;
        if(m_status != PREPARED){
            return -1;
        }

        /* allocate the output media context */
        avformat_alloc_output_context2(&oc, NULL, format, filename);
        if (!oc) {
            goto FAIL;
        }
        oc->pb = pb;
        
        /* Add the audio and video streams using the default format codecs
         * and initialize the codecs. */
        video_st = NULL;
        audio_st = NULL;
        if (m_video_codec != AV_CODEC_ID_NONE) {
            video_codec = &stub_libx264_encoder;
            stub_libx264_encoder.id = m_video_codec;
            video_st = add_stream(oc, &video_codec, m_video_codec);
        }else{
            av_log(NULL, AV_LOG_WARNING, "no video stream\n");
        }
        if (m_audio_codec != AV_CODEC_ID_NONE) {
            if(m_audio_codec == AV_CODEC_ID_AAC && m_audio_packet_format == AAC_FORMAT_ADTS){
                AVBitStreamFilterContext *bsfc = NULL;
                const char *bsf = "aac_adtstoasc";
                if (!(bsfc = av_bitstream_filter_init(bsf))){
                    av_log(NULL, AV_LOG_FATAL, "Unknown bitstream filter %s\n", bsf);
                    return -1;
                }
                m_a_bitstream_filters = bsfc;
            }
            audio_codec = &stub_libfaac_encoder;
            stub_libx264_encoder.id = m_audio_codec;
            audio_st = add_stream(oc, &audio_codec, m_audio_codec);
        }else{
            av_log(NULL, AV_LOG_WARNING, "no audio stream\n");
        }
        
        /* Now that all the parameters are set, we can open the audio and
         * video codecs and allocate the necessary encode buffers. */
        if (video_st){
            //open_codec(oc, video_codec, video_st);
            video_st_idx = video_st->id;
        }
        if (audio_st){
            //open_codec(oc, audio_codec, audio_st);
            audio_st_idx = audio_st->id;
        }
//        av_dump_format(oc, 0, filename, 1);

        ret = ff_rtmp_start_publish(oc->pb->opaque, 0, flag, ts_us);
        if(ret != 0){
            goto FAIL;
        }
        
        /* Write the stream header, if any. */
        ret = avformat_write_header(oc, NULL);
        if (ret < 0) {
            fprintf(stderr, "Error occurred when opening output file: %d\n", ret);
            goto FAIL;
        }
        m_status = PUBLISHING;
        return 0;
FAIL:
        /* Close each codec. */
        if (video_st)
            close_codec(oc, video_st);
        if (audio_st)
            close_codec(oc, audio_st);

        AVBitStreamFilterContext *bsfc = m_a_bitstream_filters;
        while (bsfc) {
            AVBitStreamFilterContext *next = bsfc->next;
            av_bitstream_filter_close(bsfc);
            bsfc = next;
        }
        m_a_bitstream_filters = NULL;

        if(oc){
            avformat_free_context(oc);
            oc = NULL;
        }
        video_st = audio_st = NULL;

        return -1;
    }

    int stop_publish(){
        if(m_status != PUBLISHING){
            return -1;
        }
        if(oc == NULL){
            return 0;
        }
        /* Write the trailer, if any. The trailer must be written before you
         * close the CodecContexts open when you wrote the header; otherwise
         * av_write_trailer() may try to use memory that was freed on
         * av_codec_close(). */
        av_write_trailer(oc);
        
        /* Close each codec. */
        if (video_st)
            close_codec(oc, video_st);
        if (audio_st)
            close_codec(oc, audio_st);
        
        AVBitStreamFilterContext *bsfc = m_a_bitstream_filters;
        while (bsfc) {
            AVBitStreamFilterContext *next = bsfc->next;
            av_bitstream_filter_close(bsfc);
            bsfc = next;
        }
        m_a_bitstream_filters = NULL;
        
        ff_rtmp_stop_publish(oc->pb->opaque);
        avformat_free_context(oc);
        oc = NULL;
        video_st = audio_st = NULL;

        m_status = PREPARED;
        return 0;
    }

    int write_audio_frame(AVPacket *pkt)
    {
    
#ifdef PROFILE
        int64_t st = av_gettime();
#endif
        if(m_status != PUBLISHING){
            av_log(NULL, AV_LOG_ERROR, "In write_audio_frame, m_status is not PUBLISHING!");
            return -1;
        }

        pkt->stream_index = audio_st_idx;
        if(pkt->pts == AV_NOPTS_VALUE){
            compute_audio_pts(pkt);
        }

        av_log(NULL, AV_LOG_DEBUG, "stream%d: data:0x%llx size:%d dts:%lld timebase:%d/%d flags:0x%x\n", 
                pkt->stream_index, (int64_t)pkt->data, pkt->size, pkt->dts, 
                audio_st->time_base.num, audio_st->time_base.den, pkt->flags);
        AVBitStreamFilterContext *bsfc = m_a_bitstream_filters;
        while (bsfc) {
            AVPacket new_pkt = *pkt;
            AVCodecContext          *avctx = audio_st->codec;
            int a = av_bitstream_filter_filter(bsfc, avctx, NULL,
                                               &new_pkt.data, &new_pkt.size,
                                               pkt->data, pkt->size,
                                               pkt->flags & AV_PKT_FLAG_KEY);
            if (a > 0) {
                av_free_packet(pkt);
                new_pkt.destruct = av_destruct_packet;
            } else if (a < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open bitstream filter %s for stream %d with codec %s",
                       bsfc->filter->name, pkt->stream_index,
                       avctx->codec ? avctx->codec->name : "copy");
                return a;
            }
            *pkt = new_pkt;
            bsfc = bsfc->next;
        }
        int ret = av_interleaved_write_frame(oc, pkt);
        if (ret != 0) {
            fprintf(stderr, "Error while writing audio frame: %d\n", (ret));
            return ret;
        }

#ifdef PROFILE
        int time_use_us = av_gettime() - st;
        if(m_max_time_use < time_use_us){
            m_max_time_use = time_use_us;
        }
        m_total_time += time_use_us;
        m_total_size += pkt->size;
        av_log(NULL, AV_LOG_DEBUG, "write_audio_frame done! time=[%dus], avg_speed=[%lldkbps][%lldbyte/%lldus] max_time_use=[%d]\n", 
                time_use_us, m_total_size * 8000 / m_total_time, m_total_size, m_total_time, m_max_time_use);
#endif
        
        return 0;
    }


    int write_video_frame(AVPacket *pkt)
    {
#ifdef PROFILE
        int64_t st = av_gettime();
#endif
        int ret;
        if(m_status != PUBLISHING){
            av_log(NULL, AV_LOG_ERROR, "In write_video_frame, m_status is not PUBLISHING!");

			av_free_packet(pkt);
            return -1;
        }

        pkt->stream_index = video_st_idx;
        if(pkt->pts == AV_NOPTS_VALUE){
            compute_audio_pts(pkt);
        }
        
        //printf("\ndata = 0x%llx\n", (int64_t)pkt->data);
        //printf("\nsize = %d\n", pkt->size);
        //printf("\ndts = %lld\n", pkt->dts);
        //printf("\ntimebase = %d/%d", video_st->time_base.num, video_st->time_base.den);
        av_log(NULL, AV_LOG_DEBUG, "stream%d: data:0x%llx size:%d dts:%lld timebase:%d/%d flags:0x%x\n", 
                pkt->stream_index, (int64_t)pkt->data, pkt->size, pkt->dts, 
                video_st->time_base.num, video_st->time_base.den, pkt->flags);
        ret = av_interleaved_write_frame(oc, pkt);

        if (ret != 0) {
			av_free_packet(pkt);
            fprintf(stderr, "Error while writing video frame: %d\n", (ret));
            return ret;
        }
        frame_count++;
#ifdef PROFILE
        int time_use_us = av_gettime() - st;
        if(m_max_time_use < time_use_us){
            m_max_time_use = time_use_us;
        }

        m_total_time += time_use_us;
        m_total_size += pkt->size;
        av_log(NULL, AV_LOG_DEBUG, "write_video_frame done! time=[%dus], avg_speed=[%lldkbps][%lldbyte/%lldus] max_time_use=[%d]\n", 
                time_use_us, m_total_size * 8000 / m_total_time, m_total_size, m_total_time, m_max_time_use);

#endif
		av_free_packet(pkt);
        return 0;
    }


void stop(){

    if(m_status == PUBLISHING){
        //stop_publish();
    }

    if (fmt != NULL && !(fmt->flags & AVFMT_NOFILE) && pb != NULL){
        /* Close the output file. */
        avio_close(pb);
        pb = NULL;
    }
    
    av_free(m_audio_codec_extra_data);
    m_audio_codec_extra_data = NULL;
    m_audio_codec_extra_data_size = 0;

    av_free(m_video_codec_extra_data);
    m_video_codec_extra_data = NULL;
    m_video_codec_extra_data_size = 0;

    /* free the stream */
    if(oc != NULL){
        avformat_free_context(oc);
        oc = NULL;
    }

}

    
private:

    int compute_video_pts(AVPacket *pkt){

        return 0;
    }

    int compute_audio_pts(AVPacket *pkt){

        return 0;
    }

    int copy_extra_data(AVCodecContext *c, const uint8_t *extradata, int extradata_size){
        if(extradata == NULL || extradata_size <= 0){
            return 0;
        }
        fprintf(stderr, "copy_extra_data '%s', [size=%d]\n", avcodec_get_name(c->codec_id), extradata_size);
        uint64_t extra_size = (uint64_t)extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
        if (extra_size > INT_MAX) {
            return AVERROR(EINVAL);
        }
        c->extradata      = (uint8_t*)av_mallocz(extra_size);
        if (!c->extradata) {
            return AVERROR(ENOMEM);
        }
        memcpy(c->extradata, extradata, extradata_size);
        c->extradata_size= extradata_size;
        return 0;
    }

    /* Add an output stream. */
    AVStream *add_stream(AVFormatContext *oc, AVCodec **codec,
                                enum AVCodecID codec_id)
    {
        AVCodecContext *c;
        AVStream *st;
        
        if(*codec == NULL){
            /* find the encoder */
            *codec = avcodec_find_encoder(codec_id);
            if (!(*codec)) {
                fprintf(stderr, "Could not find encoder for '%s'\n",
                        avcodec_get_name(codec_id));
                return NULL;
            }
        }

        st = avformat_new_stream(oc, *codec);
        if (!st) {
            fprintf(stderr, "Could not allocate stream\n");
            return NULL;
        }
        st->id = oc->nb_streams-1;
        c = st->codec;

        switch ((*codec)->type) {
        case AVMEDIA_TYPE_AUDIO:
            c->time_base = m_audio_time_base;
            c->codec_id = codec_id;
            c->codec_type = (*codec)->type;
            c->sample_fmt  = AV_SAMPLE_FMT_S16;
            c->bit_rate    = m_audio_bit_rate;
            c->sample_rate = m_audio_sample_rate;
            c->channels    = m_audio_channels;
            /*
            if(codec_id == AV_CODEC_ID_ADPCM_SWF){
                c->frame_size  = 4096;
            }else{
                c->frame_size = 1024;
            }
            */
            copy_extra_data(c, m_audio_codec_extra_data, m_audio_codec_extra_data_size);
            break;

        case AVMEDIA_TYPE_VIDEO:
            avcodec_get_context_defaults3(c, *codec);
            c->codec_id = codec_id;
            c->codec_type = (*codec)->type;
            c->bit_rate = m_video_bitrate;
            /* Resolution must be a multiple of two. */
            c->width    = m_video_width;
            c->height   = m_video_height;
            /* timebase: This is the fundamental unit of time (in seconds) in terms
             * of which frame timestamps are represented. For fixed-fps content,
             * timebase should be 1/framerate and timestamp increments should be
             * identical to 1. */
            //c->time_base.den = STREAM_FRAME_RATE;
            //c->time_base.num = 1;
            c->time_base = m_video_time_base;
            st->avg_frame_rate = m_video_frame_rate;
            //c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
            c->pix_fmt       = STREAM_PIX_FMT;            
            copy_extra_data(c, m_video_codec_extra_data, m_video_codec_extra_data_size);
            break;

        default:
            break;
        }

        /* Some formats want stream headers to be separate. */
        if (oc->oformat->flags & AVFMT_GLOBALHEADER)
            c->flags |= CODEC_FLAG_GLOBAL_HEADER;

        return st;
    }

    int open_codec(AVFormatContext *oc, AVCodec *codec, AVStream *st)
    {
        int ret;
        AVCodecContext *c = st->codec;
    
        /* open the codec */
        ret = avcodec_open2(c, codec, NULL);
        if (ret < 0) {
            fprintf(stderr, "Could not open video codec: %d\n", (ret));
            return -1;
        }
        return 0;
    }

    void close_codec(AVFormatContext *oc, AVStream *st){
        avcodec_close(st->codec);
    }
    
    enum RtmpStatus{
        INITED     = 0,
        PREPARED   = 1,
        PUBLISHING = 2
    };

    RtmpStatus m_status;

    AVCodecID m_audio_codec;
    int32_t m_audio_sample_rate;
    int32_t m_audio_bit_rate;
    int32_t m_audio_channels;
    AVRational m_audio_time_base;
    int m_audio_packet_format;
    int m_audio_codec_extra_data_size;
    uint8_t* m_audio_codec_extra_data;
    AVBitStreamFilterContext *m_a_bitstream_filters;

    AVCodecID m_video_codec;
    int32_t m_video_width;
    int32_t m_video_height;
    int32_t m_video_bitrate;
    AVRational m_video_frame_rate;
    AVRational m_video_time_base;
    int m_video_codec_extra_data_size;
    uint8_t* m_video_codec_extra_data;

    char *filename;
    char *format;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVIOContext     *pb;
    AVStream *audio_st; 
    AVStream *video_st;
    int audio_st_idx;
    int video_st_idx;
    AVCodec *audio_codec;
    AVCodec *video_codec;
    double audio_pts;
    double video_pts;
    int64_t frame_count;
#ifdef PROFILE
    int m_max_time_use;
    int64_t m_total_time;
    int64_t m_total_size;
#endif


    
};



void *rtmp_sender_alloc(const char *url){
    if(url == NULL){
        return NULL;
    }
    return new SmartMuxer(url, "flv");
}



int rtmp_sender_set_audio_params( void *handle,
                                int codec,
                                int ch,
                                int smp_rate,
                                int bit_rate,
                                int bits_per_coded_sample, 
                                int extra_size,
                                const uint8_t *extra_data,
                                int audio_pkt_fmt){
    SmartMuxer * muxer = (SmartMuxer *)handle;
    AVRational time_base = {1, 90000};
    return muxer->set_audio_params(codec, ch, smp_rate, bit_rate, 
            time_base, bits_per_coded_sample, 
            audio_pkt_fmt, extra_size, extra_data);
}

int rtmp_sender_set_video_params( void *handle,
                            int codec,
                            int32_t width,
                            int32_t height,
                            int32_t bitrate,
                            int frame_rate_num, int frame_rate_den,
                            int extra_size,
                            const uint8_t *extra_data,
                            int video_pkt_fmt){
    SmartMuxer * muxer = (SmartMuxer *)handle;
    AVRational time_base = {1, 90000};
    AVRational frame_rate = {frame_rate_num, frame_rate_den};
    return muxer->set_video_params(codec, width, height, bitrate, frame_rate,
                            time_base, video_pkt_fmt, extra_size, extra_data);
}

int rtmp_sender_prepare(void *handle, int debug){
    SmartMuxer * muxer = (SmartMuxer *)handle;
    return muxer->prepare(debug);
}

int rtmp_sender_start_publish(void *handle, uint32_t flag, int64_t ts_us){
    SmartMuxer * muxer = (SmartMuxer *)handle;
    return muxer->start_publish(flag, ts_us);
}

int rtmp_sender_stop_publish(void *handle){
    SmartMuxer * muxer = (SmartMuxer *)handle;
    return muxer->stop_publish();
}

int rtmp_sender_get_fd(void *handle){
    SmartMuxer * muxer = (SmartMuxer *)handle;
    return muxer->get_fd();
}

int rtmp_sender_write_audio_frame(void *handle, 
                                    uint8_t *data, int size, uint64_t dts_us){
    AVPacket pkt;
    AVRational time_base = {1, 1000};
    av_init_packet(&pkt);
    pkt.data = data;
    pkt.size = size;
    pkt.flags = AV_PKT_FLAG_KEY;
    pkt.dts = av_rescale_q(dts_us, AV_TIME_BASE_Q, time_base);
    SmartMuxer * muxer = (SmartMuxer *)handle;
    return muxer->write_audio_frame(&pkt);
}

int rtmp_sender_write_video_frame(void *handle, 
                                    uint8_t *data, int size, uint64_t dts_us, int key){
    AVPacket pkt;
    AVRational time_base = {1, 1000};
    av_init_packet(&pkt);
    pkt.data = data;
    pkt.size = size;
    pkt.flags = (key != 0 ? AV_PKT_FLAG_KEY : 0);
    pkt.dts = av_rescale_q(dts_us, 
            AV_TIME_BASE_Q, time_base);
    SmartMuxer * muxer = (SmartMuxer *)handle;
    return muxer->write_video_frame(&pkt);
}

void rtmp_sender_free(void *handle){
    SmartMuxer * muxer = (SmartMuxer *)handle;
    delete muxer;
}


