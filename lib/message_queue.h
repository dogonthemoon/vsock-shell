/*****************************************************************************/
/*    vsock-shell - Message queue interface                                 */
/*****************************************************************************/
#ifndef VSOCK_SHELL_MESSAGE_QUEUE_H
#define VSOCK_SHELL_MESSAGE_QUEUE_H

#include "message.h"

/* Callback types */
typedef int (*MessageReceivedCallback)(void *context, int fd, Message *msg);
typedef void (*ErrorCallback)(void *context, const char *error);

/* Queue management */
int message_queue_init(int fd);
int message_queue_destroy(int fd);

/* Writing functions */
int message_queue_write(int fd, Message *msg);
int message_queue_write_raw(int fd, const char *data, int length);
int message_queue_has_pending_writes(int fd);
int message_queue_is_saturated(int fd);
void message_queue_flush_writes(int fd);

/* Reading functions */
void message_queue_read(void *context, int fd, 
                        MessageReceivedCallback on_message,
                        ErrorCallback on_error);

#endif /* VSOCK_SHELL_MESSAGE_QUEUE_H */
