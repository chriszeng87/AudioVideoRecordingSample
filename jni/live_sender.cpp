#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#ifdef ANDORID
#include <android/log.h>
#endif
#include <unistd.h>

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "put_bits.h"
    
    
#ifdef ANDROID
#include "libfaac/faac.h"
#include "libfaac/faaccfg.h"
#include "aecm/echo_control_mobile.h"
#endif
#include "rtmp_sender.h"

#ifdef WIN32

#define inline __inline
#define isnan(x) ((x)!=(x))
#include "inttypes.h"
#include "w32pthreads.h"
#define SDL_Delay(x) Sleep(x)
#else
#include "pthread.h"
#ifdef ANDROID
#include <unistd.h>
#include <android/log.h>

#endif
#endif

#include "packet_queue.h"

#ifdef __cplusplus
}
#endif

#ifdef ANDROID
#include "audiosl/opensl_audio.h"
#include "live_avsource.h"
#endif

#include "rtmp_sender.h"
#include "live_sender.h"

#include "live_state.h"


#define BUF_SIZE 10240

#define MAX_STR_LENGTH 1024

#ifndef ANDROID
static int64_t mTimeS = 0;
#endif

typedef void* (*FUNC_THREAD)(void *p);

typedef void (*FUNC_CALLBACK)(int);
typedef void (*FUNC_USRCMD_CALLBACK) (char*);

#undef AV_TIME_BASE_Q

#define AV_TIME_BASE_Q avtimebase

#define MAXSIZE 512 * 1024

static int sockfd;

static ContextSender* temp_context; //ugly code, to be deleted
static unsigned char* s_video_extra_data;
static int s_video_extra_data_length;

static int no_video_frame_yet = 1;


int read_data(void * opaque, unsigned char * buf, int buf_size)
{
    int ret = 0;
    
    int length = recv(sockfd, buf, buf_size, 0);
    if (length < 0) {//接收信息
        printf("\nserver has no reply!\n");
    } else {
        //        printf("\nreceived data %d bytes\n", length);
        ret = length;
    }
    
	return length;
}

struct dts_list{
    int num;
    int size;
    float dtss[100];
};


struct ContextSender
{
	char urlrtmp[MAX_STR_LENGTH];

	AVDictionary * sender_opts;

	int status;

	SENDER_INTERNAL_STATE internal_state;
	SENDER_EVENT last_incoming_call;
	SENDER_EVENT last_success_call;

	void * muxer;
	// for FLV AAC spec config
	unsigned char adts_extra_data[4];
	int extra_data_size;
	int channels;
	int sample_rate;
	int bits_per_coded_sample;
	int signal_strength;
    
    unsigned char* video_extra_data;
    int video_extra_data_length;
    
    int video_width;
    int video_height;
    int audio_num;
    int video_num;
    dts_list *vdts;
    dts_list *adts;
    
    int video_quality;
    double audio_dts;
    double video_dts;
    bool audio_only_mode;
    int qos;
    
    int drop_head;
    
	// for packet fetch and processing
	FUNC_THREAD func_send_rtmp;



	FUNC_CALLBACK func_error_callback;
	FUNC_CALLBACK func_status_callback;

	FUNC_USRCMD_CALLBACK func_usrcmd_error_callback;
	FUNC_USRCMD_CALLBACK func_usrcmd_status_callback;

	// for input source
	int		src_type;
	IAVSource *	src_ctx;

	pthread_t tid_send;


	PacketQueue* sendQueue;
    #if (defined ANDROID) || (defined WIN32)

    FUNC_THREAD func_process_data;
    pthread_t tid_process;
	PacketQueue* srcQueue;


	// for faac encoder
	faacEncHandle hEncoder;
	faacEncConfigurationPtr pConfiguration;
	uint32_t nMaxOutpuBytes;
	uint32_t nPCMBufferSize;
	void* pbAACBuffer;
	uint64_t sample_count; // for dts calculation

	// for aecm
	void* aecmInst;
	int needaecm;
	AudioSL* audiosl;
	short aecmbuf[4096];
	char *audiobuf[4096];

	// for resample
	SwrContext *swr_ctx;
	uint8_t **src_data;
	uint8_t **dst_data;
	int src_linesize;
	int dst_linesize;
	int max_dst_nb_samples;
    #endif
};

static int init_faac(ContextSender* p);

int checkStateChange(ContextSender* pctx, SENDER_EVENT eventID){

	pctx->last_incoming_call = eventID;

	int ret = ID_SENDER_ERROR_INVALID_OP;

	switch (pctx->internal_state)
	{
	case ID_SENDER_STATE_IDLE:
		if(eventID == ID_SENDER_EVENT_CREATE)
			ret = ID_SENDER_STATE_INITED;
		break;

	case ID_SENDER_STATE_INITED:
		if(eventID == ID_SENDER_EVENT_SET_CALLBACK)
			ret = ID_SENDER_STATE_INITED;
		if(eventID == ID_SENDER_EVENT_CONNECT)
			ret = ID_SENDER_STATE_CONNECT_ESTABLISHED;

		break;

	case ID_SENDER_STATE_CONNECT_ESTABLISHED:
		if(eventID == ID_SENDER_EVENT_START)
			ret = ID_SENDER_STATE_PUBLISHING;
		if(eventID == ID_SENDER_EVENT_CLOSE)
			ret = ID_SENDER_STATE_CONNECT_CLOSED;
		if(eventID == ID_SENDER_EVENT_SEND_CMD)
			ret = ID_SENDER_STATE_CONNECT_ESTABLISHED;
		break;

	case ID_SENDER_STATE_CONNECT_CLOSED:
		if(eventID == ID_SENDER_EVENT_SHUTDOWN)
			ret = ID_SENDER_STATE_IDLE;
		if(eventID == ID_SENDER_EVENT_CONNECT)
			ret = ID_SENDER_STATE_CONNECT_ESTABLISHED;
		break;

	case ID_SENDER_STATE_PUBLISHING:

		if(eventID == ID_SENDER_EVENT_STOP)
			ret = ID_SENDER_STATE_CONNECT_ESTABLISHED;\

		if(eventID == ID_SENDER_EVENT_SEND_CMD)
			ret = ID_SENDER_STATE_PUBLISHING;

		if(eventID == ID_SENDER_EVENT_PAUSE)
			ret = ID_SENDER_STATE_PUBLISH_WAITING;
		break;

	case ID_SENDER_STATE_PUBLISH_WAITING:

		if(eventID == ID_SENDER_EVENT_STOP)
			ret = ID_SENDER_STATE_CONNECT_ESTABLISHED;

		if(eventID == ID_SENDER_EVENT_RESUME)
			ret = ID_SENDER_STATE_PUBLISHING;

		if(eventID == ID_SENDER_EVENT_SEND_CMD)
			ret = ID_SENDER_STATE_PUBLISH_WAITING;

		break;

	default:
		break;
	}

	if(ret < 0 && pctx->func_error_callback)
		pctx->func_error_callback(ret);

	return ret;
}

