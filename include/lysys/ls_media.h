#ifndef _LS_MEDIA_H_
#define _LS_MEDIA_H_

#include "ls_defs.h"
#include "ls_sync.h"

#define LS_MEDIA_PROPERTY_TITLE 0
#define LS_MEDIA_PROPERTY_ARTIST 1
#define LS_MEDIA_PROPERTY_ALBUM 2
#define LS_MEDIA_PROPERTY_DURATION 3
#define LS_MEDIA_PROPERTY_ELAPSED_TIME 4

#define LS_MEDIA_COMMAND_PLAY 0
#define LS_MEDIA_COMMAND_PAUSE 1
#define LS_MEDIA_COMMAND_PLAYPAUSE 2
#define LS_MEDIA_COMMAND_PREVIOUS 3
#define LS_MEDIA_COMMAND_NEXT 4
#define LS_MEDIA_COMMAND_SKIP_BACK 5
#define LS_MEDIA_COMMAND_SKIP_FORWARD 6

struct ls_image
{
    unsigned char *pixels;
    int width, height;
    int stride;
};

ls_handle ls_media_player_open(void);

int ls_media_player_poll(ls_handle mp, ls_handle sema);
int ls_media_player_get_revision(ls_handle mp);

unsigned long ls_media_player_getpid(ls_handle mp);

int ls_media_player_send_command(ls_handle mp, int cname);

const char *ls_media_player_getstring(ls_handle mp, int pname);
double ls_media_player_getdouble(ls_handle mp, int pname);

int ls_media_player_getartwork(ls_handle mp, struct ls_image *artwork);

int ls_media_player_setstring(ls_handle mp, int pname, const char *val);
int ls_media_player_setdouble(ls_handle mp, int pname, double val);
int ls_media_player_setartwork(ls_handle mp, const void *data, size_t data_len);

int ls_media_player_publish(ls_handle mp, ls_handle sema);

#endif // _LS_MEDIA_H_
