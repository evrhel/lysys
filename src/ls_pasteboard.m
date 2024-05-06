#include <lysys/ls_clipboard.h>

#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>

#include <assert.h>

#include <lysys/ls_core.h>

#include "ls_pasteboard.h"
#include "ls_native.h"

// format id = index in formats + ID_START
#define ID_START 100

#define ID2INDEX(id) ((NSUInteger)((id) - ID_START))
#define INDEX2ID(idx) ((intptr_t)((idx) + ID_START))

//! \brief Manages pasteboard formats
@interface PasteboardManager : NSObject
{
    NSMutableArray<NSString *> *formats;
}

//! \brief Retrieve the pasteboard manager
//!
//! \return An instance of the pasteboard manager
+ (instancetype)pasteboardManager;

//! \brief Register a clipboard format by name
//!
//! If a format with the name was already registered, the existing
//! format is returned. Sets _ls_errno on error.
//!
//! \param name The name of the format
//!
//! \return The id of the format
- (intptr_t)registerFormat:(const char *)name;

//! \brief Lookup a format name by id
//!
//! The returned object is not retained. Sets _ls_errno on error.
//!
//! \param format The id of the format
//!
//! \return The name of the format, of nil if it is not a valid id
- (NSString *)getFormat:(intptr_t)format;

@end

@implementation PasteboardManager

+ (id)pasteboardManager
{
    static PasteboardManager *pbm = nil;
    static dispatch_once_t tok = 0;
    
    dispatch_once(&tok, ^{
        pbm = [[self alloc] init];
        
        // clean up on exit
        atexit_b(^ {
            [pbm dealloc];
            pbm = nil;
        });
    });
    
    return pbm;
}

- (intptr_t)registerFormat:(const char *)name
{
    NSUInteger i;
    NSString *format_name;
    
    if (!name)
        return ls_set_errno(LS_INVALID_ARGUMENT);
    
    format_name = [NSString stringWithUTF8String:name];

    i = [formats indexOfObject:format_name];
    if (i == NSNotFound)
    {
        [formats addObject:format_name];
        return INDEX2ID([formats count] - 1);
    }
    
    [format_name release];
    
    return INDEX2ID(i);
}

- (NSString *)getFormat:(intptr_t)format
{
    NSUInteger i;
    
    i = format - ID_START;
    if (i >= [formats count])
    {
        ls_set_errno(LS_NOT_FOUND);
        return nil;
    }
        
    return [formats objectAtIndex:i];
}

- (id)init
{
    self = [super init];
    if (self)
        formats = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc
{
    [formats dealloc];
    [super dealloc];
}

@end

intptr_t ls_register_pasteboard_format(const char *name)
{
    return [[PasteboardManager pasteboardManager] registerFormat:name];
}

int ls_set_pasteboard_data(intptr_t fmt, const void *data, size_t cb)
{
    NSString *format;
    NSPasteboard *pb;
    NSData *nsd;
    BOOL rc;
    
#if NSUIntegerMax < SIZE_MAX
    if (cb > NSUIntegerMax)
        return ls_set_errno(LS_OUT_OF_RANGE);
#endif // NSUIntegerMax < SIZE_MAX
    
    format = [[PasteboardManager pasteboardManager] getFormat:fmt];
    if (format == nil)
        return -1;
    
    nsd = [NSData dataWithBytes:data length:cb];
    if (nsd == nil)
        return ls_set_errno(LS_OUT_OF_MEMORY);
    
    pb = [NSPasteboard generalPasteboard];
    
    [pb clearContents];
    
    @try {
        rc = [pb setData:nsd forType:format];
    } @catch (NSException *e) {
        // NSPasteboardCommunicationException
        return ls_set_errno(LS_IO_ERROR);
    } @finally {
        [nsd release];
    }
    
    if (rc == NO)
        return ls_set_errno(LS_ACCESS_DENIED); // ownership changed
    
    return 0;}

int ls_clear_pasteboard_data(void)
{
    [[NSPasteboard generalPasteboard] clearContents];
    return 0;
}

size_t ls_get_pasteboard_data(intptr_t fmt, void *data, size_t cb)
{
    NSString *format;
    NSPasteboard *pb;
    NSData *nsd;
    size_t size;
    
    if (!data != !cb)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    pb = [NSPasteboard generalPasteboard];
    
    format = [[PasteboardManager pasteboardManager] getFormat:fmt];
    if (format == nil)
        return -1;
    
    nsd = [pb dataForType:format];
    if (nsd == nil)
        return ls_set_errno(LS_NO_DATA);
    
    size = [nsd length];
    
    if (data != 0)
        [nsd getBytes:data length:cb];
    
    [nsd release];
    
    return size;
}
