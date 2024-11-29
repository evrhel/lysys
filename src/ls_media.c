#include <lysys/ls_media.h>
#include <lysys/ls_core.h>

#include "ls_handle.h"
#include "ls_native.h"
#include "ls_media_priv.h"

#if LS_DARWIN
int ls_media_player_poll_APPLE(struct mediaplayer *mp, ls_handle sema);
int ls_media_player_send_command_APPLE(struct mediaplayer *mp, int cname);
pid_t ls_media_player_getpid_APPLE(struct mediaplayer *mp);
int ls_media_player_cache_artwork_APPLE(struct mediaplayer *mp);
int ls_media_player_publish_APPLE(struct mediaplayer *mp, ls_handle sema);
#endif // LS_DARWIN

static void ls_media_player_dtor(struct mediaplayer *mp)
{
#if LS_WINDOWS
#elif LS_DARWIN
    if (mp->artwork_id)
        CFRelease(mp->artwork_id);
    
    if (mp->data)
        CFRelease(mp->data);
    
    if (mp->art_data)
        ls_free(mp->art_data);
    
    dispatch_release(mp->queue);
#else
#endif // LS_WINDOWS
}

static const struct ls_class MediaPlayerClass = {
    .type = LS_MEDIAPLAYER,
    .cb = sizeof(struct mediaplayer),
    .dtor = (ls_dtor_t)&ls_media_player_dtor,
    .wait = NULL};

ls_handle ls_media_player_open(void)
{
#if LS_WINDOWS
    struct mediaplayer *mp;
    
    mp = ls_handle_create(&MediaPlayerClass, 0);
    if (!mp)
        return NULL;
    
    return mp;
#elif LS_DARWIN
    struct mediaplayer *mp;

    mp = ls_handle_create(&MediaPlayerClass, 0);
    if (!mp)
        return NULL;

    mp->queue = dispatch_queue_create(NULL, DISPATCH_QUEUE_SERIAL);
    if (!mp->queue)
    {
        ls_handle_dealloc(mp->queue);
        ls_set_errno(LS_OUT_OF_MEMORY);
        return NULL;
    }

    return mp;
#else
    ls_set_errno(LS_NOT_SUPPORTED);
    return NULL;
#endif // LS_WINDOWS
}

int ls_media_player_poll(ls_handle mp, ls_handle sema)
{
#if LS_WINDOWS
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#elif LS_DARWIN
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return ls_media_player_poll_APPLE(mp, sema);
#else
    return ls_set_errno(LS_NOT_SUPPORTED);
#endif // LS_WINDOWS
}

int ls_media_player_get_revision(ls_handle mp)
{
    struct mediaplayer *media_player = mp;
    
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return media_player->revision;
}

unsigned long ls_media_player_getpid(ls_handle mp)
{
    struct mediaplayer *media_player = mp;
    
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return media_player->pid;
}

int ls_media_player_send_command(ls_handle mp, int cname)
{
#if LS_WINDOWS
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#elif LS_DARWIN
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return ls_media_player_send_command_APPLE(mp, cname);
#else
    return ls_set_errno(LS_NOT_SUPPORTED);
#endif // LS_WINDOWS
}

const char *ls_media_player_getstring(ls_handle mp, int pname)
{
    struct mediaplayer *media_player = mp;
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return NULL;
    
    switch (pname)
    {
    default:
        ls_set_errno(LS_NOT_FOUND);
        return NULL;
    case LS_MEDIA_PROPERTY_TITLE:
        return media_player->title;
    case LS_MEDIA_PROPERTY_ARTIST:
        return media_player->artist;
    case LS_MEDIA_PROPERTY_ALBUM:
        return media_player->album;
    }
}

double ls_media_player_getdouble(ls_handle mp, int pname)
{
    struct mediaplayer *media_player = mp;
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return 0.0;
    
    switch (pname)
    {
    default:
        ls_set_errno(LS_NOT_FOUND);
        return 0.0;
    case LS_MEDIA_PROPERTY_DURATION:
        return media_player->duration;
    case LS_MEDIA_PROPERTY_ELAPSED_TIME:
        return media_player->elapsed_time;
    }
}

int ls_media_player_getartwork(ls_handle mp, struct ls_image *artwork)
{
    struct mediaplayer *media_player = mp;
    
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    
    if (media_player->art_out_of_date)
    {
#if LS_WINDOWS
#elif LS_DARWIN
        if (ls_media_player_cache_artwork_APPLE(mp) != 0)
            return -1;
#else
#endif // LS_WINDOWS
        
        media_player->art_out_of_date = 0;
    }
    
    if (!media_player->art.pixels)
        return ls_set_errno(LS_NOT_FOUND);
    
    memcpy(artwork, &media_player->art, sizeof(struct ls_image));
    return 0;
}

int ls_media_player_setstring(ls_handle mp, int pname, const char *val)
{
    struct mediaplayer *media_player = mp;
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    
    switch (pname)
    {
    default:
        return ls_set_errno(LS_NOT_FOUND);
    case LS_MEDIA_PROPERTY_TITLE:
        strncpy(media_player->title, val, sizeof(media_player->title));
        return 0;
    case LS_MEDIA_PROPERTY_ARTIST:
        strncpy(media_player->artist, val, sizeof(media_player->artist));
        return 0;
    case LS_MEDIA_PROPERTY_ALBUM:
        strncpy(media_player->album, val, sizeof(media_player->album));
        return 0;
    }
}

int ls_media_player_setdouble(ls_handle mp, int pname, double val)
{
    struct mediaplayer *media_player = mp;
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return 0.0;
    
    switch (pname)
    {
    default:
        ls_set_errno(LS_NOT_FOUND);
        return 0.0;
    case LS_MEDIA_PROPERTY_DURATION:
        media_player->duration = val;
        return 0;
    case LS_MEDIA_PROPERTY_ELAPSED_TIME:
        media_player->elapsed_time = val;
        return 0;
    }
}

int ls_media_player_setartwork(ls_handle mp, const void *data, size_t data_len)
{
    void *datap;
    
    struct mediaplayer *media_player = mp;
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    
    if (media_player->art_data_capacity < data_len)
    {
        datap = ls_malloc(data_len);
        if (!datap)
            return -1;
        
        ls_free(media_player->art_data);
        
        media_player->art_data = datap;
        media_player->art_data_capacity = data_len;
    }
    
    memcpy(media_player->art_data, data, data_len);
    media_player->art_data_length = data_len;
    
    return 0;
}

int ls_media_player_publish(ls_handle mp, ls_handle sema)
{
#if LS_WINDOWS
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#elif LS_DARWIN
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return ls_media_player_publish_APPLE(mp, sema);
#else
    return ls_set_errno(LS_NOT_SUPPORTED);
#endif // LS_WINDOWS
}
