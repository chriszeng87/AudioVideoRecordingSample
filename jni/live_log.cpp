#include "live_log.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#ifdef __cplusplus
}
#endif
#include <android/log.h>

//#define  LOG_TAG    "live_sender"

void log_callback_android(void *ptr, int level, const char *fmt, va_list vl)
{
    char line[512];
    vsnprintf(line, sizeof(line) - 1, fmt, vl);
    if(level == AV_LOG_QUIET || level == AV_LOG_PANIC ||
            level == AV_LOG_FATAL || level == AV_LOG_ERROR){
        LOGE("%s", line);
    }else if(level == AV_LOG_WARNING){
        LOGW("%s", line);
    }else if(level == AV_LOG_INFO){
        LOGI("%s", line);
    }else if(level == AV_LOG_VERBOSE){
        LOGV("%s", line);
    }else {
        LOGD("%s", line);
    }
}
