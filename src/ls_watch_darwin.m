#include "ls_watch_darwin.h"

#include <lysys/ls_core.h>

#include <CoreServices/CoreServices.h>
#include <Foundation/Foundation.h>

#include <pthread.h>

struct ls_watch_event_darwin
{
    int action;
    char *source;
    char *target;
    
    struct ls_watch_event_darwin *next;
};

struct _ls_watch_darwin
{
    FSEventStreamRef stream;
    dispatch_queue_t queue;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    
    char source[PATH_MAX];
    char target[PATH_MAX];
    
    struct ls_watch_event_darwin *front, *back;
};

static void ls_on_fs_event(ConstFSEventStreamRef streamRef, void *clientCallBackInfo,
    size_t numEvents, void *eventPaths, const FSEventStreamEventFlags *eventFlags,
    const FSEventStreamEventId *eventIds)
{
    ls_watch_darwin_t w = clientCallBackInfo;
    size_t i;
    
    for (i = 0; i < numEvents; i++)
    {
        // TODO: store the event in the watch
    }
}

ls_watch_darwin_t ls_watch_darwin_alloc(const char *dir, int recursive)
{
    ls_watch_darwin_t w;
    int rc;
    FSEventStreamContext ctx;
    NSString *dirsr;
    NSArray<NSString *> *paths;
    Boolean b;
    
    w = ls_malloc(sizeof(struct _ls_watch_darwin));
    if (!w)
        return NULL;
    
    rc = pthread_mutex_init(&w->lock, NULL);
    if (rc == -1)
    {
        ls_free(w);
        return NULL;
    }
    
    rc = pthread_cond_init(&w->cond, NULL);
    if (rc == -1)
    {
        ls_free(w);
        return NULL;
    }
    
    dirsr = [NSString stringWithUTF8String:dir];
    paths = [NSArray arrayWithObject:dirsr];
    [dirsr release];

    ctx.version = 0;
    ctx.info = w;
    ctx.retain = NULL;
    ctx.release = NULL;
    ctx.copyDescription = NULL;
    
    // create event queue
    w->queue = dispatch_queue_create(dir, DISPATCH_QUEUE_CONCURRENT);
    if (!w->queue)
    {
        pthread_cond_destroy(&w->cond);
        pthread_mutex_destroy(&w->lock);
        ls_free(w);
        return NULL;
    }
   
    // Create event stream
    w->stream = FSEventStreamCreate(NULL,
                                    &ls_on_fs_event,
                                    &ctx,
                                    (__bridge CFArrayRef)paths,
                                    kFSEventStreamEventIdSinceNow,
                                    0.1,
                                    kFSEventStreamCreateFlagNone);
    [paths release];
    
    if (!w->stream)
    {
        dispatch_release(w->queue);
        pthread_cond_destroy(&w->cond);
        pthread_mutex_destroy(&w->lock);
        ls_free(w);
        return NULL;
    }
    
    // set the event queue for the fs stream
    FSEventStreamSetDispatchQueue(w->stream, w->queue);
    
    // start recieving events
    b = FSEventStreamStart(w->stream);
    if (!b)
    {
        FSEventStreamRelease(w->stream);
        dispatch_release(w->queue);
        pthread_cond_destroy(&w->cond);
        pthread_mutex_destroy(&w->lock);
        ls_free(w);
        return NULL;
    }
    
    memset(w->source, 0, sizeof(w->source));
    memset(w->target, 0, sizeof(w->target));
    
    w->front = NULL;
    w->back = NULL;
    
    return w;
}

void ls_watch_darwin_free(ls_watch_darwin_t w)
{
    // release the event stream
    FSEventStreamRelease(w->stream);
    
    // release the event queue
    dispatch_release(w->queue);
    
    pthread_cond_destroy(&w->cond);
    pthread_mutex_destroy(&w->lock);
    ls_free(w);
}

int ls_watch_darwin_wait(ls_watch_darwin_t w, unsigned long ms)
{
    int rc;
    struct timespec ts;
    
    rc = pthread_mutex_lock(&w->lock);
    if (rc == -1)
        return -1;
    
    rc = 0;
    while (!w->front)
    {
        if (ms == LS_INFINITE)
        {
            rc = pthread_cond_wait(&w->cond, &w->lock);
            if (rc != 0)
            {
                rc = -1;
                break;
            }
        }
        else
        {
            ts.tv_sec = ms / 1000;
            ts.tv_nsec = (ms % 1000) * 1000000;

            rc = pthread_cond_timedwait(&w->cond, &w->lock, &ts);
            if (rc == ETIMEDOUT)
            {
                rc = 1;
                break;
            }
            else if (rc != 0)
            {
                rc = -1;
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&w->lock);
    
    return rc;
}

int ls_watch_darwin_get_result(ls_watch_darwin_t w, struct ls_watch_event *event)
{
    int rc;
    
    rc = pthread_mutex_lock(&w->lock);
    if (rc == -1)
        return -1;
    
    if (!w->front)
    {
        pthread_mutex_unlock(&w->lock);
        return -1;
    }
    
    // TODO: copy event data into `event`
    
    pthread_mutex_unlock(&w->lock);
    
    return -1;
}
