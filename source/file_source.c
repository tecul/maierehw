#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "file_source.h"

struct file_source {
    int f;
    int first_sample_time_in_ms;
    struct subscriber queue_empty_subscriber;
};

static int read_one_ms(int f, FLOAT complex *buffer)
{
    char raw_buffer[2 * GPS_SAMPLING_RATE / 1000];
    int res = read(f, raw_buffer, sizeof(raw_buffer));
    int i;

    if (res < sizeof(raw_buffer))
        return -1;

    for(i = 0; i < GPS_SAMPLING_RATE / 1000; i++)
        buffer[i] = raw_buffer[2 * i] + raw_buffer[2 * i + 1] * I;

    return 0;
}

static void queue_empty_notify(struct subscriber *subscriber, struct event *evt)
{
    struct file_source *file_source = container_of(subscriber, struct file_source, queue_empty_subscriber);
    struct event_one_ms_buffer *event_one_ms_buffer = (struct event_one_ms_buffer *) allocate_event(EVT_ONE_MS_BUFFER);

    if (read_one_ms(file_source->f, event_one_ms_buffer->file_source_buffer)) {
        struct event_exit *event_exit = (struct event_exit *) allocate_event(EVT_EXIT);

        deallocate_event(&event_one_ms_buffer->evt);
        publish(&event_exit->evt);
        return ;
    }

    event_one_ms_buffer->first_sample_time_in_ms = file_source->first_sample_time_in_ms++;
    publish(&event_one_ms_buffer->evt);
}

/* public api */
handle create_file_source(char *filename)
{
    struct file_source *file_source = (struct file_source *) malloc(sizeof(struct file_source));

    assert(file_source);
    memset(file_source, 0, sizeof(struct file_source));
    file_source->f = open(filename, O_RDONLY);
    assert(file_source->f >= 0);
    file_source->first_sample_time_in_ms = 0;
    file_source->queue_empty_subscriber.notify = queue_empty_notify;
    subscribe(&file_source->queue_empty_subscriber, EVT_QUEUE_EMPTY);

    return (handle) file_source;
}

void destroy_file_source(handle hdl)
{
    struct file_source *file_source = (struct file_source *) hdl;

    unsubscribe(&file_source->queue_empty_subscriber, EVT_QUEUE_EMPTY);

    free(file_source);
}
