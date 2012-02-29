#ifndef DAEMON_H
#define DAEMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h> /* for PATH_MAX */

#include "protocol.h"

#define INILINE_MAX 256
#define MAX_SLAVES 5

typedef struct {
    char *socketname;
    char *iphostname;
    int ipport;
    int num_threads;
    char *tile_dir;
    char *mapnik_plugins_dir;
    char *mapnik_font_dir;
    int mapnik_font_dir_recurse;
    char * stats_filename;
} renderd_config;

typedef struct {
    char xmlname[XMLCONFIG_MAX];
    char xmlfile[PATH_MAX];
    char xmluri[PATH_MAX];
    char host[PATH_MAX];
    char htcpip[PATH_MAX];
    char tile_dir[PATH_MAX];
} xmlconfigitem;

typedef struct {
    long noDirtyRender;
    long noReqRender;
    long noReqPrioRender;
    long noReqBulkRender;
    long noReqDroped;
    long noZoomRender[MAX_ZOOM + 1];
    long timeReqRender;
    long timeReqPrioRender;
    long timeReqBulkRender;
    long timeZoomRender[MAX_ZOOM + 1];
} stats_struct;

void statsRenderFinish(int z, long time);
void request_exit(void);

#define QUEUE_IDX_UNDEF		-1
#define QUEUE_IDX_DUPLICATE	-2
#define QUEUE_IDX_RENDERING	-3

struct queue {
    int queue_idx;
    struct item *head;
    int reqNum;
    int currRender;
    int maxRender;
    int con_minz;
    int con_maxz;
    int con_dirty;
};
#define QUEUE_COUNT 4

#ifdef __cplusplus
}
#endif
#endif
