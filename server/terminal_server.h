/*****************************************************************************/
/*    vsock-shell - Terminal server interface                               */
/*****************************************************************************/
#ifndef VSOCK_SHELL_TERMINAL_SERVER_H
#define VSOCK_SHELL_TERMINAL_SERVER_H

#include "protocol.h"
#include "../include/common.h"
#include "../include/message.h"

/* Client session structure */
typedef struct ClientSession {
    int pid;
    int socket_fd;
    int pty_master_fd;
    ConnectionType connection_type;
    int file_fd;
    int file_transfer_started;
    char file_path[MAX_PATH_LENGTH];
    struct ClientSession *prev;
    struct ClientSession *next;
} ClientSession;

/* Session management */
ClientSession *terminal_server_create_session(int socket_fd);
void terminal_server_destroy_session(ClientSession *session);
ClientSession *terminal_server_find_session_by_socket(int socket_fd);
ClientSession *terminal_server_find_session_by_pty(int pty_fd);

/* Message handling */
int terminal_server_handle_message(ClientSession *session, Message *msg);

/* Main loop */
void terminal_server_setup_select(fd_set *read_fds, int *max_fd);
void terminal_server_handle_io(fd_set *read_fds);
void terminal_server_cleanup_dead_sessions(void);

#endif /* VSOCK_SHELL_TERMINAL_SERVER_H */
