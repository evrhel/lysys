#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

#include <lysys/ls_media.h>
#include <lysys/ls_core.h>
#include <lysys/ls_sync.h>

#include "ls_handle.h"
#include "ls_native.h"
#include "ls_media_priv.h"

#define kMRPlay 0
#define kMRPause 1
#define kMRTogglePlayPause 2
#define kMRNextTrack 4
#define kMRPreviousTrack 5
#define kMRGoBackFifteenSeconds 12
#define kMRSkipFifteenSeconds 13

extern CFStringRef kMRMediaRemoteNowPlayingInfoAlbum;
extern CFStringRef kMRMediaRemoteNowPlayingInfoArtist;
extern CFStringRef kMRMediaRemoteNowPlayingInfoArtworkData;
extern CFStringRef kMRMediaRemoteNowPlayingInfoDuration;
extern CFStringRef kMRMediaRemoteNowPlayingInfoElapsedTime;
extern CFStringRef kMRMediaRemoteNowPlayingInfoTimestamp;
extern CFStringRef kMRMediaRemoteNowPlayingInfoTitle;
extern CFStringRef kMRMediaRemoteNowPlayingInfoArtworkIdentifier;

typedef void (^MRMediaRemoteGetNowPlayingInfoCompletion)(CFDictionaryRef info);
typedef void (^MRMediaRemoteGetNowPlayingApplicationPIDCompletion)(int pid);

extern Boolean MRMediaRemoteSendCommand(int command, id userInfo);
extern void MRMediaRemoteGetNowPlayingApplicationPID(dispatch_queue_t queue, MRMediaRemoteGetNowPlayingApplicationPIDCompletion completion);
extern void MRMediaRemoteGetNowPlayingInfo(dispatch_queue_t queue, MRMediaRemoteGetNowPlayingInfoCompletion completion);

int ls_media_player_poll_APPLE(struct mediaplayer *mp, ls_handle sema)
{
    if (sema && ls_type_check(sema, LS_SEMAPHORE) != 0)
        return -1;
    
    MRMediaRemoteGetNowPlayingApplicationPID(mp->queue, ^(int pid) {
        mp->pid = pid;
        
        MRMediaRemoteGetNowPlayingInfo(mp->queue, ^(CFDictionaryRef info) {
            CFStringRef artwork_id;
            
            CFRetain(info);
            
            if (mp->data)
                CFRelease(mp->data);
            
            mp->data = info;
            
            artwork_id = CFDictionaryGetValue(info, kMRMediaRemoteNowPlayingInfoArtworkIdentifier);
            if (!mp->artwork_id || !CFEqual(mp->artwork_id, artwork_id))
            {
                if (mp->artwork_id)
                    CFRelease(mp->artwork_id);
                mp->artwork_id = CFRetain(artwork_id);
                
                mp->art_out_of_date = 1;
            }
            
            mp->revision++;
            
            if (sema)
                ls_semaphore_signal(sema);
        });
    });
    
    return 0;
}

int ls_media_player_send_command_APPLE(struct mediaplayer *mp, int cname)
{
    Boolean r;
    
    switch (cname)
    {
    default:
        return ls_set_errno(LS_INVALID_ARGUMENT);
    case LS_MEDIA_COMMAND_PLAY:
        r = MRMediaRemoteSendCommand(kMRPlay, 0);
        break;
    case LS_MEDIA_COMMAND_PAUSE:
        r = MRMediaRemoteSendCommand(kMRPause, 0);
        break;
    case LS_MEDIA_COMMAND_PLAYPAUSE:
        r = MRMediaRemoteSendCommand(kMRTogglePlayPause, 0);
        break;
    case LS_MEDIA_COMMAND_PREVIOUS:
        r = MRMediaRemoteSendCommand(kMRPreviousTrack, 0);
        break;
    case LS_MEDIA_COMMAND_NEXT:
        r = MRMediaRemoteSendCommand(kMRNextTrack, 0);
        break;
    case LS_MEDIA_COMMAND_SKIP_BACK:
        r = MRMediaRemoteSendCommand(kMRGoBackFifteenSeconds, 0);
        break;
    case LS_MEDIA_COMMAND_SKIP_FORWARD:
        r = MRMediaRemoteSendCommand(kMRSkipFifteenSeconds, 0);
        break;
    }
    
    if (!r)
        return ls_set_errno(LS_IO_ERROR);
    
    return 0;
}

int ls_media_player_cache_artwork_APPLE(struct mediaplayer *mp)
{
    struct ls_image *artp;
    CFDataRef value;
    NSBitmapImageRep *bitmap;
    unsigned char *pixel_data, *pixel_end;
    NSInteger bytes_per_pixel;
    NSInteger width, height, stride;
    unsigned char *row_src, *row_dst;
    unsigned char *pixel_src, *pixel_dst;
    unsigned char *row_end;
    NSInteger dst_stride;
    
    artp = &mp->art;
    
    value = CFDictionaryGetValue(mp->data, kMRMediaRemoteNowPlayingInfoArtworkData);
    if (!value)
        return ls_set_errno(LS_NOT_FOUND);
    
    bitmap = [[NSBitmapImageRep alloc] initWithData:(__bridge NSData *)value];
    if (!bitmap)
        return ls_set_errno(LS_IO_ERROR);
    
    /* Must be at least RGB */
    bytes_per_pixel = [bitmap bitsPerPixel] / 8;
    if (bytes_per_pixel < 3)
    {
        [bitmap release];
        return ls_set_errno(LS_NOT_SUPPORTED);
    }
    
    pixel_data = [bitmap bitmapData];
    width = [bitmap pixelsWide];
    height = [bitmap pixelsHigh];
    stride = [bitmap bytesPerRow];
    
    pixel_end = pixel_data + stride * height;
    
    dst_stride = 3 * width;
    dst_stride = (dst_stride + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1);
    
    artp->pixels = ls_realloc(artp->pixels, dst_stride * height);
    if (!artp->pixels)
    {
        [bitmap release];
        return -1;
    }

    /* Copy pixels */
    for (row_src = pixel_data, row_dst = artp->pixels;
         row_src < pixel_end;
         row_src += stride, row_dst += dst_stride)
    {
        row_end = row_src + stride;
        for (pixel_src = row_src, pixel_dst = row_dst;
             pixel_src < row_end;
             pixel_src += bytes_per_pixel,
             pixel_dst += 3)
        {
            pixel_dst[0] = pixel_src[0];
            pixel_dst[1] = pixel_src[1];
            pixel_dst[2] = pixel_src[2];
        }
            
    }
    
    [bitmap release];
    
    artp->width = (int)width;
    artp->height = (int)height;
    artp->stride = (int)dst_stride;
    
    return 0;
}

int ls_media_player_publish_APPLE(struct mediaplayer *mp, ls_handle sema)
{
    return ls_set_errno(LS_NOT_IMPLEMENTED);
}
