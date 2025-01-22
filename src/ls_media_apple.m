#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreAudio/CoreAudio.h>

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

extern CFStringRef kMRMediaRemoteNowPlayingInfoDidChangeNotification;
extern CFStringRef kMRMediaRemoteNowPlayingApplicationIsPlayingDidChangeNotification;

extern CFStringRef kMRMediaRemoteNowPlayingApplicationIsPlayingUserInfoKey;
extern CFStringRef kMRMediaRemoteNowPlayingApplicationPIDUserInfoKey;

extern CFStringRef kMRMediaRemoteNowPlayingInfoAlbum;
extern CFStringRef kMRMediaRemoteNowPlayingInfoArtist;
extern CFStringRef kMRMediaRemoteNowPlayingInfoArtworkData;
extern CFStringRef kMRMediaRemoteNowPlayingInfoDuration;
extern CFStringRef kMRMediaRemoteNowPlayingInfoElapsedTime;
extern CFStringRef kMRMediaRemoteNowPlayingInfoTimestamp;
extern CFStringRef kMRMediaRemoteNowPlayingInfoTitle;
extern CFStringRef kMRMediaRemoteNowPlayingInfoArtworkIdentifier;

extern CFStringRef kMRMediaRemoteUpdatedContentItemsUserInfoKey;

typedef void (^MRMediaRemoteGetNowPlayingInfoCompletion)(CFDictionaryRef info);
typedef void (^MRMediaRemoteGetNowPlayingApplicationPIDCompletion)(int pid);

extern Boolean MRMediaRemoteSendCommand(int command, id userInfo);
extern void MRMediaRemoteGetNowPlayingApplicationPID(dispatch_queue_t queue, MRMediaRemoteGetNowPlayingApplicationPIDCompletion completion);
extern void MRMediaRemoteGetNowPlayingInfo(dispatch_queue_t queue, MRMediaRemoteGetNowPlayingInfoCompletion completion);

extern void MRMediaRemoteRegisterForNowPlayingNotifications(dispatch_queue_t queue);
extern void MRMediaRemoteUnregisterForNowPlayingNotifications();

#define cfstring_to_array(cfstring, array) CFStringGetBytes((cfstring), CFRangeMake(0, CFStringGetLength((cfstring))), kCFStringEncodingUTF8, 0, false, (UInt8 *)(array), sizeof((array)), NULL)

@interface MediaSubscription : NSObject

- (instancetype)init:(ls_handle)sema player:(struct mediaplayer *)player;
- (void)dealloc;

- (void)nowPlayingInfoDidChange:(NSNotification *)notification;
- (void)nowPlayingAppIsPlayingDidChange:(NSNotification *)notification;

@end

static void populate_mediaplayer(struct mediaplayer *mp, CFDictionaryRef info)
{
    CFStringRef string;
    CFNumberRef number;
    
    memset(mp->title, 0, sizeof(mp->title));
    memset(mp->artist, 0, sizeof(mp->artist));
    memset(mp->album, 0, sizeof(mp->album));
    mp->elapsed_time = 0.0;
    mp->duration = 0.0;
    
    if (!info)
        return;
    
    string = CFDictionaryGetValue(info, kMRMediaRemoteNowPlayingInfoTitle);
    if (string)
        cfstring_to_array(string, mp->title);
    
    string = CFDictionaryGetValue(info, kMRMediaRemoteNowPlayingInfoArtist);
    if (string)
        cfstring_to_array(string, mp->artist);
    
    string = CFDictionaryGetValue(info, kMRMediaRemoteNowPlayingInfoAlbum);
    if (string)
        cfstring_to_array(string, mp->album);
    
    number = CFDictionaryGetValue(info, kMRMediaRemoteNowPlayingInfoElapsedTime);
    if (number)
        CFNumberGetValue(number, kCFNumberDoubleType, &mp->elapsed_time);
    
    number = CFDictionaryGetValue(info, kMRMediaRemoteNowPlayingInfoDuration);
    if (number)
        CFNumberGetValue(number, kCFNumberDoubleType, &mp->duration);
}

static void handle_info_update(struct mediaplayer *mp, CFDictionaryRef info, unsigned long pid)
{
    CFStringRef artwork_id;
    
    lock_lock(&mp->lock);
    
    mp->pid = pid;
    
    if (!info)
    {
        if (mp->data)
        {
            CFRelease(mp->data);
            mp->data = nil;
            
            if (mp->artwork_id)
            {
                CFRelease(mp->artwork_id);
                mp->artwork_id = nil;
                
                mp->art_out_of_date = YES;
            }
            
            mp->revision++;
            populate_mediaplayer(mp, info);
        }
        
        lock_unlock(&mp->lock);
        return;
    }
    
    CFRetain(info);
    
    // TODO: better way to check
    if (mp->data)
    {
        if (CFEqual(
                    CFDictionaryGetValue(mp->data, kMRMediaRemoteNowPlayingInfoTitle),
                    CFDictionaryGetValue(info, kMRMediaRemoteNowPlayingInfoTitle)
                    ))
        {
            lock_unlock(&mp->lock);
            CFRelease(info);
            return;
        }
        
        
        CFRelease(mp->data);
    }
    
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
    
    populate_mediaplayer(mp, info);
    
    lock_unlock(&mp->lock);
}

