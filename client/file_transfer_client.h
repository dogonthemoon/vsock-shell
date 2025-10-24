/*****************************************************************************/
/*    vsock-shell - File transfer client interface                          */
/*****************************************************************************/
#ifndef VSOCK_SHELL_FILE_TRANSFER_CLIENT_H
#define VSOCK_SHELL_FILE_TRANSFER_CLIENT_H

/* File transfer operations */
int file_transfer_upload(int socket_fd, const char *local_path, 
                         const char *remote_dir);
int file_transfer_download(int socket_fd, const char *remote_path,
                           const char *local_dir);

/* File transfer event loop */
void file_transfer_run_upload_loop(int socket_fd, const char *local_path,
                                   const char *remote_dir);
void file_transfer_run_download_loop(int socket_fd, const char *remote_path,
                                     const char *local_dir);

#endif /* VSOCK_SHELL_FILE_TRANSFER_CLIENT_H */
