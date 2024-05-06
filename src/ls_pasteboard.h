#ifndef _LS_PASTEBOARD_H_
#define _LS_PASTEBOARD_H_

#include <stdlib.h>
#include <stdint.h>

intptr_t ls_register_pasteboard_format(const char *name);
int ls_set_pasteboard_data(intptr_t fmt, const void *data, size_t cb);
int ls_clear_pasteboard_data(void);
size_t ls_get_pasteboard_data(intptr_t fmt, void *data, size_t cb);

#endif // _LS_PASTEBOARD_H_
