#include <string.h>
#include <assert.h>
#include <sys/queue.h>
#include <stdlib.h>

#include "core.h"
 
struct subscriber_list_entry {
    LIST_ENTRY(subscriber_list_entry) entries;
    struct subscriber *subscriber;
};
LIST_HEAD(subscriber_head, subscriber_list_entry);

struct msg_type_list_entry {
    LIST_ENTRY(msg_type_list_entry) entries;
    char *msg_type;
    struct subscriber_head subscribers;
};
LIST_HEAD(msg_type_head, msg_type_list_entry);

/* globals */
static struct msg_type_head msg_type_list = LIST_HEAD_INITIALIZER(msg_type_list);

/* privates */
static struct msg_type_list_entry *find_msg_type_list_entry(char *msg_type)
{
    struct msg_type_list_entry *desc;
    struct msg_type_list_entry *desc_find = NULL;

    LIST_FOREACH(desc, &msg_type_list, entries) {
        if (strcmp(msg_type, desc->msg_type) == 0) {
            desc_find = desc;
            break;
        }
    }

    return desc;
}

static struct msg_type_list_entry *find_msg_type_list_entry_or_allocate(char *msg_type)
{
    struct msg_type_list_entry *desc = find_msg_type_list_entry(msg_type);

    if (!desc) {
        desc = (struct msg_type_list_entry *) malloc(sizeof(struct msg_type_list_entry));
        assert(desc != NULL);
        desc->msg_type = strdup(msg_type);
        LIST_INIT(&desc->subscribers);
        LIST_INSERT_HEAD(&msg_type_list, desc, entries);
    }

    return desc;
}

/* public api */
void subscribe(struct subscriber *subscriber, char *msg_type)
{
    struct msg_type_list_entry *msg_type_list_entry = find_msg_type_list_entry_or_allocate(msg_type);
    struct subscriber_list_entry *subscriber_list_entry = (struct subscriber_list_entry *) malloc(sizeof(struct subscriber_list_entry));

    assert(subscriber_list_entry != NULL);
    subscriber_list_entry->subscriber = subscriber;
    LIST_INSERT_HEAD(&msg_type_list_entry->subscribers, subscriber_list_entry, entries);
}

void unsubscribe(struct subscriber *subscriber, char *msg_type)
{
    struct msg_type_list_entry *msg_type_list_entry = find_msg_type_list_entry(msg_type);
    struct subscriber_list_entry *subscriber_list_entry_found = NULL;

    if (msg_type_list_entry) {
        struct subscriber_list_entry *subscriber_list_entry;

        LIST_FOREACH(subscriber_list_entry, &msg_type_list_entry->subscribers, entries) {
            if (subscriber_list_entry->subscriber == subscriber) {
                subscriber_list_entry_found = subscriber_list_entry;
                break;
            }
        }
    }
    if (subscriber_list_entry_found)
        LIST_REMOVE(subscriber_list_entry_found, entries);
}

void publish(struct msg *msg)
{
    struct msg_type_list_entry *msg_type_list_entry = find_msg_type_list_entry(msg->msg_type);

    if (msg_type_list_entry) {
        struct subscriber_list_entry *subscriber_list_entry;

        LIST_FOREACH(subscriber_list_entry, &msg_type_list_entry->subscribers, entries) {
            subscriber_list_entry->subscriber->notify(subscriber_list_entry->subscriber, msg);
        }
    }
}