void doStateChange(ContextSender* pctx,  SENDER_EVENT eventID)
{
	//TODO mutex for state?
	int newState = checkStateChange(pctx, eventID);
	pctx->internal_state = (SENDER_INTERNAL_STATE) newState;
	pctx->last_success_call = eventID;

	if(pctx->func_status_callback)
		pctx->func_status_callback(newState);
}


#if (defined ANDROID) || (defined WIN32)
static void * process_faac_thread_func(void* parg);

int pkt_aacadts_to_sender(void *opaque, void *buf, int buf_size, int type)
{
	ContextSender* p = (ContextSender* )opaque;
    AVPacket pkt = { 0 };
	pkt.data = (uint8_t*)buf;
	pkt.size = buf_size;

	p->sample_count ++;

	pkt.dts = p->sample_count * 23;

	packet_queue_put(p->sendQueue,&pkt);

	if(p->sendQueue->size > 200 * 1024)
#ifdef WIN32
		Sleep(100);
#else
		usleep(100 * 1000);
#endif

	return 0;

}
#endif

#ifndef ANDROID
int ios_pkt_aacadts_to_sender(void *opaque, void *buf, int buf_size, int type)
{
	ContextSender* p = temp_context;
    if (!p->audio_only_mode && no_video_frame_yet) {
        return 0;
    }
    
    uint64_t dts = *(uint64_t*)opaque;

    AVPacket pkt = { 0 };
	pkt.data = (uint8_t*)buf;
	pkt.size = buf_size;
    pkt.stream_index = 0;
    
    if (p->audio_only_mode) {
        if (mTimeS == 0) {
            mTimeS = time(NULL) * 1000;
        }
        pkt.dts = mTimeS + dts/42 ;//p->sample_count*1000/p->sample_rate/p->channels;
    } else {
        p->sendQueue->audio_pkt_num++;
        if(p->sendQueue->audio_pkt_num > 10 && p->sendQueue->video_pkt_num > 0) {
            if(p->drop_head == 1 && p->vdts->size == 0 && p->vdts->dtss[0] == -1 && p->vdts->dtss[1] == -1)
                p->vdts->dtss[p->vdts->size] = 0;
            else {
                p->vdts->dtss[p->vdts->size] = (float)1000 / p->sendQueue->video_pkt_num;
            }
        
            p->vdts->size = p->vdts->size == 99 ? 0: p->vdts->size + 1;
            p->sendQueue->video_pkt_num = 0;
        }
    }
    
    packet_queue_put(p->sendQueue, &pkt);

	//if(p->srcQueue->size > 500 * 1024)
	//	usleep(200 * 1000);
    return 0;

}

int pkt_h264_to_sender(void *opaque, void*buf, int buf_size, double dts, int key) {
	ContextSender* p = (ContextSender* )opaque;
    
    
    AVPacket pkt = { 0 };
    pkt.data = (u_int8_t*)buf;
    pkt.size = buf_size;
    pkt.flags = key;
    pkt.stream_index = 1;
    
    if (no_video_frame_yet) {
        no_video_frame_yet = 0;
        return 0;
    }
//    m_pkt_dts += 40000;
//    pkt.dts =m_pkt_dts;  // to do
    
    int ret = packet_queue_put(p->sendQueue, &pkt);
    
    if(p->sendQueue->audio_pkt_num > 10) {
        if(p->drop_head == 1 && p->adts->size == 0 && p->adts->dtss[0] == -1 && p->adts->dtss[1] == -1)
            p->adts->dtss[p->adts->size] = 0;
        else
            p->adts->dtss[p->adts->size] = (float)1000/p->sendQueue->audio_pkt_num;
        p->adts->size = p->adts->size == 99 ? 0:p->adts->size + 1;
        p->sendQueue->audio_pkt_num = 0;
    }
    p->sendQueue->video_pkt_num++;
    
    
    
    return ret;
}
#endif

#ifdef ANDROID
int pkt_pcm_to_sender(void *opaque, void *buf, int buf_size, int type)
{
	ContextSender* p = (ContextSender* )opaque;



	if(buf == 0 && type == 0) // return pcm chunk size
		return  p->nPCMBufferSize;

	if(buf == 0 && type == 1)
		return p->channels;

	if(buf == 0 && type == 2)
		return p->sample_rate;




    AVPacket pkt = { 0 };
	pkt.data = (uint8_t*)buf;
	pkt.size = buf_size;

	packet_queue_put(p->srcQueue,&pkt);

	//if(p->srcQueue->size > 500 * 1024)
	//	usleep(200 * 1000);

	return buf_size;

}
#endif


