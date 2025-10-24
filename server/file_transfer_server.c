/*****************************************************************************/
/*    vsock-shell - File transfer server implementation                     */
/*****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "file_transfer_server.h"
#include "../lib/message_queue.h"
#include "../include/message.h"
#include "../include/common.h"

/*****************************************************************************/
static int validate_upload_request(const char *source, const char *destination,
                                   char *response, size_t response_size)
{
    struct stat st;
    char dir_path[MAX_PATH_LENGTH];
    char *dir_name;
    
    /* Check if destination already exists */
    if (stat(destination, &st) == 0) {
        snprintf(response, response_size, 
                "KO destination '%s' already exists", destination);
        return -1;
    }
    
    /* Check if parent directory exists */
    snprintf(dir_path, sizeof(dir_path), "%s", destination);
    dir_name = dirname(dir_path);
    
    if (stat(dir_name, &st) < 0) {
        snprintf(response, response_size,
                "KO destination directory '%s' does not exist", dir_name);
        return -1;
    }
    
    snprintf(response, response_size, "OK %s %s", source, destination);
    return 0;
}

/*****************************************************************************/
static int validate_download_request(const char *source, const char *destination,
                                     char *response, size_t response_size)
{
    struct stat st;
    
    /* Check if source file exists */
    if (stat(source, &st) < 0) {
        snprintf(response, response_size,
                "KO source file '%s' does not exist", source);
        return -1;
    }
    
    /* Check if it's a regular file */
    if (!S_ISREG(st.st_mode)) {
        snprintf(response, response_size,
                "KO '%s' is not a regular file", source);
        return -1;
    }
    
    snprintf(response, response_size, "OK %s %s", source, destination);
    return 0;
}

