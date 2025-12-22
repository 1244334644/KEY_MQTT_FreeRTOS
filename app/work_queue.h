#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H


typedef void (*work_t)(void *param);

void workqueue_init(void);
void workqueue_run(work_t work, void *param);

#endif // WORK_QUEUE_H