void * rtmp_send_thread_func(void* parg)
{
	ContextSender* p = (ContextSender* )parg;
    AVPacket pkt = { 0 };
	int serial;
    int first_i = 0;

	int ret;
	while(1)
	{
		if(p->status == -1)
			break;
		ret = packet_queue_get(p->sendQueue, &pkt, 1, &serial);
		if(ret < 0)
			break;
		//according to pkt type, write audio and video seperately


		//TODO if not ffmpeg generated pts , no need for time base conversion.
        if (p->audio_only_mode) {
            int64_t pkt_dts = pkt.dts * 1000;//av_rescale_q(pkt.dts, p->fmt_ctx->streams[pkt.stream_index]->time_base, AV_TIME_BASE_Q);
            ret = rtmp_sender_write_audio_frame(p->muxer, pkt.data, pkt.size, pkt_dts);
        } else {
            int64_t pkt_dts;
            if (pkt.stream_index == 0) {
                while (p->adts->dtss[p->adts->num] == -1) {
                    if(p->status == -1)
                        return NULL;
                    usleep(10000);
                }
            
                printf("p->adts->dtss[p->adts->num] = %f\n", p->adts->dtss[p->adts->num]);
            
                p->audio_dts += p->adts->dtss[p->adts->num];
                p->audio_num++;
                if(p->audio_num > 10 && p->video_num > 0) {
                    p->vdts->dtss[p->vdts->num] = -1;
                    p->vdts->num = p->vdts->num == 99 ? 0 : p->vdts->num + 1;
                    p->video_num = 0;
                }
            
                if(p->adts->dtss[p->adts->num] == 0 && p->audio_num > 1) {
                    ret = 0;
                    av_free_packet(&pkt);
                    continue;
                }
                pkt_dts= pkt.dts * 1000;//av_rescale_q(pkt.dts, p->fmt_ctx->streams[pkt.stream_index]->time_base,       AV_TIME_BASE_Q);
                ret = rtmp_sender_write_audio_frame(p->muxer, pkt.data, pkt.size, p->audio_dts * 1000);
                printf("----------------- write audio frame  %lld--------------------\n ", av_gettime());
            } else {
                pkt_dts= pkt.dts;
                while (p->vdts->dtss[p->vdts->num] == -1) {
                    if (p->status == -1) {
                        return NULL;
                    }
                    usleep(10000);
                }
                p->video_dts += p->vdts->dtss[p->vdts->num];
                p->video_num++;
                if (p->audio_num > 10) {
                    p->adts->dtss[p->adts->num] = -1;
                    p->adts->num = p->adts->num == 99 ? 0 : p->adts->num + 1;
                    p->audio_num = 0;
                }
            
                if(p->vdts->dtss[p->vdts->num] == 0 && p->video_num > 1) {
                    ret = 0;
                    av_free_packet(&pkt);
                    continue;
                }
                printf("p->vdts->video_dts[p->adts->num] = %f\n", p->vdts->dtss[p->vdts->num]);
                
                if (p->qos) {
                    
                    if (((pkt.flags & AV_PKT_FLAG_KEY) != 0)
						|| (p->sendQueue->nb_packets < 1000)) {
                        if (((pkt.flags & AV_PKT_FLAG_KEY) != 0) && !first_i)
                            first_i = 1;
                        if (first_i) {
                            ret = rtmp_sender_write_video_frame(p->muxer, pkt.data,
                                                                pkt.size, p->video_dts * 1000,
                                                                pkt.flags & AV_PKT_FLAG_KEY);
                        }
                    } else {
                        first_i = 0;
                        ret = 0;
                    }
                    
                } else {
                    ret = rtmp_sender_write_video_frame(p->muxer, pkt.data,
                                                        pkt.size, p->video_dts * 1000,
                                                        pkt.flags & AV_PKT_FLAG_KEY);
                    if (ret != 0) {
                        //LOGE("rtmp_sender_write_video_frame error\n");
                    }
                }
                
            
//                ret = rtmp_sender_write_video_frame(p->muxer, pkt.data, pkt.size, p->video_dts * 1000, pkt.flags);
            
                printf("+++++++++++++++++++ write video frame %lf ++++++++++++++++++++s\n ", p->video_dts * 1000);
            }
        }
		//TODO write video packet

		av_free_packet(&pkt);

		if(ret != 0){
			p->status = 0x11;
			// TODO set error code and notify the app
			break; //
			//break;// send error
		}

	}

	return NULL;
}


void closeContext(ContextSender* p)
{
	if(checkStateChange(p, ID_SENDER_EVENT_SHUTDOWN) < 0)
		return;

	rtmp_sender_free(p->muxer);

	LOGI("close sendqueue");
	packet_queue_flush(p->sendQueue);
	packet_queue_destroy(p->sendQueue);

	LOGI("close srcqueue");

#if (defined ANDROID) || (defined WIN32)
	packet_queue_flush(p->srcQueue);
	packet_queue_destroy(p->srcQueue);
#endif

	p->status = -1;

#if (defined ANDROID) || (defined WIN32)
	av_free(p->srcQueue);
#endif
	av_free(p->sendQueue);
    
    if(p->vdts)
        av_free(p->vdts);
    
    if(p->adts)
        av_free(p->adts);
    
	av_free(p);

	doStateChange(p, ID_SENDER_EVENT_SHUTDOWN);

}

void setSourceContext(ContextSender* p, IAVSource * srcctx, int type)
{

	p->src_ctx = srcctx;
	p->src_type = type;

    #if (defined ANDROID) || (defined WIN32)
	if(type == 0) // source File, aac
	{
		// TODO
		p->func_process_data = 0;
	}

	if(type == 1)
		p->func_process_data = process_faac_thread_func;
    #endif
}

#if ANDROID
void enableAECM(ContextSender* p)
{
	p->needaecm = 1;
}
#endif

