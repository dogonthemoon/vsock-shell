/*****************************************************************************/
/*    vsock-shell - Terminal server implementation                          */
/*****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include "terminal_server.h"
#include "file_transfer_server.h"
#include "../lib/message_queue.h"
#include "../include/message.h"
#include "../include/common.h"

static ClientSession *session_list_head = NULL;
static int session_count = 0;
static int signal_pipe_write_fd = -1;

/* Environment variables */
static char env_home[MAX_PATH_LENGTH];
static char env_path[MAX_PATH_LENGTH];
static char env_term[MAX_PATH_LENGTH];
static char env_shell[MAX_PATH_LENGTH];

/*****************************************************************************/
static void initialize_environment(void)
{
    char *home = getenv("HOME");
    
    memset(env_home, 0, sizeof(env_home));
    memset(env_path, 0, sizeof(env_path));
    memset(env_term, 0, sizeof(env_term));
    memset(env_shell, 0, sizeof(env_shell));
    
    if (home) {
        snprintf(env_home, sizeof(env_home) - 1, "HOME=%s", home);
    } else {
        snprintf(env_home, sizeof(env_home) - 1, "HOME=/root");
    }
    
    snprintf(env_path, sizeof(env_path) - 1, 
             "PATH=/usr/sbin:/usr/bin:/sbin:/bin");
    snprintf(env_term, sizeof(env_term) - 1, "TERM=xterm");
    snprintf(env_shell, sizeof(env_shell) - 1, "SHELL=/bin/bash");
}

/*****************************************************************************/
static void signal_handler(int signum)
{
    char notification = 'S';
    if (signal_pipe_write_fd >= 0) {
        if (write(signal_pipe_write_fd, &notification, 1) < 0) {
            VSOCK_LOG_ERROR("Failed to write signal notification");
        }
    }
}

