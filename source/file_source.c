#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "file_source.h"

struct file_source {
    struct source_itf source_itf;
    int f;
};

static int read_one_ms(struct source_itf *source_itf, FLOAT complex *buffer)
{
    struct file_source *file_source = container_of(source_itf, struct file_source, source_itf);
    char raw_buffer[2 * GPS_SAMPLING_RATE / 1000];
    int res = read(file_source->f, raw_buffer, sizeof(raw_buffer));
    int i;

    if (res < sizeof(raw_buffer))
        return -1;

    for(i = 0; i < GPS_SAMPLING_RATE / 1000; i++)
        buffer[i] = raw_buffer[2 * i] + raw_buffer[2 * i + 1] * I;

    return 0;
}

static void destroy(struct source_itf *source_itf)
{
    struct file_source *file_source = container_of(source_itf, struct file_source, source_itf);

    free(file_source);
}

/* public api */
struct source_itf *create_file_source(char *filename)
{
    struct file_source *file_source = (struct file_source *) malloc(sizeof(struct file_source));

    assert(file_source);
    file_source->f = open(filename, O_RDONLY);
    assert(file_source->f >= 0);
    file_source->source_itf.read_one_ms = read_one_ms;
    file_source->source_itf.destroy = destroy;

    return &file_source->source_itf;
}