ContextSender* createContext()
{
	ContextSender* p = (ContextSender* )av_malloc(sizeof(ContextSender));
    
	p->internal_state = ID_SENDER_STATE_IDLE;

	if(checkStateChange(p, ID_SENDER_EVENT_CREATE) < 0){
		av_free(p);
		return 0;
	}

  av_register_all();

	p->sendQueue = (PacketQueue*)av_malloc(sizeof(PacketQueue));
	packet_queue_init(p->sendQueue);
	//packet_queue_start(p->sendQueue);

#if (defined ANDROID) || (defined WIN32)
	p->srcQueue = (PacketQueue*)av_malloc(sizeof(PacketQueue));
	packet_queue_init(p->srcQueue);
	//packet_queue_start(p->srcQueue);
#endif
    
    p->sendQueue->audio_pkt_num = 0;
    p->sendQueue->video_pkt_num = 0;
    
    p->vdts = (dts_list *)malloc(sizeof(dts_list));
    p->vdts->num = 0;
    p->vdts->size = 0;
    for (int i = 0; i < 100; i++) {
        p->vdts->dtss[i] = -1;
    }
    
    p->adts = (dts_list*)malloc(sizeof(dts_list));
    p->adts->num = 0;
    p->adts->size = 0;
    for (int i=0; i < 100; i++) {
        p->adts->dtss[i] = -1;
    }
    
    p->audio_num = 0;
    p->video_num = 0;
    
    p->audio_dts = 0;
    p->video_dts = 0;
    
    p->drop_head = 0;

	p->status = 0;
	p->signal_strength = 0;

	p->sender_opts = 0;

    #if (defined ANDROID) || (defined WIN32)
	p->sample_count = 0;
    p->needaecm = 0;

	p->swr_ctx = 0;
	p->aecmInst = 0;
	p->hEncoder = 0;
    #endif

	p->func_send_rtmp = rtmp_send_thread_func;



	p->func_error_callback = 0;
	p->func_status_callback = 0;
	p->func_usrcmd_error_callback = 0;
	p->func_usrcmd_status_callback = 0;

	doStateChange(p, ID_SENDER_EVENT_CREATE);

    temp_context = p;
	return p;
}

void setRtmpUrl(ContextSender* p,char * url_rtmp)
{
	strcpy(p->urlrtmp,url_rtmp);
}

int setLoginInfo(ContextSender* p, char * key, char * value)
{
	  if(p->sender_opts == NULL)
		  	  av_dict_set(&p->sender_opts, "connect", "1", 0);

		av_dict_set(&p->sender_opts, key, value, 0);
		return 0;
}
int setOption(ContextSender* p, char * key, char * value)
{
	  if(p->sender_opts == NULL)
		  	  av_dict_set(&p->sender_opts, "connect", "1", 0);

		av_dict_set(&p->sender_opts, key, value, 0);
		return 0;
}

void enable_qos(ContextSender* p)
{
    p->qos = 1;
}

void disable_qos(ContextSender* p)
{
    p->qos = 0;
}


int set_adts_extra_data(ContextSender *ctx,
							   uint8_t *buf)
{
	const int ADTS_HEADER_SIZE = 4;
	PutBitContext pb;

	int sample_rate_index = 4;
	int flv_sample_rate_index = 3;


	/*
	0: 96000 Hz
	1: 88200 Hz
	2: 64000 Hz
	3: 48000 Hz
	4: 44100 Hz
	5: 32000 Hz
	6: 24000 Hz
	7: 22050 Hz
	8: 16000 Hz
	9: 12000 Hz
	10: 11025 Hz
	11: 8000 Hz
	12: 7350 Hz
	13: Reserved
	14: Reserved
	15: frequency is written explictly
	*/

	switch(ctx->sample_rate)
	{
	case 96000:
		sample_rate_index = 0;
		break;
	case 88200:
		sample_rate_index = 1;
		break;
	case 64000:
		sample_rate_index = 2;
		break;
	case 44100:
		sample_rate_index = 4;
		flv_sample_rate_index = 3;
		break;
	case 22050:
		sample_rate_index = 7;
		flv_sample_rate_index = 2;
		break;
	case 16000:
		sample_rate_index = 8;
		break;
	case 11025:
		sample_rate_index = 10;
		flv_sample_rate_index = 1;
		break;
	case 8000:
		sample_rate_index = 11;
		break;
	default:
		sample_rate_index = 4;
		flv_sample_rate_index = 3;
		break;
	}


	init_put_bits(&pb, buf, ADTS_HEADER_SIZE);


	/* adts_fixed_header */
	put_bits(&pb, 4, 0xa);   /* aac format*/
	put_bits(&pb, 2, flv_sample_rate_index);        /* sample rate */
	put_bits(&pb, 1, 1);        /* bits per sample */
	put_bits(&pb, 1, !!(ctx->channels));        /* stereo */
	put_bits(&pb, 8, 0); /* aac sequence header*/


	put_bits(&pb, 5, 2);
	put_bits(&pb, 4, sample_rate_index);        /* private_bit */
	put_bits(&pb, 4, ctx->channels); /* channel_configuration */
	put_bits(&pb, 1, 0);        /* original_copy */
	put_bits(&pb, 1, 0);        /* home */
	put_bits(&pb, 1, 0);        /* home */

	flush_put_bits(&pb);

	ctx->extra_data_size = 4;

	return 0;
}

void setAudioSourceInfo(ContextSender* p, int smp_rate,
                                int ch,
                                int bits_per_coded_sample)
{
	p->channels = ch;
	p->sample_rate = smp_rate;
	p->bits_per_coded_sample = bits_per_coded_sample;

	set_adts_extra_data(p,p->adts_extra_data);

	LOGI("channel = %d, samplerate = %d, bitspersample=%d, %x,%x", ch, smp_rate, bits_per_coded_sample,
			p->adts_extra_data[2],p->adts_extra_data[3]);
}

