#include <lysys/ls_window.h>

#include <Cocoa/Cocoa.h>

#include "ls_native.h"

int ls_alert_cocoa(void *parent, const char *title, const char *message, int flags)
{
    NSAlert *alert;
    NSString *itext, *mtext;
    NSModalResponse r;
    
    alert = [[NSAlert alloc] init];
    
    if (message != NULL)
    {
        itext = [NSString stringWithUTF8String:message];
        [alert setInformativeText:itext];
        // [itext release];
    }
    
    if (title != NULL)
    {
        mtext = [NSString stringWithUTF8String:title];
        [alert setMessageText:mtext];
        // [mtext release];
    }
    
    // set icon
    switch (flags & LS_DIALOG_ICON_MASK)
    {
    case LS_DIALOG_ERROR:
        [alert setAlertStyle:NSAlertStyleCritical];
        break;
    case LS_DIALOG_WARNING:
        [alert setAlertStyle:NSAlertStyleWarning];
        break;
    case LS_DIALOG_QUESTION:
    case LS_DIALOG_INFORMATION:
    default:
        [alert setAlertStyle:NSAlertStyleInformational];
        break;
    }
    
    // setup buttons, display, and return
    switch (flags & LS_DIALOG_TYPE_MASK)
    {
    case LS_DIALOG_OK:
        [alert addButtonWithTitle:@"OK"];
        r = [alert runModal];
        [alert release];
        if (r == NSAlertFirstButtonReturn)
            return LS_CMD_OK;
        return LS_CMD_CANCEL;
    case LS_DIALOG_OKCANCEL:
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Cancel"];
        r = [alert runModal];
        [alert release];
        if (r == NSAlertFirstButtonReturn)
            return LS_CMD_OK;
        return LS_CMD_CANCEL;
    case LS_DIALOG_ABORTRETRYIGNORE:
        [alert addButtonWithTitle:@"Abort"];
        [alert addButtonWithTitle:@"Retry"];
        [alert addButtonWithTitle:@"Ignore"];
        r = [alert runModal];
        [alert release];
        if (r == NSAlertFirstButtonReturn)
            return LS_CMD_ABORT;
        if (r == NSAlertSecondButtonReturn)
            return LS_CMD_RETRY;
        if (r == NSAlertThirdButtonReturn)
            return LS_CMD_IGNORE;
        return LS_CMD_CANCEL;
    case LS_DIALOG_YESNOCANCEL:
        [alert addButtonWithTitle:@"Yes"];
        [alert addButtonWithTitle:@"No"];
        [alert addButtonWithTitle:@"Cancel"];
        r = [alert runModal];
        [alert release];
        if (r == NSAlertFirstButtonReturn)
            return LS_CMD_YES;
        if (r == NSAlertSecondButtonReturn)
            return LS_CMD_NO;
        return LS_CMD_CANCEL;
    case LS_DIALOG_YESNO:
        [alert addButtonWithTitle:@"Yes"];
        [alert addButtonWithTitle:@"No"];
        r = [alert runModal];
        [alert release];
        if (r == NSAlertFirstButtonReturn)
            return LS_CMD_YES;
        if (r == NSAlertSecondButtonReturn)
            return LS_CMD_NO;
        return LS_CMD_CANCEL;
    case LS_DIALOG_RETRYCANCEL:
        [alert addButtonWithTitle:@"Retry"];
        [alert addButtonWithTitle:@"Cancel"];
        r = [alert runModal];
        [alert release];
        if (r == NSAlertFirstButtonReturn)
            return LS_CMD_RETRY;
        return LS_CMD_CANCEL;
    case LS_DIALOG_CANCELTRYCONTINUE:
        [alert addButtonWithTitle:@"Continue"];
        [alert addButtonWithTitle:@"Try Again"];
        [alert addButtonWithTitle:@"Cancel"];
        r = [alert runModal];
        [alert release];
        if (r == NSAlertFirstButtonReturn)
            return LS_CMD_CONTINUE;
        if (r == NSAlertSecondButtonReturn)
            return LS_CMD_TRYAGAIN;
        return LS_CMD_CANCEL;
    default:
        [alert release];
        return -1;
    }
}

int ls_dialog_input_cocoa(void *parent, const char *title, const char *message, char *buffer, size_t size, int flags)
{
    return -1;
}
