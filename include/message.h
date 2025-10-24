/*****************************************************************************/
/*    vsock-shell - Message structure                                       */
/*****************************************************************************/
#ifndef VSOCK_SHELL_MESSAGE_H
#define VSOCK_SHELL_MESSAGE_H

#include "../include/message.h"
#include <stdint.h>
#include "../include/protocol.h"

#define MAX_MESSAGE_DATA 4096

/* Message structure */
typedef struct {
    uint32_t magic;                    /* Protocol magic number */
    uint32_t type;                     /* Message type */
    uint32_t length;                   /* Data length */
    uint8_t data[MAX_MESSAGE_DATA];    /* Message payload */
} Message;

#define MESSAGE_HEADER_SIZE (3 * sizeof(uint32_t))

#endif /* VSOCK_SHELL_MESSAGE_H */