void set_video_quality(ContextSender* p,int videoQuality)
{
    p->video_quality = videoQuality;
    if (videoQuality == VIDEO_QUALITY_LOW) {
        p->video_width = 320;
        p->video_height = 240;
    } else if(videoQuality == VIDEO_QUALITY_MEDIUM) {
        p->video_width = 640;
        p->video_height = 480;
    } else if(videoQuality == VIDEO_QUALITY_HIGH) {
        p->video_width = 1280;
        p->video_height = 720;
    }
}

void set_audio_only_mode(ContextSender *p)
{
    p->audio_only_mode = true;
}

int sendUserCommand(ContextSender* p, const char* method, const char *parma)
{

	if(checkStateChange(p, ID_SENDER_EVENT_SEND_CMD) < 0){
		return -1;
	}

	int ret= rtmp_sender_send_user_cmd(p->muxer,method,parma);

	doStateChange(p, ID_SENDER_EVENT_SEND_CMD);

	return ret;
}

void setErrorCallback(ContextSender* p, RTMPErrorInvoke errorcallback)
{

	p->func_error_callback = errorcallback;
}

void setStatueCallback(ContextSender* p, RTMPStatueInvoke statuecallback)
{
	p->func_status_callback = statuecallback;
}

void set_video_extra_data(unsigned char* video_extra_data, int video_extra_data_length)
{
    s_video_extra_data = (unsigned char*)malloc(video_extra_data_length);
    memcpy(s_video_extra_data, video_extra_data, video_extra_data_length);
    s_video_extra_data_length = video_extra_data_length;
}

static void start_publish(ContextSender* p)
{


			p->status = 0;
    rtmp_sender_set_audio_params(p->muxer, FLV_CODECID_AAC,
            p->channels, p->sample_rate,
            60, -1,
            2,
			p->adts_extra_data + 2,
			AAC_FORMAT_ISO);
    
    if (!p->audio_only_mode) {
        rtmp_sender_set_video_params(p->muxer, FLV_CODECID_H264,
                                 p->video_width, p->video_height, 0,
                                 25,
                                 1,
                                 s_video_extra_data_length, s_video_extra_data, AVC_FORMAT_ISO);
    }


	if(0 !=  rtmp_sender_start_publish(p->muxer, 0, av_gettime())){
		av_log(p,AV_LOG_ERROR, "rtmp sender start publish failed!\n");
		if(p->func_error_callback)
			p->func_error_callback(ID_SENDER_ERROR_CONNECT_FAIL_PUB);
		return;
	}

	//TODO
#if (defined ANDROID) || (defined WIN32)
	init_faac(p);
#endif

	// start a working thread
#ifdef WIN32
	p->tid_send = *pthread_create(p->func_send_rtmp, p);
#else
	pthread_create(&(p->tid_send), NULL, p->func_send_rtmp, p);
#endif

	// TODO
#if (defined ANDROID) || (defined WIN32)
	if(p->func_process_data)
#ifdef WIN32
	    p->tid_process = *pthread_create(p->func_process_data, p);
#elif ANDROID
    pthread_create(&(p->tid_process), NULL, p->func_process_data, p);
#endif
#endif

}

void* start_send_video(void* server_addr)
{
    
    struct sockaddr_in * m_server_addr = (struct sockaddr_in *)(server_addr);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);//创建socket
    if (sockfd < 0) {
        printf("create server socket failed");
        return NULL;
    }
    
    if (connect(sockfd, (struct sockaddr *)m_server_addr, sizeof(struct sockaddr)) < 0) {
        printf("server client connect failed");
        return NULL;
    }
    
    AVPacket pkt;
    AVFormatContext *fmt_ctx = NULL;
    
	unsigned char * buffer = (unsigned char*)av_malloc(4096);//500*1024);//
	AVIOContext* pb = avio_alloc_context(buffer, 4096,//500*1024,//
                                         0, NULL, read_data, NULL, NULL);
    
	
	fmt_ctx = avformat_alloc_context();
	fmt_ctx->pb = pb;
    
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, "", NULL, NULL) < 0) {
        //        fprintf(stderr, "Could not open source input %s\n", src_filename);
        //        exit(1);
        printf("avformat_open_input failed");
        return NULL;
    }
    
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        //        exit(1);
        return NULL;
    }
    //    av_dump_format(fmt_ctx, 0, src_filename, 0);
    
    start_publish(temp_context);
    
    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    do{
        
		int64_t pkt_dts = -40*1000;
        
        while (av_read_frame(fmt_ctx, &pkt) >= 0){
            if(pkt.stream_index < 0 || pkt.stream_index >= fmt_ctx->nb_streams){
                fprintf(stderr, "[pkt.stream_index = %d] illegal!\n", pkt.stream_index);
                return NULL;
            }
            
            int ret = 0;
			pkt_dts += 40000;
            
            
            if(fmt_ctx->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
//                fprintf(stderr, "audio pkt [size:%d][timebase:%lld/%lld][dts:%lld][flags:0x%x]!\n",
//                        pkt.size, fmt_ctx->streams[pkt.stream_index]->time_base.num,
//                        fmt_ctx->streams[pkt.stream_index]->time_base.den, pkt.dts, pkt.flags);
                //                ret = rtmp_sender_write_audio_frame(muxer, pkt.data, pkt.size, pkt_dts);
            } else if(fmt_ctx->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
//                fprintf(stderr, "video pkt [size:%d][timebase:%lld/%lld][dts:%lld][flags:0x%x]!\n",
//                        pkt.size, fmt_ctx->streams[pkt.stream_index]->time_base.num,
//                        fmt_ctx->streams[pkt.stream_index]->time_base.den, pkt.dts, pkt.flags);
                //                ret = rtmp_sender_write_video_frame(temp_context->muxer, pkt.data, pkt.size,
                //                                                    pkt_dts, pkt.flags & AV_PKT_FLAG_KEY);
                
                //
                ret = pkt_h264_to_sender(temp_context, pkt.data, pkt.size, pkt_dts, pkt.flags & AV_PKT_FLAG_KEY);
            }
            
            av_free_packet(&pkt);
            
            
            if(ret != 0){
                fprintf(stderr, "stream%d: write frame failed!\n", pkt.stream_index);
                return NULL;
            }
        }
        
        av_seek_frame(fmt_ctx, -1, 0, AVSEEK_FLAG_ANY);
    }while(1);
    
    close(sockfd);
    
    
