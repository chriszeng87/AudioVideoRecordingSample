//
// Created by Chris on 11/24/15.
//
#include "com_chris_video_RTMPPublisher.h"
#include<pthread.h>
#include <stdio.h>
extern "C" {
    #include "ffmpeg.h"
}

static int audio_pipe_id;
static int video_pipe_id;


//ffmpeg -re -i test2015.h264 -vcodec copy -f flv rtmp://***
void * start_publish(void* args)
{
//    int argc, char **argv
    int numberOfArgs = 15;
    char** arguments = (char**)calloc(numberOfArgs, sizeof(char*));

    arguments[0] = "ffmpeg";
    arguments[1] = "-re";
    arguments[2] = "-i";
    char vinput[20];
    memset(vinput, 0, sizeof(vinput));
    sprintf(vinput, "pipe:%d", video_pipe_id);
    arguments[3] = vinput;//"/sdcard/test2016.h264"; //pipe
//    arguments[4] = "-i";
//    char ainput[20];
//    memset(ainput, 0, sizeof(ainput));
//    sprintf(ainput, "pipe:%d", audio_pipe_id);
//    arguments[5] = ainput;//"/sdcard/test2016.aac"; //pipe
    arguments[6] = "-acodec";
    arguments[7] = "copy";
    arguments[8] = "-vcodec";
    arguments[9] = "copy";
    arguments[10] = "-f";
    arguments[11] = "flv";
//    arguments[12] = "-bsf:a";
//    arguments[13] = "aac_adtstoasc";
//    arguments[14] = "-bsf";
//    arguments[15] = "h264_mp4toannexb";
    arguments[14] = "/sdcard/test.h264";

    int result = ffmpeg_main(numberOfArgs, arguments);

//    ffmpeg_run(ses, ses->argc, ses->argv);
//    int ret = generateCmd();
//    if ( ret != 0) {
//        return ret;
//    }
//
//    pthread_mutex_lock(&ses->apiMux);
//
//    // open output
//    char * url = (char*)malloc(strlen( strUrl ) + strlen(streamkey) + 2);
//    sprintf(url, "%s/%s", strUrl, streamkey);
//    appendArgv(ses, url);
//    free(url);
//    url = NULL;
//
////    ses->argc += 1;
////    ses->exitOnFail = 0;
//
//    /* parse options and open all input/output files */
//    ret = ffmpeg_parse_options(ses->argc, ses->argv);
//    cleanupArgv(ses);
//
//    if (ret < 0 || ses->exitOnFail ){
//        return ret;
//    }
//    pthread_exit(0);
}

JNIEXPORT void JNICALL Java_com_chris_video_RTMPPublisher_setAudioPipeId
  (JNIEnv * env, jobject obj, jint audioPipeId) {
	audio_pipe_id = audioPipeId;
}

JNIEXPORT void JNICALL Java_com_chris_video_RTMPPublisher_setVideoPipeId
  (JNIEnv * env, jobject obj, jint videoPipeId) {
	video_pipe_id = videoPipeId;
}


JNIEXPORT void JNICALL Java_com_chris_video_RTMPPublisher_publish
(JNIEnv * env, jobject obj) {
    pthread_t start_thread;
    pthread_create(&start_thread, NULL, start_publish,  NULL);

//    int ret = ffmpeg_run(ses, ses->argc, ses->argv);
}
