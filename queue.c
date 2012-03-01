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

static struct queue *queue_list=NULL;
static struct queue *queue_render=NULL;

struct item *queue_fetch_request(void)
{
    struct item *item = NULL;
    struct queue *queue;
    int queue_idx;
    int todo=0;

    while(!todo) {
	queue=queue_list;
	while(queue!=NULL) {
	    if((queue->reqNum>0)&&
	       (queue->currRender<
	        queue->maxRender)) {
		todo=1;
	    }

	    queue=queue->next;
	}

	if(!todo)
	    wait_for_request();
    }

    queue=queue_list;
    while(queue!=NULL) {
	if((queue->reqNum>0)&&
	   (queue->currRender<
	    queue->maxRender)) {
	    item=queue->head->next;
	    queue->reqNum--;
	    queue->currRender++;
	    queue->stats++;

	    queue_status(queue);
	}

	queue=queue->next;
    }

    if (item) {
	queue_push(queue_render, item);
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
    if(queue!=queue_render)
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
    struct queue *queue;

    item->queue_status=QUEUE_UNDEF;
    queue=queue_list;
    while(queue!=NULL) {
      if((queue->reqNum < REQ_LIMIT)&&
	 (queue_check_constraints(queue, item))) {
	  item->queue = queue;
	  item->queue_status = QUEUE_PEND;

	  syslog(LOG_DEBUG, "Added to queue %s", queue->id);
	  queue_status(queue);

	  break;
	}

	queue=queue->next;
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
struct queue *queue_init(char *id, int maxRender, int con_minz, int con_maxz, int con_dirty) {
    struct queue *queue;

    queue=(struct queue*)malloc(sizeof(struct queue));

    queue->head=(struct item *)malloc(sizeof(struct item));
    queue->head->next=queue->head;
    queue->head->prev=queue->head;

    queue->maxRender=maxRender;
    queue->con_minz=con_minz;
    queue->con_maxz=con_maxz;
    queue->con_dirty=con_dirty;

    queue->id=id;
    queue->reqNum=0;
    queue->currRender=0;
    queue->stats=0;

    return queue;
}

void queue_status(struct queue *queue) {
  syslog(LOG_DEBUG, "queue %s: pending requests (%d), current active (%d/%d), total (%d)", queue->id, queue->reqNum, queue->currRender, queue->maxRender, queue->stats);
}

void queues_init() {
  queue_render=queue_init("render", 0, 0, 0, 0);
}

void queue_ini_add(dictionary *ini, char *section) {
    char buffer[PATH_MAX];
    char *id;
    int maxRender, con_minz, con_maxz, con_dirty;

    id=&section[6];

    sprintf(buffer, "%s:maxRender", section);
    maxRender=iniparser_getint(ini, buffer, NUM_THREADS);

    sprintf(buffer, "%s:con_minz", section);
    con_minz=iniparser_getint(ini, buffer, 0);

    sprintf(buffer, "%s:con_maxz", section);
    con_maxz=iniparser_getint(ini, buffer, MAX_ZOOM);

    sprintf(buffer, "%s:con_dirty", section);
    con_dirty=iniparser_getint(ini, buffer, -1);

    queue_add(queue_init(id, maxRender, con_minz, con_maxz, con_dirty));
}

void queue_remove(struct queue *queue, struct item *item) {
    item->next->prev = item->prev;
    item->prev->next = item->next;
    queue->currRender--;
    queue_status(queue);
}

struct queue *queue_add(struct queue *queue) {
    struct queue *curr;

    if(queue_list==NULL) {
	queue_list=queue;
    }
    else {
	curr=queue_list;
	while(curr->next!=NULL) {
	    curr=curr->next;
	}

	curr->next=queue;
    }

    queue->next=NULL;

    return queue;
}
