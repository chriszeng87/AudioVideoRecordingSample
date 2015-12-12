

#ifndef __LIVE_SENDER_H__
#define __LIVE_SENDER_H__


#include "live_log.h"


#define VIDEO_QUALITY_LOW 0
#define VIDEO_QUALITY_MEDIUM 1
#define VIDEO_QUALITY_HIGH 2



// pre-defined read2pkt functions
int pkt_aacadts_to_sender(void *opaque, void *buf, int buf_size, int pkt_type = 0);

int ios_pkt_aacadts_to_sender(void *opaque, void *buf, int buf_size, int type);
int pkt_pcm_to_sender(void *opaque, void *buf, int buf_size, int pkt_type = 0);
int pkt_h264_to_sender(void *opaque, void*buf, int buf_size, double dts, int key);
void* start_send_video(void* server_addr);


struct IAVSource;
struct ContextSender;


IAVSource* createAVSource(int src_type, int sub_type, char* url);
void closeAVSource(IAVSource* p);


ContextSender* createContext();
void closeContext(ContextSender* p);

void setRtmpUrl(ContextSender* p,char * url_rtmp);

void set_audio_only_mode(ContextSender *p);

void setAudioSourceInfo(ContextSender* p, int smp_rate,int ch, int bits_per_coded_sample = 16);
void setSourceContext(ContextSender* p,IAVSource * srcctx, int type = 0);

void set_video_extra_data(unsigned char* video_extra_data, int video_extra_data_length);


int connect(ContextSender* p);
int close(ContextSender* p);

int start(ContextSender* p);
int stop(ContextSender * p);

int pause(ContextSender * p);
int resume(ContextSender * p);


#endif
