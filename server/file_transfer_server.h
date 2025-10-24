/*****************************************************************************/
/*    vsock-shell - File transfer server interface                          */
/*****************************************************************************/
#ifndef VSOCK_SHELL_FILE_TRANSFER_SERVER_H
#define VSOCK_SHELL_FILE_TRANSFER_SERVER_H

#include "terminal_server.h"

/* File transfer handlers */
int file_transfer_handle_upload_start(ClientSession *session, Message *msg);
int file_transfer_handle_download_start(ClientSession *session, Message *msg);
int file_transfer_handle_data(ClientSession *session, Message *msg);
int file_transfer_handle_data_end(ClientSession *session);

/* File sending */
void file_transfer_send_data(ClientSession *session);

#endif /* VSOCK_SHELL_FILE_TRANSFER_SERVER_H */