/*****************************************************************************/
int file_transfer_handle_upload_start(ClientSession *session, Message *msg)
{
    char buffer[MAX_MESSAGE_DATA + 1];
    char *source_path;
    char *dest_path;
    char response[MAX_PATH_LENGTH];
    Message response_msg;
    
    /* Parse request */
    memcpy(buffer, msg->data, msg->length);
    buffer[msg->length] = '\0';
    
    source_path = strtok(buffer, " ");
    dest_path = strtok(NULL, " ");
    
    if (!source_path || !dest_path) {
        VSOCK_LOG_ERROR("Invalid upload request format");
        return -1;
    }
    
    VSOCK_LOG_INFO("Upload request: %s -> %s", source_path, dest_path);
    
    /* Validate request */
    if (validate_upload_request(source_path, dest_path, 
                                response, sizeof(response)) == 0) {
        /* Open destination file */
        session->file_fd = open(dest_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
        
        if (session->file_fd < 0) {
            snprintf(response, sizeof(response),
                    "KO failed to create file: %s", strerror(errno));
            VSOCK_LOG_ERROR("Failed to create '%s': %s", dest_path, strerror(errno));
        } else {
            strncpy(session->file_path, dest_path, sizeof(session->file_path) - 1);
            session->connection_type = CONNECTION_TYPE_FILE_UPLOAD;
            VSOCK_LOG_INFO("Ready to receive file: %s", dest_path);
        }
    }
    
    /* Send response */
    response_msg.type = MSG_TYPE_FILE_READY_SEND;
    response_msg.length = strlen(response) + 1;
    memcpy(response_msg.data, response, response_msg.length);
    
    if (message_queue_write(session->socket_fd, &response_msg) < 0) {
        VSOCK_LOG_ERROR("Failed to send upload response");
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
int file_transfer_handle_download_start(ClientSession *session, Message *msg)
{
    char buffer[MAX_MESSAGE_DATA + 1];
    char *source_path;
    char *dest_path;
    char response[MAX_PATH_LENGTH];
    Message response_msg;
    
    /* Parse request */
    memcpy(buffer, msg->data, msg->length);
    buffer[msg->length] = '\0';
    
    source_path = strtok(buffer, " ");
    dest_path = strtok(NULL, " ");
    
    if (!source_path || !dest_path) {
        VSOCK_LOG_ERROR("Invalid download request format");
        return -1;
    }
    
    VSOCK_LOG_INFO("Download request: %s -> %s", source_path, dest_path);
    
    /* Validate request */
    if (validate_download_request(source_path, dest_path,
                                  response, sizeof(response)) == 0) {
        /* Open source file */
        session->file_fd = open(source_path, O_RDONLY);
        
        if (session->file_fd < 0) {
            snprintf(response, sizeof(response),
                    "KO failed to open file: %s", strerror(errno));
            VSOCK_LOG_ERROR("Failed to open '%s': %s", source_path, strerror(errno));
        } else {
            strncpy(session->file_path, source_path, sizeof(session->file_path) - 1);
            session->connection_type = CONNECTION_TYPE_FILE_DOWNLOAD;
            VSOCK_LOG_INFO("Ready to send file: %s", source_path);
        }
    }
    
    /* Send response */
    response_msg.type = MSG_TYPE_FILE_READY_RECV;
    response_msg.length = strlen(response) + 1;
    memcpy(response_msg.data, response, response_msg.length);
    
    if (message_queue_write(session->socket_fd, &response_msg) < 0) {
        VSOCK_LOG_ERROR("Failed to send download response");
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
int file_transfer_handle_data(ClientSession *session, Message *msg)
{
    ssize_t bytes_written;
    
    if (session->file_fd < 0) {
        VSOCK_LOG_ERROR("File descriptor not open");
        return -1;
    }
    
    if (session->connection_type != CONNECTION_TYPE_FILE_UPLOAD) {
        VSOCK_LOG_ERROR("Not in upload mode");
        return -1;
    }
    
    bytes_written = write(session->file_fd, msg->data, msg->length);
    
    if (bytes_written < 0) {
        VSOCK_LOG_ERROR("Failed to write file data: %s", strerror(errno));
        return -1;
    }
    
    if (bytes_written != msg->length) {
        VSOCK_LOG_ERROR("Partial write: %zd/%u", bytes_written, msg->length);
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
int file_transfer_handle_data_end(ClientSession *session)
{
    Message msg;
    
    if (session->file_fd >= 0) {
        close(session->file_fd);
        session->file_fd = -1;
    }
    
    VSOCK_LOG_INFO("File transfer completed: %s", session->file_path);
    
    /* Send acknowledgment */
    msg.type = MSG_TYPE_FILE_DATA_END_ACK;
    msg.length = 0;
    
    if (message_queue_write(session->socket_fd, &msg) < 0) {
        VSOCK_LOG_ERROR("Failed to send end acknowledgment");
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
void file_transfer_send_data(ClientSession *session)
{
    Message msg;
    ssize_t bytes_read;
    
    if (session->file_fd < 0) {
        return;
    }
    
    if (session->connection_type != CONNECTION_TYPE_FILE_DOWNLOAD) {
        return;
    }
    
    /* Send begin marker on first call */
    if (!session->file_transfer_started) {
        msg.type = MSG_TYPE_FILE_DATA_BEGIN;
        msg.length = 0;
        
        if (message_queue_write(session->socket_fd, &msg) < 0) {
            VSOCK_LOG_ERROR("Failed to send data begin marker");
            return;
        }
        
        session->file_transfer_started = 1;
    }
    
    /* Read and send file data */
    while (1) {
        bytes_read = read(session->file_fd, msg.data, MAX_MESSAGE_DATA);
        
        if (bytes_read < 0) {
            VSOCK_LOG_ERROR("Failed to read file: %s", strerror(errno));
            close(session->file_fd);
            session->file_fd = -1;
            return;
        }
        
        if (bytes_read == 0) {
            /* EOF - send end marker */
            msg.type = MSG_TYPE_FILE_DATA_END;
            msg.length = 0;
            
            if (message_queue_write(session->socket_fd, &msg) < 0) {
                VSOCK_LOG_ERROR("Failed to send data end marker");
            }
            
            close(session->file_fd);
            session->file_fd = -1;
            
            VSOCK_LOG_INFO("File send completed: %s", session->file_path);
            break;
        }
        
        /* Send data chunk */
        msg.type = MSG_TYPE_FILE_DATA;
        msg.length = bytes_read;
        
        if (message_queue_write(session->socket_fd, &msg) < 0) {
            VSOCK_LOG_ERROR("Failed to send file data");
            close(session->file_fd);
            session->file_fd = -1;
            return;
        }
        
        /* Check if write queue is saturated */
        if (message_queue_is_saturated(session->socket_fd)) {
            break;
        }
    }
}
