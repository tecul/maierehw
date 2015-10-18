#include <stdio.h>

#include "core.h"
#include "file_source.h"

int main(int argc, char **argv)
{
    struct msg_payload_new_one_ms_buffer msg_payload_new_one_ms_buffer;
    struct msg msg;

    msg.msg_type = "new_one_ms_buffer";
    msg.msg_payload = &msg_payload_new_one_ms_buffer;
    msg_payload_new_one_ms_buffer.first_sample_time_in_ms = 0;
    /* create world */
    struct source_itf *source_itf = create_file_source(argv[1]);

    while(source_itf->read_one_ms(source_itf, msg_payload_new_one_ms_buffer.file_source_buffer) == 0) {
        publish(&msg);
        msg_payload_new_one_ms_buffer.first_sample_time_in_ms++;
    }

    /* destroy world */
    source_itf->destroy(source_itf);

    return 0;
}
