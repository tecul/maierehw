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

static struct subscriber exit_subscriber;
static int is_looping = 1;

/* private functions */
static void exit_notify(struct subscriber *subscriber, struct event *evt)
{
    is_looping = 0;
}

/* public api */
#define CASE_ALLOCATE_EVENT(type_, name_) \
case type_: \
    evt = (struct event *) malloc(sizeof(struct name_)); \
    evt->type = type_; \
    evt->size = sizeof(struct name_); \
    INIT_LIST_HEAD(&evt->list); \
    break

struct event *allocate_event(event_type_t event_type)
{
    struct event *evt = NULL;

    switch(event_type) {
        CASE_ALLOCATE_EVENT(EVT_QUEUE_EMPTY, event_queue_empty);
        CASE_ALLOCATE_EVENT(EVT_EXIT, event_exit);
        CASE_ALLOCATE_EVENT(EVT_ONE_MS_BUFFER, event_one_ms_buffer);
        CASE_ALLOCATE_EVENT(EVT_SATELLITE_DETECTED, event_satellite_detected);
        CASE_ALLOCATE_EVENT(EVT_TRACKING_LOOP_LOCK, event_tracking_loop_lock);
        CASE_ALLOCATE_EVENT(EVT_TRACKING_LOOP_UNLOCK_OR_LOCK_FAILURE, event_tracking_loop_unlock_or_lock_failure);
        CASE_ALLOCATE_EVENT(EVT_PLL_BIT, event_pll_bit);
        CASE_ALLOCATE_EVENT(EVT_DEMOD_BIT, event_demod_bit);
        CASE_ALLOCATE_EVENT(EVT_DEMOD_BIT_TIMESTAMPED, event_demod_bit_timestamped);
        CASE_ALLOCATE_EVENT(EVT_WORD, event_word);
        CASE_ALLOCATE_EVENT(EVT_EPHEMERIS, event_ephemeris);
        CASE_ALLOCATE_EVENT(EVT_PVT_RAW, event_pvt_raw);
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
    /* subscribe to exit event */
    exit_subscriber.notify = exit_notify;
    subscribe(&exit_subscriber, EVT_EXIT);
}

void subscribe(struct subscriber *subscriber, event_type_t event_type)
{
    struct list_head *head = &subscriber_list_heads[event_type].head;
    pthread_mutex_t *lock = &subscriber_list_heads[event_type].lock;
    int res;

    res = pthread_mutex_lock(lock);
    assert(res == 0);
    list_add(&subscriber->list, head);
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

    while(is_looping && sem_wait(&event_queue.counter) == 0) {
        /* pop entry */
        res = pthread_mutex_lock(&event_queue.lock);
        assert(res == 0);
        entry = event_queue.head.next;
        list_del(entry);
        res = pthread_mutex_unlock(&event_queue.lock);
        assert(res == 0);
        /* call all subscribers */
        evt = container_of(entry, struct event, list);
        //fprintf(stderr, "evt %d\n", evt->type);
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
        //fprintf(stderr, "evt %d done\n", evt->type);
        free(evt);
        /* is list empty. Lock is not need */
        if (list_empty(&event_queue.head))
            publish(allocate_event(EVT_QUEUE_EMPTY));
    }
}
