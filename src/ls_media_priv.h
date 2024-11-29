#ifndef _LS_MEDIA_PRIV_H_
#define _LS_MEDIA_PRIV_H_

#include <lysys/ls_media.h>

#if LS_DARWIN
#include <CoreFoundation/CoreFoundation.h>
#endif // LS_DARWIN

struct mediaplayer
{
#if LS_WINDOWS
#elif LS_DARWIN
    dispatch_queue_t queue;
    CFStringRef artwork_id;
    CFDictionaryRef data;
#else
#endif // LS_WINDOWS
    char title[64];
    char artist[64];
    char album[64];
    double elapsed_time, duration;
    
    int revision;
    
    struct ls_image art;
    int art_out_of_date;
    
    void *art_data;
    size_t art_data_length;
    size_t art_data_capacity;
    
    unsigned long pid;
};

#endif // _LS_MEDIA_PRIV_H_
