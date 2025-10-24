/*****************************************************************************/
/*    vsock-shell - File transfer client implementation                     */
/*****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>
#include "file_transfer_client.h"
#include "../lib/message_queue.h"
#include "../include/message.h"
#include "../include/common.h"
#include "../include/protocol.h"

static int file_descriptor = -1;
static int transfer_complete = 0;

/*****************************************************************************/
static int validate_upload_path(const char *local_path, const char *remote_dir,
                                char *remote_full_path, size_t path_size)
{
    struct stat st;
    char *file_basename;
    
    if (stat(local_path, &st) < 0) {
        VSOCK_LOG_ERROR("Local file '%s' does not exist", local_path);
        return -1;
    }
    
    if (!S_ISREG(st.st_mode)) {
        VSOCK_LOG_ERROR("'%s' is not a regular file", local_path);
        return -1;
    }
    
    file_basename = basename((char *)local_path);
    snprintf(remote_full_path, path_size, "%s/%s", remote_dir, file_basename);
    
    return 0;
}

/*****************************************************************************/
static int validate_download_path(const char *remote_path, const char *local_dir,
                                  char *local_full_path, size_t path_size)
{
    struct stat st;
    char *file_basename;
    
    if (stat(local_dir, &st) < 0) {
        VSOCK_LOG_ERROR("Local directory '%s' does not exist", local_dir);
        return -1;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        VSOCK_LOG_ERROR("'%s' is not a directory", local_dir);
        return -1;
    }
    
    file_basename = basename((char *)remote_path);
    snprintf(local_full_path, path_size, "%s/%s", local_dir, file_basename);
    
    if (stat(local_full_path, &st) == 0) {
        VSOCK_LOG_ERROR("Local file '%s' already exists", local_full_path);
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
static void send_upload_request(int socket_fd, const char *local_path,
                                const char *remote_full_path)
{
    Message msg;
    
    msg.type = MSG_TYPE_FILE_UPLOAD_START;
    msg.length = snprintf((char *)msg.data, MAX_MESSAGE_DATA,
                         "%s %s", local_path, remote_full_path) + 1;
    
    if (message_queue_write(socket_fd, &msg) < 0) {
        VSOCK_LOG_FATAL("Failed to send upload request");
    }
}

/*****************************************************************************/
static void send_download_request(int socket_fd, const char *remote_path,
                                  const char *local_full_path)
{
    Message msg;
    
    msg.type = MSG_TYPE_FILE_DOWNLOAD_START;
    msg.length = snprintf((char *)msg.data, MAX_MESSAGE_DATA,
                         "%s %s", remote_path, local_full_path) + 1;
    
    if (message_queue_write(socket_fd, &msg) < 0) {
        VSOCK_LOG_FATAL("Failed to send download request");
    }
}

/*****************************************************************************/
static void send_file_data(int socket_fd)
{
    Message msg;
    ssize_t bytes_read;
    
    if (file_descriptor < 0) {
        VSOCK_LOG_ERROR("File descriptor not open");
        return;
    }
    
    /* Send begin marker */
    msg.type = MSG_TYPE_FILE_DATA_BEGIN;
    msg.length = 0;
    if (message_queue_write(socket_fd, &msg) < 0) {
        VSOCK_LOG_ERROR("Failed to send data begin marker");
        return;
    }
    
    /* Send file data in chunks */
    while (1) {
        bytes_read = read(file_descriptor, msg.data, MAX_MESSAGE_DATA);
        
        if (bytes_read < 0) {
            VSOCK_LOG_ERROR("Failed to read file: %s", strerror(errno));
            close(file_descriptor);
            file_descriptor = -1;
            return;
        }
        
        if (bytes_read == 0) {
            /* EOF reached */
            break;
        }
        
        msg.type = MSG_TYPE_FILE_DATA;
        msg.length = bytes_read;
        
        if (message_queue_write(socket_fd, &msg) < 0) {
            VSOCK_LOG_ERROR("Failed to send file data");
            close(file_descriptor);
            file_descriptor = -1;
            return;
        }
        
        /* Check if write queue is saturated */
        if (message_queue_is_saturated(socket_fd)) {
            break;
        }
    }
    
    /* If EOF, send end marker */
    if (bytes_read == 0) {
        msg.type = MSG_TYPE_FILE_DATA_END;
        msg.length = 0;
        
        if (message_queue_write(socket_fd, &msg) < 0) {
            VSOCK_LOG_ERROR("Failed to send data end marker");
        }
        
        close(file_descriptor);
        file_descriptor = -1;
    }
}

/*****************************************************************************/
static int handle_upload_message(void *context, int fd, Message *msg)
{
    int socket_fd = *(int *)context;
    char response[MAX_PATH_LENGTH];
    
    UNUSED(fd);
    
    switch (msg->type) {
        case MSG_TYPE_FILE_READY_SEND:
            /* Server ready to receive */
            memcpy(response, msg->data, msg->length);
            response[msg->length] = '\0';
            
            if (strncmp(response, "OK", 2) == 0) {
                VSOCK_LOG_INFO("Server ready, starting upload");
                send_file_data(socket_fd);
            } else {
                VSOCK_LOG_ERROR("Server rejected upload: %s", response);
                transfer_complete = 1;
            }
            break;
            
        case MSG_TYPE_FILE_DATA_END_ACK:
            /* Transfer complete */
            VSOCK_LOG_INFO("Upload completed successfully");
            transfer_complete = 1;
            break;
            
        default:
            VSOCK_LOG_ERROR("Unexpected message type: 0x%02X", msg->type);
            break;
    }
    
    return 0;
}

/*****************************************************************************/
static int handle_download_message(void *context, int fd, Message *msg)
{
    ssize_t bytes_written;
    Message response_msg;
    int socket_fd = *(int *)context;
    char response[MAX_PATH_LENGTH];
    
    UNUSED(fd);
    
    switch (msg->type) {
        case MSG_TYPE_FILE_READY_RECV:
            /* Server ready to send */
            memcpy(response, msg->data, msg->length);
            response[msg->length] = '\0';
            
            if (strncmp(response, "OK", 2) == 0) {
                VSOCK_LOG_INFO("Server ready, starting download");
            } else {
                VSOCK_LOG_ERROR("Server rejected download: %s", response);
                transfer_complete = 1;
            }
            break;
            
        case MSG_TYPE_FILE_DATA:
            /* Receive file data */
            if (file_descriptor < 0) {
                VSOCK_LOG_ERROR("File not open for writing");
                transfer_complete = 1;
                break;
            }
            
            bytes_written = write(file_descriptor, msg->data, msg->length);
            if (bytes_written != msg->length) {
                VSOCK_LOG_ERROR("Failed to write file data: %s", strerror(errno));
                transfer_complete = 1;
            }
            break;
            
        case MSG_TYPE_FILE_DATA_END:
            /* Transfer complete */
            if (file_descriptor >= 0) {
                close(file_descriptor);
                file_descriptor = -1;
            }
            
            /* Send ACK */
            response_msg.type = MSG_TYPE_FILE_DATA_END_ACK;
            response_msg.length = 0;
            message_queue_write(socket_fd, &response_msg);
            
            VSOCK_LOG_INFO("Download completed successfully");
            transfer_complete = 1;
            break;
            
        default:
            VSOCK_LOG_ERROR("Unexpected message type: 0x%02X", msg->type);
            break;
    }
    
    return 0;
}

/*****************************************************************************/
static void handle_transfer_error(void *context, const char *error)
{
    VSOCK_LOG_ERROR("Transfer error: %s", error);
    transfer_complete = 1;
}

/*****************************************************************************/
void file_transfer_run_upload_loop(int socket_fd, const char *local_path,
                                   const char *remote_dir)
{
    char remote_full_path[MAX_PATH_LENGTH];
    fd_set read_fds;
    
    /* Validate paths */
    if (validate_upload_path(local_path, remote_dir, 
                            remote_full_path, sizeof(remote_full_path)) < 0) {
        return;
    }
    
    /* Open local file */
    file_descriptor = open(local_path, O_RDONLY);
    if (file_descriptor < 0) {
        VSOCK_LOG_ERROR("Failed to open '%s': %s", local_path, strerror(errno));
        return;
    }
    
    /* Initialize message queue */
    if (message_queue_init(socket_fd) < 0) {
        close(file_descriptor);
        VSOCK_LOG_FATAL("Failed to initialize message queue");
    }
    
    /* Send upload request */
    send_upload_request(socket_fd, local_path, remote_full_path);
    transfer_complete = 0;
    
    /* Event loop */
    while (!transfer_complete) {
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);
        
        if (select(socket_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            VSOCK_LOG_ERROR("Select error: %s", strerror(errno));
            break;
        }
        
        if (FD_ISSET(socket_fd, &read_fds)) {
            message_queue_read(&socket_fd, socket_fd,
                             handle_upload_message, handle_transfer_error);
        }
        
        /* Continue sending if not saturated */
        if (file_descriptor >= 0 && !message_queue_is_saturated(socket_fd)) {
            send_file_data(socket_fd);
        }
        
        message_queue_flush_writes(socket_fd);
    }
    
    /* Cleanup */
    if (file_descriptor >= 0) {
        close(file_descriptor);
    }
    message_queue_destroy(socket_fd);
}

/*****************************************************************************/
void file_transfer_run_download_loop(int socket_fd, const char *remote_path,
                                     const char *local_dir)
{
    char local_full_path[MAX_PATH_LENGTH];
    fd_set read_fds;
    
    /* Validate paths */
    if (validate_download_path(remote_path, local_dir,
                              local_full_path, sizeof(local_full_path)) < 0) {
        return;
    }
    
    /* Open local file for writing */
    file_descriptor = open(local_full_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (file_descriptor < 0) {
        VSOCK_LOG_ERROR("Failed to create '%s': %s", local_full_path, strerror(errno));
        return;
    }
    
    /* Initialize message queue */
    if (message_queue_init(socket_fd) < 0) {
        close(file_descriptor);
        unlink(local_full_path);
        VSOCK_LOG_FATAL("Failed to initialize message queue");
    }
    
    /* Send download request */
    send_download_request(socket_fd, remote_path, local_full_path);
    transfer_complete = 0;
    
    /* Event loop */
    while (!transfer_complete) {
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);
        
        if (select(socket_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            VSOCK_LOG_ERROR("Select error: %s", strerror(errno));
            break;
        }
        
        if (FD_ISSET(socket_fd, &read_fds)) {
            message_queue_read(&socket_fd, socket_fd,
                             handle_download_message, handle_transfer_error);
        }
        
        message_queue_flush_writes(socket_fd);
    }
    
    /* Cleanup */
    if (file_descriptor >= 0) {
        close(file_descriptor);
    }
    message_queue_destroy(socket_fd);
}
