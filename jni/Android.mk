LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := ffmpeg
LOCAL_SRC_FILES := $(LOCAL_PATH)/ffmpeg/libffmpeg.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := x264
LOCAL_SRC_FILES := $(LOCAL_PATH)/ffmpeg/libx264.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS) 
LOCAL_MODULE    := publisher


FFMPEG_INCLUDE_PATH := $(LOCAL_PATH)/ffmpeg/include

LOCAL_CFLAGS := -D__STDC_CONSTANT_MACROS

LOCAL_C_INCLUDES += $(FFMPEG_INCLUDE_PATH)

LOCAL_C_INCLUDES += $(LOCAL_PATH)

#LOCAL_LDLIBS += $(LOCAL_PATH)/ffmpeg/libffmpeg.a


                   
LOCAL_SRC_FILES :=  ffmpeg.c\
                    cmdutils.c\
                    ffmpeg_opt.c\
                    ffmpeg_filter.c\
                    com_chris_video_RTMPPublisher.cpp \

                   

LOCAL_STATIC_LIBRARIES :=  ffmpeg x264


# for logging
LOCAL_LDLIBS    += -llog -lz

include $(BUILD_SHARED_LIBRARY)
$(call import-module,android/cpufeatures)