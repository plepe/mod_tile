#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

enum queue_status { QUEUE_UNDEF, QUEUE_PEND, QUEUE_REND, QUEUE_DUPL };

struct queue {
    int queue_idx;
    struct item *head;
    int reqNum;
    int currRender;
    int maxRender;
    int stats;
    int con_minz;
    int con_maxz;
    int con_dirty;
};
#define QUEUE_COUNT 4

struct item *queue_fetch_request(void);
int queue_check_constraints(struct queue *queue, struct item *item);
void queue_clear_requests(int fd);
enum protoCmd queue_item(struct item *item);
void queue_init(int maxRender, int con_minz, int con_maxz, int con_dirty);
void queue_status(struct queue *queue);
void queues_init();
void queue_push(struct queue *queue, struct item *item);
void queue_remove(struct queue *queue, struct item *item);

#ifdef __cplusplus
}
#endif
#endif
