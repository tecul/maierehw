#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <libr/list.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>

#include "core.h"

/* globals */
static struct {
    struct list_head head;
    pthread_mutex_t lock;
    sem_t counter;
} event_queue;

static struct {
    struct list_head head;
    pthread_mutex_t lock;
} subscriber_list_heads[EVT_NB];


/* private functions */


/* public api */
struct event *allocate_event(event_type_t event_type)
{
    struct event *evt = NULL;

    switch(event_type) {
        case EVT_QUEUE_EMPTY:
            evt = (struct event *) malloc(sizeof(struct event_queue_empty));
            assert(evt);
            evt->type = EVT_QUEUE_EMPTY;
            evt->size = sizeof(struct event_queue_empty);
            INIT_LIST_HEAD(&evt->list);
            break;
        default:
            assert(0);
    }

    return evt;
}

void deallocate_event(struct event *evt)
{
    free(evt);
}

void init_event_module()
{
    pthread_mutexattr_t attr;
    int res;
    int i;

    res = pthread_mutexattr_init(&attr);
    assert(res == 0);
    res = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    assert(res == 0);
    /* event_queue */
    INIT_LIST_HEAD(&event_queue.head);
    res = pthread_mutex_init(&event_queue.lock, &attr);
    assert(res == 0);
    res = sem_init(&event_queue.counter, 0, 0);
    /* subscriber_list_heads */
    for(i = 0; i < EVT_NB; ++i) {
        INIT_LIST_HEAD(&subscriber_list_heads[i].head);
        res = pthread_mutex_init(&subscriber_list_heads[i].lock, &attr);
        assert(res == 0);
    }
}

void subscribe(struct subscriber *subscriber, event_type_t event_type)
{
    struct list_head *head = &subscriber_list_heads[event_type].head;
    pthread_mutex_t *lock = &subscriber_list_heads[event_type].lock;
    int res;

    res = pthread_mutex_lock(lock);
    assert(res == 0);
    list_add_tail(&subscriber->list, head);
    res = pthread_mutex_lock(lock);
    assert(res == 0);
}

void unsubscribe(struct subscriber *subscriber, event_type_t event_type)
{
    struct list_head *head = &subscriber_list_heads[event_type].head;
    pthread_mutex_t *lock = &subscriber_list_heads[event_type].lock;
    int res;

    res = pthread_mutex_lock(lock);
    assert(res == 0);
    list_del(&subscriber->list);
    res = pthread_mutex_lock(lock);
    assert(res == 0);
}

void publish(struct event *evt)
{
    int res;

    res = pthread_mutex_lock(&event_queue.lock);
    assert(res == 0);
    list_add_tail(&evt->list, &event_queue.head);
    res = pthread_mutex_unlock(&event_queue.lock);
    assert(res == 0);
    sem_post(&event_queue.counter);
}

void event_loop()
{
    int res;
    struct list_head *entry;
    struct list_head *safe;
    struct event *evt;

    while(sem_wait(&event_queue.counter) == 0) {
        /* pop entry */
        res = pthread_mutex_lock(&event_queue.lock);
        assert(res == 0);
        entry = event_queue.head.next;
        list_del(entry);
        res = pthread_mutex_unlock(&event_queue.lock);
        assert(res == 0);
        /* call all subscribers */
        evt = container_of(entry, struct event, list);
        fprintf(stderr, "evt %d\n", evt->type);
        {
            struct list_head *head = &subscriber_list_heads[evt->type].head;
            pthread_mutex_t *lock = &subscriber_list_heads[evt->type].lock;
            struct subscriber *subscriber;

            res = pthread_mutex_lock(lock);
            assert(res == 0);
            list_for_each_safe(entry, safe, head) {
                subscriber = container_of(entry, struct subscriber, list);
                /* call notify */
                subscriber->notify(subscriber, evt);
            }
            res = pthread_mutex_unlock(lock);
            assert(res == 0);
        }
        /* delete event */
        fprintf(stderr, "evt %d done\n", evt->type);
        free(evt);
    }
}