FAIL:
    if(fmt_ctx){
        avformat_close_input(&fmt_ctx);
    }
    //    if(muxer){
    //        rtmp_sender_free(muxer);
    //    }
    close(sockfd);
    return NULL;
}


static int stop_publish(ContextSender* p)
{

	if (p) {
		p->status = -1;

		LOGI("abort srcqueue");

        #if (defined ANDROID) || (defined WIN32)
		packet_queue_flush(p->srcQueue);
		packet_queue_abort(p->srcQueue);
        #endif

		LOGI("abort sendqueue");
		packet_queue_flush(p->sendQueue);
		packet_queue_abort(p->sendQueue);



#ifdef WIN32
		pthread_join(&(p->tid_send), NULL);
#else
		if(p->tid_send)
			pthread_join(p->tid_send, NULL);
#endif

#if (defined ANDROID) || (defined WIN32)
		if(p->func_process_data)
#ifdef WIN32
		    pthread_join(&(p->tid_process), NULL);
#else
		    pthread_join(p->tid_process, NULL);
#endif
#endif


		if(0 != rtmp_sender_stop_publish(p->muxer)){
			av_log(p, AV_LOG_ERROR, "rtmp sender stop publish failed!\n");
			return -1;
		}
		LOGI("after stop publish");
	}

	return 0;

}

void setUserCommandErrorCallback(ContextSender* p, RTMPUserCommandErrorInvoke errorcallback)
{
	p->func_usrcmd_error_callback = errorcallback;

}

void setUserCommandResultCallback(ContextSender* p, RTMPUserCommandResultInvoke statuecallback)
{
	p->func_usrcmd_status_callback = statuecallback;

}

int connect(ContextSender* p)
{

	if(checkStateChange(p, ID_SENDER_EVENT_CONNECT) < 0){
		return -1;
	}

 	p->muxer = rtmp_sender_alloc(p->urlrtmp);

	if(p->muxer == NULL){
		fprintf(stderr, "please call setRtmpURL first\n");

	}

   rtmp_sender_set_login_info(p->muxer, p->sender_opts);

	rtmp_sender_set_error_invoke(p->muxer,p->func_error_callback);
	rtmp_sender_set_statue_invoke(p->muxer,p->func_status_callback);
	rtmp_sender_set_userCommand_error_invoke(p->muxer,p->func_usrcmd_error_callback);
	rtmp_sender_set_userCommand_result_invoke(p->muxer,p->func_usrcmd_status_callback);


    if(rtmp_sender_prepare(p->muxer, 0) < 0){
		if(p->func_error_callback != NULL)
    		p->func_error_callback(ID_SENDER_ERROR_CONNECT_FAIL_PREPARE);
//		av_log(p,AV_LOG_ERROR, "rtmp sender prepare failed!\n");
		LOGE("rtmp sender prepare failed!");
        return -1;
    }

  doStateChange(p, ID_SENDER_EVENT_CONNECT);
	return 0;
}


int close(ContextSender* p)
{
	if(checkStateChange(p, ID_SENDER_EVENT_CLOSE) < 0){
		return -1;
	}

	rtmp_sender_free(p->muxer);
	p->muxer = 0;

	doStateChange(p, ID_SENDER_EVENT_CLOSE);

	return 0;
}


int start(ContextSender* p)
{
	if(checkStateChange(p, ID_SENDER_EVENT_START) < 0){
		return -1;
	}

    if (p->audio_only_mode) {
        start_publish(p);
    }

    #if (defined ANDROID) || (defined WIN32)
	if(p->src_ctx)
	{
		p->src_ctx->start();
	}
    # endif

	doStateChange(p, ID_SENDER_EVENT_START);
	return 0;
}

int pause(ContextSender* p)
{
	if(checkStateChange(p, ID_SENDER_EVENT_PAUSE) < 0){
		return -1;
	}

    #if (defined ANDROID) || (defined WIN32)
        p->src_ctx->stop();
    #endif

	doStateChange(p, ID_SENDER_EVENT_PAUSE);

	return 0;
}

int resume(ContextSender* p)
{
	if(checkStateChange(p, ID_SENDER_EVENT_RESUME) < 0){
		return -1;
	}

    #if (defined ANDROID) || (defined WIN32)
        p->src_ctx->start();
    #endif

	doStateChange(p, ID_SENDER_EVENT_RESUME);
	return 0;
}

int stop(ContextSender* p)
{
	if(checkStateChange(p, ID_SENDER_EVENT_STOP) < 0){
		return -1;
	}
    #if (defined ANDROID) || (defined WIN32)
	if(p->src_ctx)
	{
		p->src_ctx->stop();
	}
    #endif

	stop_publish(p);

	doStateChange(p, ID_SENDER_EVENT_STOP);
	return 0;
}

int  querySignalStrength(ContextSender* p){
	return p->signal_strength;
}


#if (defined ANDROID) || (defined WIN32)
////////////////////////////////////////
/// resample function
/////////////////////////////////////////


