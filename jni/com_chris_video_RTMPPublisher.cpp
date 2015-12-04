//
// Created by Chris on 11/24/15.
//
#include "com_chris_video_RTMPPublisher.h"
#include<pthread.h>
#include <stdio.h>
extern "C" {
    #include "ffmpeg.h"
}

static int pipeid;


//ffmpeg -re -i test2015.h264 -vcodec copy -f flv rtmp://***
void * start_publish(void* args)
{
//    int argc, char **argv
    int numberOfArgs = 13;
    char** arguments = (char**)calloc(numberOfArgs, sizeof(char*));

    arguments[0] = "ffmpeg";
    arguments[1] = "-re";
    arguments[2] = "-i";
    char input[20];
    memset(input, 0, sizeof(input));
    sprintf(input, "pipe:%d", pipeid);
    arguments[3] = input;//"/sdcard/test2016.aac"; //pipe
    arguments[4] = "-acodec";
    arguments[5] = "copy";
    arguments[6] = "-vcodec";
    arguments[7] = "copy";
    arguments[8] = "-f";
    arguments[9] = "flv";
    arguments[10] = "-bsf:a";
    arguments[11] = "aac_adtstoasc";
    arguments[12] = //"/sdcard/test.mp4";
    		"rtmp://120.132.75.127/demo/chris12";

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

JNIEXPORT void JNICALL Java_com_chris_video_RTMPPublisher_publish
(JNIEnv * env, jobject obj, jint pipeId) {
    pipeid = pipeId;
    pthread_t start_thread;
    pthread_create(&start_thread, NULL, start_publish,  NULL);

//    int ret = ffmpeg_run(ses, ses->argc, ses->argv);
}
