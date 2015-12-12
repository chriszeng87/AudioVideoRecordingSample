#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"

#ifdef WIN32

#define inline __inline
#define isnan(x) ((x)!=(x))
#include "inttypes.h"
#include "w32pthreads.h"
#define SDL_Delay(x) Sleep(x)
#else
#include "pthread.h"
#endif

#include "packet_queue.h"



static AVPacket flush_pkt;
//static AVPacket codec_pkt;
static int init_flush = 0;


void init_flush_pkt(AVPacket** ppkt)
{
	if(init_flush > 0)
		*ppkt = &flush_pkt;

	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t *)&flush_pkt;
	init_flush = 1;
	*ppkt = &flush_pkt;
}

void init_codec_pkt(AVPacket** ppkt)
{
	if(init_flush > 0)
		*ppkt = &flush_pkt;

	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t *)&flush_pkt;
	init_flush = 1;
	*ppkt = &flush_pkt;
}

int is_flush_pkt(AVPacket *pkt)
{
	if(pkt->data == flush_pkt.data)
		return 1;
	else
		return 0;
}

static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
	MyAVPacketList *pkt1;

	if (q->abort_request)
		return -1;

	pkt1 = av_malloc(sizeof(MyAVPacketList));
	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;
	if (is_flush_pkt(pkt))
		q->serial++;
	pkt1->serial = q->serial;

	if (!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size + sizeof(*pkt1);
	/* XXX: should duplicate packet data in DV case */
	pthread_cond_signal(q->cond);
	return 0;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	int ret;

	/* duplicate the packet */
	if (!is_flush_pkt(pkt) && av_dup_packet(pkt) < 0)
		return -1;

	pthread_mutex_lock(q->mutex);
	ret = packet_queue_put_private(q, pkt);
	pthread_mutex_unlock(q->mutex);

	//if (!is_flush_pkt(pkt) && ret < 0)
		//av_free_packet(pkt);

	return ret;
}

/* packet queue handling */
void packet_queue_init(PacketQueue *q)
{
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = malloc(sizeof(pthread_mutex_t));
	q->cond = malloc(sizeof(pthread_cond_t));
	pthread_mutex_init(q->mutex,PTHREAD_MUTEX_NORMAL);//(pthread_mutex_t*) av_malloc(sizeof(pthread_mutex_t));
	pthread_cond_init(q->cond,PTHREAD_MUTEX_NORMAL);//SDL_CreateCond();

	q->abort_request = 0;
}

void packet_queue_flush(PacketQueue *q)
{
	MyAVPacketList *pkt, *pkt1;

	pthread_mutex_lock(q->mutex);
	for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	pthread_mutex_unlock(q->mutex);
}

void packet_queue_destroy(PacketQueue *q)
{
	packet_queue_flush(q);
	pthread_mutex_destroy(q->mutex);
	pthread_cond_destroy(q->cond);
}

void packet_queue_abort(PacketQueue *q)
{
	pthread_mutex_lock(q->mutex);

	q->abort_request = 1;

	pthread_cond_signal(q->cond);

	pthread_mutex_unlock(q->mutex);
}

void packet_queue_start(PacketQueue *q)
{
	pthread_mutex_lock(q->mutex);
	q->abort_request = 0;
	packet_queue_put_private(q, &flush_pkt);
	pthread_mutex_unlock(q->mutex);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
	MyAVPacketList *pkt1;
	int ret;

	pthread_mutex_lock(q->mutex);

	for (;;) 
	{
		if (q->abort_request) 
		{
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1) 
		{
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size + sizeof(*pkt1);
			*pkt = pkt1->pkt;
			if (serial)
				*serial = pkt1->serial;
			av_free(pkt1);
			ret = 1;
			break;
		} 
		else if (!block) 
		{
			ret = 0;
			break;
		} 
		else 
		{
			//SDL_CondWait(q->cond, q->mutex);
			pthread_cond_wait(q->cond, q->mutex);
		}
	}
	pthread_mutex_unlock(q->mutex);
	return ret;
}