int init_resample(ContextSender* p, int src_rate, int dst_rate, int src_channels, int dst_channels, int src_samples)
{
	enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_S16, dst_sample_fmt = AV_SAMPLE_FMT_S16;
	int64_t src_ch_layout = src_channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	int64_t dst_ch_layout = dst_channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	int ret;

    /* create resampler context */
    p->swr_ctx = swr_alloc();
    if (!p->swr_ctx) {
    	ret = -1;
        LOGE("Could not allocate resampler context\n");
        goto end;
    }

    /* set options */
    av_opt_set_int(p->swr_ctx, "in_channel_layout",    src_ch_layout, 0);
    av_opt_set_int(p->swr_ctx, "in_sample_rate",       src_rate, 0);
    av_opt_set_sample_fmt(p->swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

    av_opt_set_int(p->swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
    av_opt_set_int(p->swr_ctx, "out_sample_rate",       dst_rate, 0);
    av_opt_set_sample_fmt(p->swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(p->swr_ctx)) < 0) {
    	LOGE("Failed to initialize the resampling context\n");
        goto end;
    }

    /* allocate source and destination samples buffers */
    ret = av_samples_alloc_array_and_samples(&p->src_data, &p->src_linesize, src_channels, src_samples, src_sample_fmt, 0);
    if (ret < 0) {
    	LOGE("Could not allocate source samples\n");
        goto end;
    }

    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    p->max_dst_nb_samples = av_rescale_rnd(src_samples, dst_rate, src_rate, AV_ROUND_UP);

    /* buffer is going to be directly written to a rawaudio file, no alignment */
    ret = av_samples_alloc_array_and_samples(&p->dst_data, &p->dst_linesize, dst_channels, p->max_dst_nb_samples, dst_sample_fmt, 0);
    if (ret < 0) {
    	LOGE("Could not allocate destination samples\n");
        goto end;
    }

end:
    return ret;
}

int process_resample(ContextSender* p, int src_rate, int dst_rate, int src_channels, int dst_channels, int src_samples, uint8_t *src_data, int *dst_samples, uint8_t *dst_data)
{
    enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16;
    int dst_nb_samples;
    int ret;

    if(!p->swr_ctx)
    	init_resample(p, src_rate, dst_rate, src_channels, dst_channels, src_samples);

    memcpy(p->src_data[0], src_data, src_samples * src_channels * 2);

    /* compute destination number of samples */
    dst_nb_samples = av_rescale_rnd(swr_get_delay(p->swr_ctx, src_rate) + src_samples, dst_rate, src_rate, AV_ROUND_UP);
    if (dst_nb_samples > p->max_dst_nb_samples) {
        ret = av_samples_alloc(p->dst_data, &p->dst_linesize, dst_channels, dst_nb_samples, dst_sample_fmt, 1);
        if (ret < 0)
        	goto end;
        p->max_dst_nb_samples = dst_nb_samples;
    }

    /* convert to destination format */
    ret = swr_convert(p->swr_ctx, p->dst_data, dst_nb_samples, (const uint8_t **)p->src_data, src_samples);
    if (ret < 0) {
        LOGE("Error while converting\n");
        goto end;
    }

    memcpy(dst_data, p->dst_data[0], dst_nb_samples * dst_channels * 2);
    *dst_samples = dst_nb_samples;
    LOGI("in:%d out:%d\n", src_samples, ret);

end:
    return ret;
}


void close_resample(ContextSender* p)
{
    if (p->src_data)
        av_freep(&p->src_data[0]);
    av_freep(&p->src_data);

    if (p->dst_data)
        av_freep(&p->dst_data[0]);
    av_freep(&p->dst_data);

    swr_free(&p->swr_ctx);
}

////////////////////////////////////////
/// aecm function
/////////////////////////////////////////

int init_aecm(ContextSender* p)
{
	int ret;
	int nSampleRate = p->sample_rate;
	if(nSampleRate != 16000 && nSampleRate != 8000) {
		nSampleRate = 16000;
	}
	ret = WebRtcAecm_Create(&(p->aecmInst));
	if(ret < 0) {
		LOGE("[ERROR] Failed to call WebRtcAecm_Create()\n");
		return -1;
	}
	ret = WebRtcAecm_Init(p->aecmInst,nSampleRate);
	if(ret < 0) {
		LOGE("[ERROR] Failed to call WebRtcAecm_Init()\n");
		return -1;
	}
	return 0;
}

void process_aecm(ContextSender* p, uint8_t * data, int size)
{
	int bufcount = 0;
	int dst_samples = 0;

	int farsize = 0;
	int nearsize = 0;
	int processsize = 0;

	int ret = 0;

	if(p->audiosl->nextBuffer) {
		bufcount = p->audiosl->bufcount;
		int temp = (clock() - p->audiosl->playerstart_time) * 1000 / CLOCKS_PER_SEC;
retry:
		if(temp > 0 && p->audiosl->nextBuffer) {
			if(temp <= p->audiosl->nextSize / (p->audiosl->player_samplesrate * p->audiosl->player_channels * 2 / 1000)) {
				usleep((p->audiosl->nextSize / (p->audiosl->player_samplesrate * p->audiosl->player_channels * 2 / 1000) - temp) * 1000);
			} else if(bufcount > 0){
				bufcount--;
				temp -= p->audiosl->nextSize / (p->audiosl->player_samplesrate * p->audiosl->player_channels * 2 / 1000);
				goto retry;
			} else {
				temp -= p->audiosl->nextSize / (p->audiosl->player_samplesrate * p->audiosl->player_channels * 2 / 1000);
				usleep(p->audiosl->nextSize / (p->audiosl->player_samplesrate * p->audiosl->player_channels * 2 / 1000) * 1000);
				goto retry;
			}
		}


		nearsize = size;
		LOGI("nearsize:%d",nearsize);
		if(p->audiosl->player_samplesrate != 16000 && p->audiosl->player_channels != 1) {
			bufcount = p->audiosl->bufcount;
			while(farsize < nearsize && p->audiosl->nextBuffer) {
				if(bufcount <= p->audiosl->bufcount) {
					bufcount++;
					process_resample(p, p->audiosl->player_samplesrate, 16000, p->audiosl->player_channels, 1,
							p->audiosl->nextSize / p->audiosl->player_channels / 2,
							(uint8_t *)p->audiosl->nextBuffer, &dst_samples, (uint8_t *)p->audiobuf);
					farsize += dst_samples * 2;
				}
			}
		} else if(p->audiosl->nextBuffer) {
			memcpy(p->audiobuf, p->audiosl->nextBuffer, p->audiosl->nextSize);
			farsize = p->audiosl->nextSize;
		}
		LOGI("farsize:%d",farsize);


		while(farsize >= 160 * 2 && nearsize >= 160 * 2 && p->audiosl->nextBuffer) {
			ret = WebRtcAecm_BufferFarend(p->aecmInst, (const int16_t*)(p->audiobuf + processsize), 160);
			if(ret < 0)
				LOGE("[ERROR] Failed to call WebRtcAecm_BufferFarend()\n");
			else
				LOGI("[OK] OK to call WebRtcAecm_BufferFarend()\n");
			ret = WebRtcAecm_Process(p->aecmInst, (const int16_t*)(data + processsize), NULL, p->aecmbuf + processsize / 2, 160, 10);
			if(ret < 0)
				LOGE("[ERROR] Failed to call WebRtcAecm_Process()\n");
			else
				LOGI("[OK] OK to call WebRtcAecm_Process()\n");
			farsize -= 160 * 2;
			nearsize -= 160 * 2;
			processsize += 160 * 2;
		}
		farsize = nearsize = processsize = 0;
	} else
		memcpy(p->aecmbuf,data,size);
}

int close_aecm(ContextSender* p)
{
	int ret = WebRtcAecm_Free(p->aecmInst);
	if(ret < 0) {
		LOGE("[ERROR] Failed to call WebRtcAecm_Free()\n");
	}
	return ret;
}


////////////////////////////////////////
/// ffaac function
/////////////////////////////////////////

static int init_faac(ContextSender* p)
{


	int nSampleRate = p->sample_rate;  // ������
	int nChannels = p->channels;         // �����
	int nPCMBitSize = p->bits_per_coded_sample;      // ����λ��
	unsigned long nInputSamples = 0;
	p->sample_count = 0;

	p->extra_data_size = 4;

	unsigned long  nMaxOutputBytes;


	p->hEncoder = faacEncOpen(nSampleRate, nChannels, &nInputSamples, &nMaxOutputBytes);
	if(p->hEncoder == NULL)
	{
		printf("[ERROR] Failed to call faacEncOpen()\n");
		return -1;
	}

	LOGI("sr=%d, ch=%d,inputsample=%d, maxoutput=%d", p->sample_rate, p->channels, nInputSamples, nMaxOutputBytes);

	p->nPCMBufferSize = nInputSamples * nPCMBitSize / 8;
	p->pbAACBuffer = av_malloc(nMaxOutputBytes);
	p->nMaxOutpuBytes = nMaxOutputBytes;

	p->pConfiguration = faacEncGetCurrentConfiguration(p->hEncoder);
	//if(p->bits_per_coded_sample == 16)
		p->pConfiguration->inputFormat = FAAC_INPUT_16BIT;
		p->pConfiguration->aacObjectType = LOW;
		p->pConfiguration->outputFormat = 0;
		p->pConfiguration->quantqual = 100;
		p->pConfiguration->bitRate = 32000;
	//else
	//	p->pConfiguration->inputFormat = FAAC_INPUT_8BIT;

	int nRet = faacEncSetConfiguration(p->hEncoder, p->pConfiguration);

	return nRet;

}


int close_faac(ContextSender* p)
{

	int ret = faacEncClose(p->hEncoder);
	//TODO error check
	av_free(p->pbAACBuffer);

	return ret;
}


void * process_faac_thread_func(void* parg)
{
	ContextSender* p = (ContextSender* )parg;
    AVPacket pkt = { 0 };
	int serial;


	int ret;
	int nInputSamples;


	int nMaxOutputBytes = p->nMaxOutpuBytes;

	if(p->needaecm) {
		init_aecm(p);
		p->audiosl = AudioSL::GetInstance();
	}

	while(1)
	{
		ret = packet_queue_get(p->srcQueue, &pkt, 1, &serial);
		if(ret < 0) // force break
			break;

		nInputSamples = pkt.size/(p->bits_per_coded_sample/8);

		int svalue[3];
		short* pSample = (short*)pkt.data;
		svalue[0] = pSample[0];
		svalue[1] = pSample[nInputSamples/2];
		svalue[2] = pSample[nInputSamples -1];

		int diff = abs(svalue[0] - svalue[1]) + abs(svalue[1] - svalue[2]);

		p->signal_strength = (diff >> 7);

		if(p->needaecm) {
			process_aecm(p, pkt.data, pkt.size);
		}

		ret = faacEncEncode( p->hEncoder, p->needaecm == 1 ? (int *) p->aecmbuf : (int*) pkt.data, nInputSamples, (uint8_t*)p->pbAACBuffer, nMaxOutputBytes);

		if(ret == 0){
			p->sample_count += nInputSamples;
			continue;
		}

		//TODO error check: set error status in the context

		av_free_packet(&pkt);
		pkt.data = (uint8_t*)p->pbAACBuffer;
		pkt.size = ret;

		//TODO calculate dts according to samples
		p->sample_count += nInputSamples;
		pkt.dts = p->sample_count*1000/p->sample_rate/p->channels;


		packet_queue_put(p->sendQueue,&pkt);
	}



	if(p->hEncoder)
		close_faac(p);

	if(p->swr_ctx)
		close_resample(p);

	if(p->aecmInst)
		close_aecm(p);


	return NULL;
}

#endif



