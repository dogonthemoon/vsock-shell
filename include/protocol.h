/*****************************************************************************/
/*    vsock-shell - Protocol definitions                                    */
/*****************************************************************************/
#ifndef VSOCK_SHELL_PROTOCOL_H
#define VSOCK_SHELL_PROTOCOL_H

/* Protocol magic number */
#define PROTOCOL_MAGIC 0xCAFEBABE

/* Message types */
typedef enum {
    MSG_TYPE_PTY_DATA = 0x07,
    MSG_TYPE_OPEN_BASH,
    MSG_TYPE_OPEN_CMD,
    MSG_TYPE_WINDOW_SIZE,
    MSG_TYPE_CLIENT_DATA,
    MSG_TYPE_CLIENT_END,
    MSG_TYPE_FILE_UPLOAD_START,
    MSG_TYPE_FILE_DOWNLOAD_START,
    MSG_TYPE_FILE_READY_SEND,
    MSG_TYPE_FILE_READY_RECV,
    MSG_TYPE_FILE_DATA,
    MSG_TYPE_FILE_DATA_END,
    MSG_TYPE_FILE_DATA_BEGIN,
    MSG_TYPE_FILE_DATA_END_ACK
} MessageType;

/* Connection types */
typedef enum {
    CONNECTION_TYPE_BASH = 0,
    CONNECTION_TYPE_CMD,
    CONNECTION_TYPE_FILE_UPLOAD,
    CONNECTION_TYPE_FILE_DOWNLOAD
} ConnectionType;

#endif /* VSOCK_SHELL_PROTOCOL_H */