static id get_metadata_or_nil(NSDictionary *dict)
{
    NSArray *content_array;
    id content_item, metadata;
    
    content_array = dict[(__bridge NSString *)kMRMediaRemoteUpdatedContentItemsUserInfoKey];
    if (!content_array)
        return nil;
    
    if (![content_array count])
        return nil;
    
    content_item = content_array[0];
    if (![content_item respondsToSelector:@selector(metadata)])
        return nil;
    
    return [content_item performSelector:@selector(metadata)];
}

@implementation MediaSubscription

dispatch_queue_t queue;
ls_handle semaphore;
struct mediaplayer *mp;

- (instancetype)init:(ls_handle)sema player:(struct mediaplayer *)player
{
    queue = dispatch_queue_create(NULL, DISPATCH_QUEUE_SERIAL);
    semaphore = sema;
    mp = player;
    
    MRMediaRemoteRegisterForNowPlayingNotifications(queue);
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                          selector:@selector(nowPlayingInfoDidChange:)
                                          name:(__bridge NSString *)kMRMediaRemoteNowPlayingInfoDidChangeNotification
                                          object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                          selector:@selector(nowPlayingAppIsPlayingDidChange:)
                                          name:(__bridge NSString *)kMRMediaRemoteNowPlayingApplicationIsPlayingDidChangeNotification
                                          object:nil];
}

- (void)dealloc
{
    MRMediaRemoteUnregisterForNowPlayingNotifications();
    dispatch_release(queue);
    [super dealloc];
}

- (void)nowPlayingInfoDidChange:(NSNotification *)notification
{
    NSArray *content_array;
    id content_item;
    id metadata;
    NSNumber *is_playing;
    NSString *title, *old_title;
    BOOL was_updated = NO;
    
    metadata = get_metadata_or_nil(notification.userInfo);
    if (metadata)
    {
        title = [metadata title];
        
        if (title)
        {
            lock_lock(&mp->lock);
            old_title = [NSString stringWithUTF8String:mp->title];
            lock_unlock(&mp->lock);
            
            was_updated = ![title isEqualToString:old_title];
            //[old_title release];
        }
    }
    else
    {
        was_updated = YES;
    }
    
    if (was_updated)
        ls_semaphore_signal(semaphore);
}

- (void)nowPlayingAppIsPlayingDidChange:(NSNotification *)notification
{
    // TODO: handle notification
}

@end

static MediaSubscription *_subscription = nil;

int ls_media_player_poll_APPLE(struct mediaplayer *mp, ls_handle sema)
{
    if (sema && ls_type_check(sema, LS_SEMAPHORE) != 0)
        return -1;
    
    MRMediaRemoteGetNowPlayingApplicationPID(mp->queue, ^(int pid) {
        MRMediaRemoteGetNowPlayingInfo(mp->queue, ^(CFDictionaryRef info) {
            handle_info_update(mp, info, pid);
            ls_semaphore_signal(sema);
        });
    });
    
    return 0;
}

ls_atom ls_media_player_subscribe_APPLE(struct mediaplayer *mp, ls_handle sema)
{
    if (_subscription)
    {
        ls_set_errno(LS_BUSY);
        return 0;
    }
    
    _subscription = [[MediaSubscription alloc] init:sema player:mp];
    
    return 1;
}

int ls_media_player_unsubscribe_APPLE(struct mediaplayer *mp, ls_atom atom)
{
    if (!_subscription || atom != 1)
        return ls_set_errno(LS_INVALID_ARGUMENT);
    
    [_subscription release];
    _subscription = nil;
}

static BOOL is_muted(void)
{
    AudioObjectPropertyAddress get_default_output_device_property_addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    
    AudioObjectPropertyAddress mute_property_addr = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        0
    };
    
    AudioDeviceID default_device_id;
    UInt32 deviceid_size = sizeof(default_device_id);
    
    UInt32 muted;
    UInt32 muteddata_size = sizeof(muted);
    
    OSStatus result;
    
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &get_default_output_device_property_addr,
                                        0, NULL,
                                        &deviceid_size, &default_device_id);
    if (result != kAudioHardwareNoError)
    {
        ls_set_errno(LS_UNKNOWN_ERROR);
        return FALSE;
    }
    
    result = AudioObjectGetPropertyData(default_device_id,
                                        &mute_property_addr,
                                        0, NULL,
                                        &muteddata_size, &muted);
    if (result != kAudioHardwareNoError)
    {
        ls_set_errno(LS_UNKNOWN_ERROR);
        return FALSE;
    }
    
    ls_set_errno(LS_SUCCESS);
    return muted;
}

