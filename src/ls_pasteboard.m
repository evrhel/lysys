#include <lysys/ls_clipboard.h>

#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>

#include <assert.h>

#include <lysys/ls_sync.h>
#include <lysys/ls_core.h>

static NSMutableSet<NSString *> *_formats = nil;
static ls_handle _lock = NULL; // lock on adding/reading, _formats cannot have items removed

int ls_init_pasteboard_APPLE(void)
{
    assert(_formats == nil && _lock == NULL);
    
    _lock = ls_lock_create();
    if (!_lock)
        return -1;
    
    _formats = [NSMutableSet new];
    
    return 0;
}

void ls_deinit_pasteboard_APPLE(void)
{
    assert(_formats != nil && _lock != NULL);
    
    [_formats release];
    _formats = nil;
    
    ls_close(_lock);
    _lock = NULL;
}

intptr_t ls_register_pasteboard_format_APPLE(const char *name)
{
    intptr_t format_id;
    NSString *format_name;
    NSSet *new_formats;
    
    format_name = [NSString stringWithUTF8String:name];
    if (format_name == nil)
        return -1;
    
    ls_lock(_lock);
    
    // check if format already registered
    if ([_formats containsObject:format_name])
    {
        ls_unlock(_lock);
        [format_name release];
        return -1;
    }

    [_formats addObject:format_name];
    
    ls_unlock(_lock);
    
    format_id = (intptr_t)format_name;
    
    [format_name release];
    
    return format_id;
}

static int set_pasteboard_impl(NSString *format, const void *data, size_t cb)
{
    NSPasteboard *pb;
    NSData *nsd;
    BOOL rc;
    
    pb = [NSPasteboard generalPasteboard];
    if (!pb)
        return -1;
    
    nsd = [NSData dataWithBytes:data length:cb];
    
    rc = [pb setData:nsd forType:format];
    
    [nsd release];

    return rc == YES ? 0 : -1;
}

int ls_set_pasteboard_data_APPLE(intptr_t fmt, const void *data, size_t cb)
{
    NSString *name;
    NSString *format_name;
    
    // set standard formats
    if (fmt == LS_CF_TEXT)
        return set_pasteboard_impl(NSPasteboardTypeString, data, cb);
    
    format_name = (NSString *)fmt;
    
    ls_lock(_lock);
    
    // check if format was registered
    for (name in _formats)
    {
        // pointer comparison
        if (format_name == name)
        {
            ls_unlock(_lock);
            return set_pasteboard_impl(name, data, cb);
        }
    }
    
    ls_unlock(_lock);
    
    return -1;
}

int ls_set_pasteboard_text_APPLE(const char *text)
{
    return set_pasteboard_impl(NSPasteboardTypeString, text, strlen(text));
}

int ls_clear_pasteboard_data_APPLE(void)
{
    NSPasteboard *pb;
    
    pb = [NSPasteboard generalPasteboard];
    if (pb == nil)
        return -1;
    
    [pb clearContents];
    
    return 0;
}

static size_t get_pasteboard_data_impl(NSString *format, void *data, size_t cb)
{
    // TODO: implement
    return -1;
}

size_t ls_get_pasteboard_data_APPLE(intptr_t fmt, void *data, size_t cb)
{
    // TODO: implement
    return -1;
}
