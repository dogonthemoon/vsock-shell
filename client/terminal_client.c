/*****************************************************************************/
/*    vsock-shell - Terminal client implementation                          */
/*****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include "terminal_client.h"
#include "../lib/message_queue.h"
#include "../include/message.h"
#include "../include/common.h"
#include "../include/protocol.h"

static struct termios original_termios;
static struct termios current_termios;
static int window_change_pipe_fd = -1;

/*****************************************************************************/
void terminal_restore_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSADRAIN, &original_termios);
    terminal_show_cursor();
}

/*****************************************************************************/
void terminal_enter_raw_mode(void)
{
    /* Save original terminal settings */
    if (tcgetattr(STDIN_FILENO, &original_termios) < 0) {
        VSOCK_LOG_FATAL("Failed to get terminal attributes: %s", strerror(errno));
    }
    
    current_termios = original_termios;
    
    /* Configure raw mode */
    current_termios.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    current_termios.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    current_termios.c_oflag &= ~(OPOST);
    current_termios.c_cflag |= CS8;
    
    /* Apply settings */
    if (tcsetattr(STDIN_FILENO, TCSADRAIN, &current_termios) < 0) {
        VSOCK_LOG_FATAL("Failed to set raw mode: %s", strerror(errno));
    }
    
    /* Register cleanup handler */
    atexit(terminal_restore_mode);
}

/*****************************************************************************/
void terminal_show_cursor(void)
{
    printf("\033[?25h\r\n");
    fflush(stdout);
}

/*****************************************************************************/
void terminal_hide_cursor(void)
{
    printf("\033[?25l");
    fflush(stdout);
}

/*****************************************************************************/
void terminal_send_window_size(int socket_fd)
{
    Message msg;
    struct winsize ws;
    
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0) {
        VSOCK_LOG_ERROR("Failed to get window size: %s", strerror(errno));
        return;
    }
    
    msg.type = MSG_TYPE_WINDOW_SIZE;
    msg.length = sizeof(struct winsize);
    memcpy(msg.data, &ws, sizeof(struct winsize));
    
    if (message_queue_write(socket_fd, &msg) < 0) {
        VSOCK_LOG_ERROR("Failed to send window size");
    }
}

/*****************************************************************************/
static void sigwinch_handler(int signum)
{
    UNUSED(signum);
    char notification = 'W';
    if (window_change_pipe_fd >= 0) {
        if (write(window_change_pipe_fd, &notification, 1) < 0) {
            VSOCK_LOG_ERROR("Failed to write window change notification");
        }
    }
}

/*****************************************************************************/
void terminal_setup_sigwinch_handler(int signal_pipe_fd)
{
    struct sigaction sa;
    
    window_change_pipe_fd = signal_pipe_fd;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigwinch_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGWINCH, &sa, NULL) < 0) {
        VSOCK_LOG_ERROR("Failed to setup SIGWINCH handler: %s", strerror(errno));
    }
}

/*****************************************************************************/
static void send_open_session_message(int socket_fd, const char *command)
{
    Message msg;
    
    if (command) {
        msg.type = MSG_TYPE_OPEN_CMD;
        msg.length = snprintf((char *)msg.data, MAX_MESSAGE_DATA, 
                             "%s", command) + 1;
    } else {
        msg.type = MSG_TYPE_OPEN_BASH;
        msg.length = 0;
    }
    
    if (message_queue_write(socket_fd, &msg) < 0) {
        VSOCK_LOG_FATAL("Failed to send session open message");
    }
}

/*****************************************************************************/
static int handle_server_message(void *context, int fd, Message *msg)
{
    int *session_active = (int *)context;
    UNUSED(fd);

    switch (msg->type) {
        case MSG_TYPE_PTY_DATA:
            /* Write PTY data to stdout */
            if (write(STDOUT_FILENO, msg->data, msg->length) < 0) {
                VSOCK_LOG_ERROR("Failed to write to stdout: %s", strerror(errno));
            }
            break;
            
        case MSG_TYPE_CLIENT_END:
            /* Server closed session */
            *session_active = 0;
            VSOCK_LOG_INFO("Server closed session");
            break;
            
        default:
            VSOCK_LOG_ERROR("Unexpected message type: 0x%02X", msg->type);
            break;
    }
    
    return 0;
}

/*****************************************************************************/
static void handle_read_error(void *context, const char *error)
{
    int *session_active = (int *)context;
    VSOCK_LOG_ERROR("Read error: %s", error);
    *session_active = 0;
}

/*****************************************************************************/
void terminal_session_run(int socket_fd, const char *command)
{
    fd_set read_fds;
    int max_fd;
    int pipe_fds[2];
    int session_active = 1;
    char stdin_buffer[4096];
    ssize_t bytes_read;
    Message msg;
    
    /* Create pipe for signal notifications */
    if (pipe(pipe_fds) < 0) {
        VSOCK_LOG_FATAL("Failed to create pipe: %s", strerror(errno));
    }
    
    /* Setup signal handler */
    terminal_setup_sigwinch_handler(pipe_fds[1]);
    
    /* Initialize message queue */
    if (message_queue_init(socket_fd) < 0) {
        VSOCK_LOG_FATAL("Failed to initialize message queue");
    }
    
    /* Send initial window size */
    terminal_send_window_size(socket_fd);
    
    /* Open session */
    send_open_session_message(socket_fd, command);
    
    /* Enter raw mode if interactive */
    if (!command) {
        terminal_enter_raw_mode();
    }
    
    /* Main event loop */
    while (session_active) {
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(pipe_fds[0], &read_fds);
        
        max_fd = (socket_fd > pipe_fds[0]) ? socket_fd : pipe_fds[0];
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            VSOCK_LOG_FATAL("Select error: %s", strerror(errno));
        }
        
        /* Handle socket data */
        if (FD_ISSET(socket_fd, &read_fds)) {
            message_queue_read(&session_active, socket_fd,
                             handle_server_message, handle_read_error);
        }
        
        /* Handle stdin data */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            bytes_read = read(STDIN_FILENO, stdin_buffer, sizeof(stdin_buffer));
            
            if (bytes_read > 0) {
                msg.type = MSG_TYPE_CLIENT_DATA;
                msg.length = bytes_read;
                memcpy(msg.data, stdin_buffer, bytes_read);
                
                if (message_queue_write(socket_fd, &msg) < 0) {
                    VSOCK_LOG_ERROR("Failed to send client data");
                    session_active = 0;
                }
            } else if (bytes_read == 0) {
                /* EOF on stdin */
                VSOCK_LOG_INFO("EOF on stdin");
                session_active = 0;
            }
        }
        
        /* Handle window size changes */
        if (FD_ISSET(pipe_fds[0], &read_fds)) {
            char notification;
            if (read(pipe_fds[0], &notification, 1) > 0) {
                terminal_send_window_size(socket_fd);
            }
        }
        
        /* Flush pending writes */
        message_queue_flush_writes(socket_fd);
    }
    
    /* Cleanup */
    message_queue_destroy(socket_fd);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
}
