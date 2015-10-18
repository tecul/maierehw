#ifndef __CORE__
#define __CORE__

struct msg {
    char *msg_type;
    void *msg;
};

struct subscriber {
    void (*notify)(struct msg *msg);
};

void subscribe(struct subscriber *subscriber, char *msg_type);
void unsubscribe(struct subscriber *subscriber, char *msg_type);
void publish(struct msg *msg);

#endif
