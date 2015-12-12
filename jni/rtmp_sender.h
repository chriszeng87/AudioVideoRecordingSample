

#ifndef RTMP_SENDER_H
#define RTMP_SENDER_H
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

struct AVDictionary;
// @brief alloc function
// @param [in] url     : RTMP URL, rtmp://127.0.0.1/live/xxx
// @return             : rtmp_sender hadler
void *rtmp_sender_alloc(const char *url);


int rtmp_sender_set_login_info(void * handle, AVDictionary* opts);

#define FLV_AUDIO_CODECID_OFFSET     4
enum {
    FLV_CODECID_PCM                  = 0,
    FLV_CODECID_ADPCM                = 1 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_MP3                  = 2 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_LE               = 3 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER_16KHZ_MONO = 4 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER_8KHZ_MONO = 5 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER           = 6 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_ALAW             = 7 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_MULAW            = 8 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_AAC                  = 10<< FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_SPEEX                = 11<< FLV_AUDIO_CODECID_OFFSET,
};


#define AAC_FORMAT_ADTS 0
#define AAC_FORMAT_ISO  1
// @brief set audio params
// @param [in] rtmp_sender handler
// @param [in] codec   : audio codec name, libfaac
// @param [in] ch      : audio channel num, [1, 2]
// @param [in] smp_rate: audio sample rate
// @param [in] bit_rate: audio bitrate
// @param [in] bits_per_coded_sample : reserved param, set to -1.
// @param [in] aac_extra_size: audio extradata size
// @param [in] aac_extra_data: AAC AudioSpecificConfig, see ISO 14496-3. 
//                         Note that this is not the same as the contents 
//                         of the esds box from an MP4/F4V file.
// @param [in] aac_fmt: audio packet format[AAC_FORMAT_ADTS, AAC_FORMAT_ISO]
// @return             : 0: OK; others: FAILED
int rtmp_sender_set_audio_params(void *handle,  
        int codec,
        int ch,
        int smp_rate,
        int bit_rate,
        int bits_per_coded_sample, 
        int aac_extra_size,
        const uint8_t *aac_extra_data,
        int aac_fmt);


enum {
    FLV_CODECID_H263    = 2,
    FLV_CODECID_SCREEN  = 3,
    FLV_CODECID_VP6     = 4,
    FLV_CODECID_VP6A    = 5,
    FLV_CODECID_SCREEN2 = 6,
    FLV_CODECID_H264    = 7,
    FLV_CODECID_REALH263= 8,
    FLV_CODECID_MPEG4   = 9,
};

#define AVC_FORMAT_APPENDB 0
#define AVC_FORMAT_ISO     1
// @brief set audio params
// @param [in] rtmp_sender handler
// @param [in] codec   : video codec name, libx264
// @param [in] width   : video width
// @param [in] height  : video height
// @param [in] bit_rate: video bitrate
// @param [in] frame_rate_num: numerator of video frame rate
// @param [in] frame_rate_den: denominator of video frame rate
// @param [in] extra_size: video extradata size
// @param [in] extra_data: [choice 1] AVCDecoderConfigurationRecord
//                         [choice 2] SPS+PPS
// @param [in] video_pkt_fmt: video packet format
//                         [AVC_FORMAT_APPENDB]
//                         [AVC_FORMAT_ISO]
// @return             : 0: OK; others: FAILED
int rtmp_sender_set_video_params(void *handle,
        int codec,
        int32_t width,
        int32_t height,
        int32_t bitrate,
        int frame_rate_num, int frame_rate_den,
        int extra_size,
        const uint8_t *extra_data,
        int video_pkt_fmt);

#define MKETAG(a,b,c,d) (-(int)((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24)))
#define RTMP_ERROR_TOKEN_EXPIRED MKETAG('T','O','K','N')
// @brief prepare
// @param [in] handle: rtmp_sender handler
// @param [in] debug:  whether to debug(1) or not(0)
// @return             : 0: OK; others: FAILED (see <errno.h> and MKETAGs)
int rtmp_sender_prepare(void *handle, int debug);


//typedef int (*RTMPUserInvoke) (void *cb_ctx, const char* cmd, const char *src, int len, char* dst, int dst_len);
// @brief set user invoke, when sdk receives the cmds sended by server or other client,
// @brief sdk call the callback to notify the app.
// @param [in] handle: rtmp_sender handler
// @param [in] cb    :  user callback, the param list is:
//                      cmd[in]:      user cmd name[startPublish, stopPublish, userCmd]
//                      src[out]:     src json string
//                      src_len[in]:  src json string length
//                      dst[out]:     dst json string
//                      dst_len[in]:  max dst json string length
// @param [in] data  : the first param [cb_ctx] of cb
// @return             : 0: OK; others: FAILED
//int rtmp_sender_set_user_invoke(void *handle, RTMPUserInvoke cb, void *data);
//
//typedef void (*RTMPErrorInvoke)(int err_code);
//int rtmp_sender_set_error_invoke(void* handle, RTMPErrorInvoke cb);
//
//typedef void (*RTMPStatueInvoke)(int statue);
//int rtmp_sender_set_statue_invoke(void* handle, RTMPStatueInvoke cb);
//
//typedef void (*RTMPUserCommandErrorInvoke)(char *data);
//int rtmp_sender_set_userCommand_error_invoke(void* handle, RTMPUserCommandErrorInvoke cb);
//
//typedef void (*RTMPUserCommandResultInvoke)(char *data);
//int rtmp_sender_set_userCommand_result_invoke(void* handle, RTMPUserCommandResultInvoke cb);
//
//
//#define RTMP_STREAM_PROPERTY_PUBLIC      0x00000001
//#define RTMP_STREAM_PROPERTY_ALARM       0x00000002
//#define RTMP_STREAM_PROPERTY_RECORD      0x00000004
// @brief start publish
// @param [in] rtmp_sender handler
// @param [in] flag        stream falg
// @param [in] ts_us       timestamp in us
// @return             : 0: OK; others: FAILED
int rtmp_sender_start_publish(void *handle, uint32_t flag, int64_t ts_us);

// @brief stop publish
// @param [in] rtmp_sender handler
// @return             : 0: OK; others: FAILED
int rtmp_sender_stop_publish(void *handle);

// @brief set stream property
// @param [in] rtmp_sender handler
// @param [in] user_data   user_data in json
//int rtmp_sender_send_user_cmd(void *handle, const char *method, const char *user_data);

// @brief get tcp/udp file discription
// @param [in] rtmp_sender handler
// @return             : >=0: OK; others: FAILED
int rtmp_sender_get_fd(void *handle);

// @brief send audio frame, now only AAC supported
// @param [in] rtmp_sender handler
// @param [in] data       : AACAUDIODATA
// @param [in] size       : AACAUDIODATA size
// @param [in] dts_us     : decode timestamp of frame
int rtmp_sender_write_audio_frame(void *handle,
        uint8_t *data,
        int size,
        uint64_t dts_us);

// @brief send video frame, now only H264 supported
// @param [in] rtmp_sender handler
// @param [in] data       : video data, (Full frames are required)
// @param [in] size       : video data size
// @param [in] dts_us     : decode timestamp of frame
// @param [in] key        : key frame indicate, [0: non key] [1: key]
int rtmp_sender_write_video_frame(void *handle,
        uint8_t *data,
        int size,
        uint64_t dts_us,
        int key);

// @brief free rtmp_sender handler
// @param [in] rtmp_sender handler
void rtmp_sender_free(void *handle);


#ifdef __cplusplus
}
#endif
#endif