/*****************************************************************************/
static void setup_signal_handlers(int pipe_fd)
{
    struct sigaction sa;
    
    signal_pipe_write_fd = pipe_fd;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

/*****************************************************************************/
static void add_session_to_list(ClientSession *session)
{
    session->next = session_list_head;
    session->prev = NULL;
    
    if (session_list_head) {
        session_list_head->prev = session;
    }
    
    session_list_head = session;
    session_count++;
}

/*****************************************************************************/
static void remove_session_from_list(ClientSession *session)
{
    if (session->prev) {
        session->prev->next = session->next;
    } else {
        session_list_head = session->next;
    }
    
    if (session->next) {
        session->next->prev = session->prev;
    }
    
    session_count--;
}

/*****************************************************************************/
ClientSession *terminal_server_find_session_by_socket(int socket_fd)
{
    ClientSession *session = session_list_head;
    
    while (session) {
        if (session->socket_fd == socket_fd) {
            return session;
        }
        session = session->next;
    }
    
    return NULL;
}

/*****************************************************************************/
ClientSession *terminal_server_find_session_by_pty(int pty_fd)
{
    ClientSession *session = session_list_head;
    
    while (session) {
        if (session->pty_master_fd == pty_fd) {
            return session;
        }
        session = session->next;
    }
    
    return NULL;
}

/*****************************************************************************/
static int spawn_shell_process(int pty_slave_fd, const char *command)
{
    char *argv[4];
    char *envp[5];
    
    /* Redirect stdio to PTY slave */
    if (dup2(pty_slave_fd, STDIN_FILENO) < 0 ||
        dup2(pty_slave_fd, STDOUT_FILENO) < 0 ||
        dup2(pty_slave_fd, STDERR_FILENO) < 0) {
        VSOCK_LOG_FATAL("Failed to redirect stdio: %s", strerror(errno));
    }
    
    close(pty_slave_fd);
    
    /* Setup environment */
    envp[0] = env_home;
    envp[1] = env_path;
    envp[2] = env_term;
    envp[3] = env_shell;
    envp[4] = NULL;
    
    /* Execute command or shell */
    if (command) {
        argv[0] = "/bin/bash";
        argv[1] = "-c";
        argv[2] = (char *)command;
        argv[3] = NULL;
    } else {
        argv[0] = "/bin/bash";
        argv[1] = NULL;
    }
    
    execve("/bin/bash", argv, envp);
    VSOCK_LOG_FATAL("Failed to execute bash: %s", strerror(errno));
    
    return -1; /* Never reached */
}

/*****************************************************************************/
static int create_pty_session(ClientSession *session, const char *command)
{
    int pty_master_fd, pty_slave_fd;
    pid_t pid;
    
    /* Create PTY pair */
    if (openpty(&pty_master_fd, &pty_slave_fd, NULL, NULL, NULL) < 0) {
        VSOCK_LOG_ERROR("Failed to create PTY: %s", strerror(errno));
        return -1;
    }
    
    /* Fork child process */
    pid = fork();
    
    if (pid < 0) {
        VSOCK_LOG_ERROR("Failed to fork: %s", strerror(errno));
        close(pty_master_fd);
        close(pty_slave_fd);
        return -1;
    }
    
    if (pid == 0) {
        /* Child process */
        close(pty_master_fd);
        setsid();
        
        if (ioctl(pty_slave_fd, TIOCSCTTY, 0) < 0) {
            VSOCK_LOG_ERROR("Failed to set controlling terminal: %s", strerror(errno));
        }
        
        spawn_shell_process(pty_slave_fd, command);
        exit(EXIT_FAILURE); /* Never reached */
    }
    
    /* Parent process */
    close(pty_slave_fd);
    
    session->pid = pid;
    session->pty_master_fd = pty_master_fd;
    
    VSOCK_LOG_INFO("Created PTY session: pid=%d, pty=%d", pid, pty_master_fd);
    return 0;
}

/*****************************************************************************/
ClientSession *terminal_server_create_session(int socket_fd)
{
    ClientSession *session;
    
    session = (ClientSession *)calloc(1, sizeof(ClientSession));
    if (!session) {
        VSOCK_LOG_ERROR("Failed to allocate session");
        return NULL;
    }
    
    session->socket_fd = socket_fd;
    session->pty_master_fd = -1;
    session->file_fd = -1;
    session->pid = -1;
    session->connection_type = CONNECTION_TYPE_BASH;
    
    if (message_queue_init(socket_fd) < 0) {
        VSOCK_LOG_ERROR("Failed to initialize message queue");
        free(session);
        return NULL;
    }
    
    add_session_to_list(session);
    VSOCK_LOG_INFO("Created new session: socket=%d", socket_fd);
    
    return session;
}

/*****************************************************************************/
void terminal_server_destroy_session(ClientSession *session)
{
    Message msg;
    
    if (!session) {
        return;
    }
    
    VSOCK_LOG_INFO("Destroying session: socket=%d, pid=%d", 
             session->socket_fd, session->pid);
    
    /* Send end message to client */
    msg.type = MSG_TYPE_CLIENT_END;
    msg.length = 0;
    message_queue_write(session->socket_fd, &msg);
    message_queue_flush_writes(session->socket_fd);
    
    /* Close PTY */
    if (session->pty_master_fd >= 0) {
        close(session->pty_master_fd);
    }
    
    /* Close file descriptor */
    if (session->file_fd >= 0) {
        close(session->file_fd);
    }
    
    /* Kill child process */
    if (session->pid > 0) {
        kill(session->pid, SIGTERM);
        waitpid(session->pid, NULL, WNOHANG);
    }
    
    /* Cleanup message queue */
    message_queue_destroy(session->socket_fd);
    
    /* Close socket */
    close(session->socket_fd);
    
    /* Remove from list */
    remove_session_from_list(session);
    
    free(session);
}

/*****************************************************************************/
static int handle_open_bash_message(ClientSession *session)
{
    session->connection_type = CONNECTION_TYPE_BASH;
    return create_pty_session(session, NULL);
}

/*****************************************************************************/
static int handle_open_cmd_message(ClientSession *session, Message *msg)
{
    char command[MAX_MESSAGE_DATA + 1];
    
    memcpy(command, msg->data, msg->length);
    command[msg->length] = '\0';
    
    session->connection_type = CONNECTION_TYPE_CMD;
    return create_pty_session(session, command);
}

/*****************************************************************************/
static int handle_window_size_message(ClientSession *session, Message *msg)
{
    struct winsize ws;
    
    if (session->pty_master_fd < 0) {
        VSOCK_LOG_ERROR("PTY not initialized");
        return -1;
    }
    
    if (msg->length != sizeof(struct winsize)) {
        VSOCK_LOG_ERROR("Invalid window size message length: %u", msg->length);
        return -1;
    }
    
    memcpy(&ws, msg->data, sizeof(struct winsize));
    
    if (ioctl(session->pty_master_fd, TIOCSWINSZ, &ws) < 0) {
        VSOCK_LOG_ERROR("Failed to set window size: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
static int handle_client_data_message(ClientSession *session, Message *msg)
{
    ssize_t bytes_written;
    
    if (session->pty_master_fd < 0) {
        VSOCK_LOG_ERROR("PTY not initialized");
        return -1;
    }
    
    bytes_written = write(session->pty_master_fd, msg->data, msg->length);
    
    if (bytes_written < 0) {
        VSOCK_LOG_ERROR("Failed to write to PTY: %s", strerror(errno));
        return -1;
    }
    
    if (bytes_written != msg->length) {
        VSOCK_LOG_ERROR("Partial write to PTY: %zd/%u", bytes_written, msg->length);
    }
    
    return 0;
}

/*****************************************************************************/
int terminal_server_handle_message(ClientSession *session, Message *msg)
{
    int result = 0;
    
    switch (msg->type) {
        case MSG_TYPE_OPEN_BASH:
            result = handle_open_bash_message(session);
            break;
            
        case MSG_TYPE_OPEN_CMD:
            result = handle_open_cmd_message(session, msg);
            break;
            
        case MSG_TYPE_WINDOW_SIZE:
            result = handle_window_size_message(session, msg);
            break;
            
        case MSG_TYPE_CLIENT_DATA:
            result = handle_client_data_message(session, msg);
            break;
            
        case MSG_TYPE_FILE_UPLOAD_START:
            result = file_transfer_handle_upload_start(session, msg);
            break;
            
        case MSG_TYPE_FILE_DOWNLOAD_START:
            result = file_transfer_handle_download_start(session, msg);
            break;
            
        case MSG_TYPE_FILE_DATA:
            result = file_transfer_handle_data(session, msg);
            break;
            
        case MSG_TYPE_FILE_DATA_END:
            result = file_transfer_handle_data_end(session);
            break;
            
        default:
            VSOCK_LOG_ERROR("Unknown message type: 0x%02X", msg->type);
            result = -1;
            break;
    }
    
    return result;
}

/*****************************************************************************/
static void handle_pty_data(ClientSession *session)
{
    Message msg;
    ssize_t bytes_read;
    
    bytes_read = read(session->pty_master_fd, msg.data, MAX_MESSAGE_DATA);
    
    if (bytes_read > 0) {
        msg.type = MSG_TYPE_PTY_DATA;
        msg.length = bytes_read;
        
        if (message_queue_write(session->socket_fd, &msg) < 0) {
            VSOCK_LOG_ERROR("Failed to queue PTY data");
        }
    } else if (bytes_read == 0) {
        /* PTY closed (child process exited) */
        VSOCK_LOG_INFO("PTY closed for session: socket=%d", session->socket_fd);
        terminal_server_destroy_session(session);
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        VSOCK_LOG_ERROR("PTY read error: %s", strerror(errno));
        terminal_server_destroy_session(session);
    }
}

/*****************************************************************************/
static int handle_session_message(void *context, int fd, Message *msg)
{
    ClientSession *session = (ClientSession *)context;
    
    if (terminal_server_handle_message(session, msg) < 0) {
        VSOCK_LOG_ERROR("Message handling failed");
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
static void handle_session_error(void *context, const char *error)
{
    ClientSession *session = (ClientSession *)context;
    VSOCK_LOG_ERROR("Session error (socket=%d): %s", session->socket_fd, error);
    terminal_server_destroy_session(session);
}

/*****************************************************************************/
void terminal_server_setup_select(fd_set *read_fds, int *max_fd)
{
    ClientSession *session = session_list_head;
    
    while (session) {
        FD_SET(session->socket_fd, read_fds);
        if (session->socket_fd > *max_fd) {
            *max_fd = session->socket_fd;
        }
        
        if (session->pty_master_fd >= 0) {
            FD_SET(session->pty_master_fd, read_fds);
            if (session->pty_master_fd > *max_fd) {
                *max_fd = session->pty_master_fd;
            }
        }
        
        session = session->next;
    }
}

/*****************************************************************************/
void terminal_server_handle_io(fd_set *read_fds)
{
    ClientSession *session = session_list_head;
    ClientSession *next_session;
    
    while (session) {
        next_session = session->next;
        
        /* Handle socket data */
        if (FD_ISSET(session->socket_fd, read_fds)) {
            message_queue_read(session, session->socket_fd,
                             handle_session_message, handle_session_error);
        }
        
        /* Handle PTY data */
        if (session->pty_master_fd >= 0 && 
            FD_ISSET(session->pty_master_fd, read_fds)) {
            handle_pty_data(session);
        }
        
        /* Handle file transfer */
        if (session->file_fd >= 0 && !message_queue_is_saturated(session->socket_fd)) {
            file_transfer_send_data(session);
        }
        
        /* Flush pending writes */
        message_queue_flush_writes(session->socket_fd);
        
        session = next_session;
    }
}

/*****************************************************************************/
void terminal_server_cleanup_dead_sessions(void)
{
    ClientSession *session = session_list_head;
    ClientSession *next_session;
    int status;
    pid_t result;
    
    while (session) {
        next_session = session->next;
        
        if (session->pid > 0) {
            result = waitpid(session->pid, &status, WNOHANG);
            
            if (result > 0) {
                VSOCK_LOG_INFO("Child process %d exited with status %d", 
                        session->pid, WEXITSTATUS(status));
                terminal_server_destroy_session(session);
            }
        }
        
        session = next_session;
    }
}

/*****************************************************************************/
void terminal_server_init(int signal_pipe_fd)
{
    initialize_environment();
    setup_signal_handlers(signal_pipe_fd);
    VSOCK_LOG_INFO("Terminal server initialized");
}