static int set_muted(BOOL muted)
{
    AudioObjectPropertyAddress get_default_output_device_property_addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    
    AudioObjectPropertyAddress mute_property_addr = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        0
    };
    
    AudioDeviceID default_device_id;
    UInt32 deviceid_size = sizeof(default_device_id);
    
    UInt32 muteddata = muted;
    
    OSStatus result;
    
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &get_default_output_device_property_addr,
                                        0, NULL,
                                        &deviceid_size, &default_device_id);
    if (result != kAudioHardwareNoError)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    
    result = AudioObjectSetPropertyData(default_device_id,
                                        &mute_property_addr,
                                        0, NULL,
                                        sizeof(muteddata), &muteddata);
    if (result != kAudioHardwareNoError)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    
    ls_set_errno(LS_SUCCESS);
    return 0;
}

static int toggle_mute(void)
{
    AudioObjectPropertyAddress get_default_output_device_property_addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    
    AudioObjectPropertyAddress mute_property_addr = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        0
    };
    
    AudioDeviceID default_device_id;
    UInt32 deviceid_size = sizeof(default_device_id);
    
    UInt32 muted;
    UInt32 muteddata_size = sizeof(muted);
    
    OSStatus result;
    
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &get_default_output_device_property_addr,
                                        0, NULL,
                                        &deviceid_size, &default_device_id);
    if (result != kAudioHardwareNoError)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    
    result = AudioObjectGetPropertyData(default_device_id,
                                        &mute_property_addr,
                                        0, NULL,
                                        &muteddata_size, &muted);
    if (result != kAudioHardwareNoError)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    
    muted = !muted;
    
    result = AudioObjectSetPropertyData(default_device_id,
                                        &mute_property_addr,
                                        0, NULL,
                                        sizeof(muted), &muted);
    if (result != kAudioHardwareNoError)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    
    ls_set_errno(LS_SUCCESS);
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
    case LS_MEDIA_COMMAND_MUTE:
        return set_muted(YES);
    case LS_MEDIA_COMMAND_UNMUTE:
        return set_muted(NO);
    case LS_MEDIA_COMMAND_MUTEUNMUTE:
        if (!is_muted())
        {
            if (_ls_errno)
                return -1;
            return set_muted(YES);
        }
            
        return set_muted(NO);
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

int ls_media_player_setvolume_APPLE(struct mediaplayer *mp, double volume)
{
    AudioObjectPropertyAddress get_default_output_device_property_addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    
    AudioObjectPropertyAddress volume1_property_addr = {
        kAudioDevicePropertyVolumeScalar,
        kAudioDevicePropertyScopeOutput,
        1
    };
    
    AudioObjectPropertyAddress volume2_property_addr = {
        kAudioDevicePropertyVolumeScalar,
        kAudioDevicePropertyScopeOutput,
        2
    };
    
    AudioDeviceID default_device_id;
    UInt32 deviceid_size = sizeof(default_device_id);
    
    Float32 volumedata;
    
    OSStatus result;
    
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &get_default_output_device_property_addr,
                                        0, NULL,
                                        &deviceid_size, &default_device_id);
    if (result != kAudioHardwareNoError)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    
    volumedata = (Float32)volume;
    
    result = AudioObjectSetPropertyData(default_device_id,
                                        &volume1_property_addr,
                                        0, NULL,
                                        sizeof(volumedata), &volumedata);
    if (result != kAudioHardwareNoError)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    
    result = AudioObjectSetPropertyData(default_device_id,
                                        &volume2_property_addr,
                                        0, NULL,
                                        sizeof(volumedata), &volumedata);
    if (result != kAudioHardwareNoError)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    
    return 0;
}

double ls_media_player_getvolume_APPLE(struct mediaplayer *mp)
{
    AudioObjectPropertyAddress get_default_output_device_property_addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    
    AudioObjectPropertyAddress volume_property_addr = {
        kAudioDevicePropertyVolumeScalar,
        kAudioDevicePropertyScopeOutput,
        0
    };
    
    AudioDeviceID default_device_id;
    UInt32 deviceid_size = sizeof(default_device_id);
    
    Float32 volumedata;
    UInt32 volumedata_size = sizeof(volumedata);
    
    OSStatus result;
    
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &get_default_output_device_property_addr,
                                        0, NULL,
                                        &deviceid_size, &default_device_id);
    if (result != kAudioHardwareNoError)
    {
        ls_set_errno(LS_UNKNOWN_ERROR);
        return 0.0;
    }
    
    result = AudioObjectGetPropertyData(default_device_id,
                                        &volume_property_addr,
                                        0, NULL,
                                        &volumedata_size, &volumedata);
    if (result != kAudioHardwareNoError)
    {
        ls_set_errno(LS_UNKNOWN_ERROR);
        return 0.0;
    }
    
    return (double)volumedata;
}

