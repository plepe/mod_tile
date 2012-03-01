#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>

#include "queue.h"
#include "render_config.h"
#include "daemon.h"
#include "gen_tile.h"
#include "protocol.h"
#include "dir_utils.h"
//#include "protocol.h"
#include "daemon.h"

static struct queue queue_list[QUEUE_COUNT];
static struct queue queue_render;
int queue_count=0;

struct item *queue_fetch_request(void)
{
    struct item *item = NULL;
    int queue_idx;
    int todo=0;

    while(!todo) {
	for(queue_idx=0; queue_idx<queue_count; queue_idx++) {
	    if((queue_list[queue_idx].reqNum>0)&&
	       (queue_list[queue_idx].currRender<
	        queue_list[queue_idx].maxRender)) {
		todo=1;
	    }
	}

	if(!todo)
	    wait_for_request();
    }

    for(queue_idx=0; queue_idx<queue_count; queue_idx++) {
	if((queue_list[queue_idx].reqNum>0)&&
	   (queue_list[queue_idx].currRender<
	    queue_list[queue_idx].maxRender)) {
	    item=queue_list[queue_idx].head->next;
	    queue_list[queue_idx].reqNum--;
	    queue_list[queue_idx].currRender++;
	    queue_list[queue_idx].stats++;

	    queue_status(&queue_list[queue_idx]);
	}
    }

    if (item) {
	queue_push(&queue_render, item);
    }

    return item;
}

void queue_push(struct queue *queue, struct item *item) {
    if(item->next) {
	item->next->prev = item->prev;
	item->prev->next = item->next;
    }

    item->prev = queue->head;
    item->next = queue->head->next;
    queue->head->next->prev = item;
    queue->head->next = item;
    if(queue!=&queue_render)
	item->queue= queue;

    queue->reqNum++;
}

int queue_check_constraints(struct queue *queue, struct item *item) {
  //syslog(LOG_DEBUG, "check %d\n", item->req.z);

  // check z
  if((item->req.z<queue->con_minz)||(item->req.z>queue->con_maxz))
    return 0;

  if((queue->con_dirty==0)&&(item->req.cmd==cmdDirty))
    return 0;
    
  if((queue->con_dirty==1)&&(item->req.cmd!=cmdDirty))
    return 0;
    
  return 1;
}

void queue_clear_requests(int fd)
{
    struct item *item, *dupes, *queueHead;

    /**Only need to look up on the shorter request and render queue
      * so using the linear list shouldn't be a problem
      */
    /*
    pthread_mutex_lock(&qLock);
    for (int i = 0; i < 4; i++) {
        switch (i) {
        case 0: { queueHead = &reqHead; break;}
        case 1: { queueHead = &renderHead; break;}
        case 2: { queueHead = &reqPrioHead; break;}
        case 3: { queueHead = &reqBulkHead; break;}
        }

        item = queueHead->next;
        while (item != queueHead) {
            if (item->fd == fd)
                item->fd = FD_INVALID;

            dupes = item->duplicates;
            while (dupes) {
                if (dupes->fd == fd)
                    dupes->fd = FD_INVALID;
                dupes = dupes->duplicates;
            }
            item = item->next;
        }
    }

    pthread_mutex_unlock(&qLock);
    */
}

enum protoCmd queue_item(struct item *item) {
    int queue_idx;
    struct queue *queue;

    item->queue_status=QUEUE_UNDEF;
    for(queue_idx=0; queue_idx<queue_count; queue_idx++) {
      if((queue_list[queue_idx].reqNum < REQ_LIMIT)&&
	 (queue_check_constraints(&queue_list[queue_idx], item))) {
	  queue = &queue_list[queue_idx];

	  item->queue = queue;
	  item->queue_status = QUEUE_PEND;

	  syslog(LOG_DEBUG, "Added to queue %d", queue_idx);
	  queue_status(queue);

	  break;
	}
    }

    if(item->queue_status == QUEUE_UNDEF) {
	syslog(LOG_WARNING, "No suitable queue found or full -> drop");
        // The queue is severely backlogged. Drop request
        //stats.noReqDroped++;
        return cmdNotDone;
    }

    if (queue) {
	queue_push(queue, item);

        /* In addition to the linked list, add item to a hash table index
         * for faster lookup of pending requests.
         */
        insert_item_idx(item);

	return cmdAdded;
    }

    return cmdNotDone;
}

/* maxRender: max count of concurrent items to render
 * con_minz..con_maxz: what zoom levels do we feel responsible for
 * con_dirty: 1 only accept requests for dirty tiles
 *            0 only accept requests for non-existing tiles
 *           -1 don't care about dirty-state
 */
void queue_initq(struct queue *queue) {
  queue->head=(struct item *)malloc(sizeof(struct item));
  queue->head->next=queue->head;
  queue->head->prev=queue->head;
  queue->reqNum=0;
  queue->currRender=0;
  queue->stats=0;
}

void queue_init(int maxRender, int con_minz, int con_maxz, int con_dirty) {
  int queue_idx;
  // get pos of queue and increase queue count
  queue_idx=queue_count++;

  queue_list[queue_idx].queue_idx=queue_idx;
  queue_initq(&queue_list[queue_idx]);
  queue_list[queue_idx].maxRender=maxRender;
  queue_list[queue_idx].con_minz=con_minz;
  queue_list[queue_idx].con_maxz=con_maxz;
  queue_list[queue_idx].con_dirty=con_dirty;
}

void queue_status(struct queue *queue) {
  syslog(LOG_DEBUG, "queue %d: pending requests (%d), current active (%d/%d), total (%d)", queue->queue_idx, queue->reqNum, queue->currRender, queue->maxRender, queue->stats);
}

void queues_init() {
  queue_init(2, 4, 5, 0);
  queue_init(1, 6, 8, 0);
  queue_initq(&queue_render);

  queue_status(&queue_list[0]);
  queue_status(&queue_list[1]);
}

void queue_remove(struct queue *queue, struct item *item) {
    item->next->prev = item->prev;
    item->prev->next = item->next;
    queue->currRender--;
    queue_status(queue);
}
