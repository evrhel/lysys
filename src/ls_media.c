#include <lysys/ls_media.h>
#include <lysys/ls_core.h>

#include "ls_handle.h"
#include "ls_native.h"
#include "ls_media_priv.h"

#if LS_WINDOWS
__declspec(dllimport)
HRESULT WINAPI RoInitialize(_In_ int initType);

__declspec(dllimport)
void WINAPI RoUninitialize();

int ls_media_player_poll_WIN32(struct mediaplayer *mp, ls_handle sema);
int ls_media_player_send_command_WIN32(struct mediaplayer *mp, int cname);
DWORD ls_media_player_getpid_WIN32(struct mediaplayer *mp);
int ls_media_player_cache_artwork_WIN32(struct mediaplayer *mp);
int ls_media_player_publish_WIN32(struct mediaplayer *mp, ls_handle sema);
int ls_media_player_setvolume_WIN32(struct mediaplayer *mp, double volume);
double ls_media_player_getvolume_WIN32(struct mediaplayer *mp);
#elif LS_DARWIN
int ls_media_player_poll_APPLE(struct mediaplayer *mp, ls_handle sema);
int ls_media_player_send_command_APPLE(struct mediaplayer *mp, int cname);
pid_t ls_media_player_getpid_APPLE(struct mediaplayer *mp);
int ls_media_player_cache_artwork_APPLE(struct mediaplayer *mp);
int ls_media_player_publish_APPLE(struct mediaplayer *mp, ls_handle sema);
int ls_media_player_setvolume_APPLE(struct mediaplayer *mp, double volume);
double ls_media_player_getvolume_APPLE(struct mediaplayer *mp);
#endif // LS_DARWIN

static void ls_media_player_dtor(struct mediaplayer *mp)
{
#if LS_WINDOWS
    RoUninitialize();
    CoUninitialize();

    lock_destroy(&mp->lock);

    ls_free(mp->art_data);
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
    .wait = NULL
};

ls_handle ls_media_player_open(void)
{
#if LS_WINDOWS
    struct mediaplayer *mp;
    HRESULT hr;
    
    mp = ls_handle_create(&MediaPlayerClass, 0);
    if (!mp)
        return NULL;

    lock_init(&mp->lock);

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        lock_destroy(&mp->lock);
        ls_handle_dealloc(mp);
        return ls_set_errno_hresult(hr);
    }

    hr = RoInitialize(1); // RO_INIT_MULTITHREADED
    if (FAILED(hr))
    {
        ls_set_errno_hresult(hr);
        return NULL;
    }

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
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return ls_media_player_poll_WIN32(mp, sema);
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
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return ls_media_player_send_command_WIN32(mp, cname);
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
        if (ls_media_player_cache_artwork_WIN32(mp) != 0)
            return -1;
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

const void *ls_media_player_get_raw_artwork(ls_handle mp, size_t *length)
{
    struct mediaplayer *media_player = mp;

    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return NULL;

    if (!length)
    {
        ls_set_errno(LS_INVALID_ARGUMENT);
        return NULL;
    }

    if (!media_player->art_data_length)
    {
        ls_set_errno(LS_SUCCESS);
        return NULL;
    }

    *length = media_player->art_data_length;
    return media_player->art_data;
}

const char *ls_media_player_get_raw_artwork_type(ls_handle mp)
{
    struct mediaplayer *media_player = mp;

    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return NULL;

    if (!media_player->art_data_length)
    {
        ls_set_errno(LS_NOT_FOUND);
        return NULL;
    }

#if LS_WINDOWS
    return "image/png";
#else
    ls_set_errno(LS_NOT_IMPLEMENTED);
    return NULL;
#endif // LS_WINDOWS
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
        return -1;
    
    switch (pname)
    {
    default:
        return ls_set_errno(LS_NOT_FOUND);
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
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return ls_media_player_publish_WIN32(mp, sema);
#elif LS_DARWIN
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    return ls_media_player_publish_APPLE(mp, sema);
#else
    return ls_set_errno(LS_NOT_SUPPORTED);
#endif // LS_WINDOWS
}

int ls_media_player_setvolume(ls_handle mp, double volume)
{
#if LS_WINDOWS
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;

    if (volume < 0.0) volume = 0.0;
    else if (volume > 1.0) volume = 1.0;

    return ls_media_player_setvolume_WIN32(mp, volume);
#elif LS_DARWIN
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return -1;
    
    if (volume < 0.0) volume = 0.0;
    else if (volume > 1.0) volume = 1.0;
    
    return ls_media_player_setvolume_APPLE(mp, volume);
#else
    return ls_set_errno(LS_NOT_SUPPORTED);
#endif // LS_WINDOWS
}

double ls_media_player_getvolume(ls_handle mp)
{
#if LS_WINDOWS
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return 0.0;
    return ls_media_player_getvolume_WIN32(mp);
#elif LS_DARWIN
    if (ls_type_check(mp, LS_MEDIAPLAYER) != 0)
        return 0.0;
    return ls_media_player_getvolume_APPLE(mp);
#else
    return ls_set_errno(LS_NOT_SUPPORTED);
#endif // LS_WINDOWS
}
