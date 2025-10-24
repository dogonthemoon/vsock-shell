/*****************************************************************************/
/*    vsock-shell - Message queue implementation                            */
/*****************************************************************************/
#include <errno.h>
#include <unistd.h>
#include "message_queue.h"
#include "common.h"

#define MAX_FD_COUNT 100
#define MAX_RX_BUFFER 100000
#define MAX_TX_BUFFER 1000000

typedef struct {
    char rx_buffer[MAX_RX_BUFFER];
    int rx_offset;
    char tx_buffer[MAX_TX_BUFFER];
    int tx_start_offset;
    int tx_end_offset;
} MessageQueue;

static MessageQueue *queues[MAX_FD_COUNT];

/*****************************************************************************/
int message_queue_init(int fd)
{
    if (fd >= MAX_FD_COUNT || queues[fd] != NULL) {
        return -1;
    }
    
    queues[fd] = (MessageQueue *)calloc(1, sizeof(MessageQueue));
    if (!queues[fd]) {
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
int message_queue_destroy(int fd)
{
    if (fd >= MAX_FD_COUNT || !queues[fd]) {
        return -1;
    }
    
    free(queues[fd]);
    queues[fd] = NULL;
    return 0;
}

/*****************************************************************************/
int message_queue_write(int fd, Message *msg)
{
    MessageQueue *queue;
    int total_length;
    int available_space;
    
    if (fd >= MAX_FD_COUNT || !queues[fd]) {
        VSOCK_LOG_ERROR("Invalid file descriptor: %d", fd);
        return -1;
    }
    
    queue = queues[fd];
    msg->magic = PROTOCOL_MAGIC;
    total_length = MESSAGE_HEADER_SIZE + msg->length;
    
    /* Calculate available space */
    if (queue->tx_end_offset >= queue->tx_start_offset) {
        available_space = MAX_TX_BUFFER - queue->tx_end_offset;
    } else {
        available_space = queue->tx_start_offset - queue->tx_end_offset;
    }
    
    if (total_length > available_space) {
        VSOCK_LOG_ERROR("TX buffer full (need %d, have %d)", 
                  total_length, available_space);
        return -1;
    }
    
    /* Copy message to TX buffer */
    memcpy(&queue->tx_buffer[queue->tx_end_offset], msg, total_length);
    queue->tx_end_offset += total_length;
    
    if (queue->tx_end_offset >= MAX_TX_BUFFER) {
        queue->tx_end_offset = 0;
    }
    
    return 0;
}

/*****************************************************************************/
int message_queue_write_raw(int fd, const char *data, int length)
{
    MessageQueue *queue;
    int available_space;
    
    if (fd >= MAX_FD_COUNT || !queues[fd]) {
        return -1;
    }
    
    queue = queues[fd];
    
    /* Calculate available space */
    if (queue->tx_end_offset >= queue->tx_start_offset) {
        available_space = MAX_TX_BUFFER - queue->tx_end_offset;
    } else {
        available_space = queue->tx_start_offset - queue->tx_end_offset;
    }
    
    if (length > available_space) {
        return -1;
    }
    
    memcpy(&queue->tx_buffer[queue->tx_end_offset], data, length);
    queue->tx_end_offset += length;
    
    if (queue->tx_end_offset >= MAX_TX_BUFFER) {
        queue->tx_end_offset = 0;
    }
    
    return 0;
}

/*****************************************************************************/
int message_queue_has_pending_writes(int fd)
{
    MessageQueue *queue;
    
    if (fd >= MAX_FD_COUNT || !queues[fd]) {
        return 0;
    }
    
    queue = queues[fd];
    return (queue->tx_start_offset != queue->tx_end_offset);
}

/*****************************************************************************/
int message_queue_is_saturated(int fd)
{
    MessageQueue *queue;
    int pending_bytes;
    
    if (fd >= MAX_FD_COUNT || !queues[fd]) {
        return 0;
    }
    
    queue = queues[fd];
    
    if (queue->tx_end_offset >= queue->tx_start_offset) {
        pending_bytes = queue->tx_end_offset - queue->tx_start_offset;
    } else {
        pending_bytes = MAX_TX_BUFFER - queue->tx_start_offset + queue->tx_end_offset;
    }
    
    return (pending_bytes > (MAX_TX_BUFFER / 2));
}

/*****************************************************************************/
void message_queue_flush_writes(int fd)
{
    MessageQueue *queue;
    int bytes_to_write;
    int bytes_written;
    
    if (fd >= MAX_FD_COUNT || !queues[fd]) {
        return;
    }
    
    queue = queues[fd];
    
    if (queue->tx_start_offset == queue->tx_end_offset) {
        return; /* Nothing to write */
    }
    
    /* Calculate bytes to write */
    if (queue->tx_end_offset > queue->tx_start_offset) {
        bytes_to_write = queue->tx_end_offset - queue->tx_start_offset;
    } else {
        bytes_to_write = MAX_TX_BUFFER - queue->tx_start_offset;
    }
    
    bytes_written = write(fd, &queue->tx_buffer[queue->tx_start_offset], 
                          bytes_to_write);
    
    if (bytes_written > 0) {
        queue->tx_start_offset += bytes_written;
        if (queue->tx_start_offset >= MAX_TX_BUFFER) {
            queue->tx_start_offset = 0;
        }
    } else if (bytes_written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        VSOCK_LOG_ERROR("Write error: %s", strerror(errno));
    }
}

/*****************************************************************************/
void message_queue_read(void *context, int fd,
                        MessageReceivedCallback on_message,
                        ErrorCallback on_error)
{
    MessageQueue *queue;
    int bytes_read;
    int available_space;
    Message *msg;
    int message_total_length;
    
    if (fd >= MAX_FD_COUNT || !queues[fd]) {
        if (on_error) {
            on_error(context, "Invalid file descriptor");
        }
        return;
    }
    
    queue = queues[fd];
    available_space = MAX_RX_BUFFER - queue->rx_offset;
    
    bytes_read = read(fd, &queue->rx_buffer[queue->rx_offset], available_space);
    
    if (bytes_read <= 0) {
        if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            if (on_error) {
                on_error(context, "Read error");
            }
        }
        return;
    }
    
    queue->rx_offset += bytes_read;
    
    /* Process complete messages */
    while (queue->rx_offset >= MESSAGE_HEADER_SIZE) {
        msg = (Message *)queue->rx_buffer;
        
        /* Validate magic number */
        if (msg->magic != PROTOCOL_MAGIC) {
            if (on_error) {
                on_error(context, "Invalid protocol magic");
            }
            return;
        }
        
        message_total_length = MESSAGE_HEADER_SIZE + msg->length;
        
        /* Check if complete message is available */
        if (queue->rx_offset < message_total_length) {
            break;
        }
        
        /* Deliver message */
        if (on_message && on_message(context, fd, msg) < 0) {
            if (on_error) {
                on_error(context, "Message handler error");
            }
            return;
        }
        
        /* Remove processed message from buffer */
        queue->rx_offset -= message_total_length;
        if (queue->rx_offset > 0) {
            memmove(queue->rx_buffer, 
                    &queue->rx_buffer[message_total_length],
                    queue->rx_offset);
        }
    }
}
