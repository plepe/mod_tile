#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "iniparser3.0b/src/iniparser.h"

enum queue_status { QUEUE_UNDEF, QUEUE_PEND, QUEUE_REND, QUEUE_DUPL };

struct queue {
    char *id;
    struct queue *next;
    struct item *head;
    int reqNum;
    int currRender;
    int maxRender;
    int stats;
    int con_minz;
    int con_maxz;
    int con_dirty;
};

struct item *queue_fetch_request(void);
int queue_check_constraints(struct queue *queue, struct item *item);
void queue_clear_requests(int fd);
enum protoCmd queue_item(struct item *item);
struct queue *queue_init(char *id, int maxRender, int con_minz, int con_maxz, int con_dirty);
void queue_status(struct queue *queue);
void queues_init();
struct queue *queue_add(struct queue *queue);
void queue_push(struct queue *queue, struct item *item);
void queue_remove(struct queue *queue, struct item *item);
void queue_ini_add(dictionary *ini, char *section);

#ifdef __cplusplus
}
#endif
#endif
