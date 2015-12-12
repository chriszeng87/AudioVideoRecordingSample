#ifndef __CBPLAYER_QUEUE_H__
#define __CBPLAYER_QUEUE_H__

typedef struct MyAVPacketList {
	AVPacket pkt;
	struct MyAVPacketList *next;
	int serial;
} MyAVPacketList;


typedef struct PacketQueue {
	MyAVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	int abort_request;
	int serial;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
    int audio_pkt_num;
    int video_pkt_num;
} PacketQueue;



int packet_queue_put(PacketQueue *q, AVPacket *pkt);
/* packet queue handling */
void packet_queue_init(PacketQueue *q);
void packet_queue_flush(PacketQueue *q);

void packet_queue_destroy(PacketQueue *q);

void packet_queue_abort(PacketQueue *q);
void packet_queue_start(PacketQueue *q);

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);

void init_flush_pkt(AVPacket** ppkt);


#endif
