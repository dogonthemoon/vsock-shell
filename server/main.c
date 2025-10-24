/*****************************************************************************/
/*    vsock-shell - Server main program                                     */
/*****************************************************************************/
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include "terminal_server.h"
#include "common.h"

static int listen_socket_fd = -1;
static int signal_pipe_fds[2] = {-1, -1};
static volatile int server_running = 1;

/*****************************************************************************/
static void print_usage(const char *program_name)
{
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  --port PORT    Listen port number (default: 9999)\n");
    printf("  --help         Show this help message\n\n");
    printf("Example:\n");
    printf("  %s --port 9999\n", program_name);
}

/*****************************************************************************/
static int create_listen_socket(unsigned int port)
{
    int sock_fd;
    struct sockaddr_vm addr;
    int reuse = 1;
    
    /* Create vsock socket */
    sock_fd = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        VSOCK_LOG_FATAL("Failed to create socket: %s", strerror(errno));
    }
    
    /* Set socket options */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &reuse, sizeof(reuse)) < 0) {
        VSOCK_LOG_ERROR("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    /* Bind to address */
    memset(&addr, 0, sizeof(addr));
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = VMADDR_CID_ANY;
    addr.svm_port = port;
    
    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        VSOCK_LOG_FATAL("Failed to bind to port %u: %s", port, strerror(errno));
    }
    
    /* Start listening */
    if (listen(sock_fd, 5) < 0) {
        VSOCK_LOG_FATAL("Failed to listen: %s", strerror(errno));
    }
    
    VSOCK_LOG_INFO("Listening on port %u", port);
    return sock_fd;
}

/*****************************************************************************/
static void handle_new_connection(void)
{
    struct sockaddr_vm client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;
    ClientSession *session;
    
    client_fd = accept(listen_socket_fd, 
                      (struct sockaddr *)&client_addr, &addr_len);
    
    if (client_fd < 0) {
        VSOCK_LOG_ERROR("Failed to accept connection: %s", strerror(errno));
        return;
    }
    
    VSOCK_LOG_INFO("New connection from CID %u", client_addr.svm_cid);
    
    session = terminal_server_create_session(client_fd);
    if (!session) {
        VSOCK_LOG_ERROR("Failed to create session");
        close(client_fd);
        return;
    }
}

/*****************************************************************************/
static void handle_signal_notification(void)
{
    char notification;
    
    if (read(signal_pipe_fds[0], &notification, 1) > 0) {
        terminal_server_cleanup_dead_sessions();
    }
}

/*****************************************************************************/
static void server_main_loop(void)
{
    fd_set read_fds;
    int max_fd;
    
    while (server_running) {
        FD_ZERO(&read_fds);
        FD_SET(listen_socket_fd, &read_fds);
        FD_SET(signal_pipe_fds[0], &read_fds);
        
        max_fd = (listen_socket_fd > signal_pipe_fds[0]) ? 
                 listen_socket_fd : signal_pipe_fds[0];
        
        /* Add session file descriptors */
        terminal_server_setup_select(&read_fds, &max_fd);
        
        /* Wait for events */
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            VSOCK_LOG_ERROR("Select error: %s", strerror(errno));
            break;
        }
        
        /* Handle new connections */
        if (FD_ISSET(listen_socket_fd, &read_fds)) {
            handle_new_connection();
        }
        
        /* Handle signal notifications */
        if (FD_ISSET(signal_pipe_fds[0], &read_fds)) {
            handle_signal_notification();
        }
        
        /* Handle session I/O */
        terminal_server_handle_io(&read_fds);
    }
}

/*****************************************************************************/
int main(int argc, char *argv[])
{
    int option_index = 0;
    int c;
    unsigned int port = 9999;
    
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"help", no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Parse command line arguments */
    while (1) {
        c = getopt_long(argc, argv, "p:h", long_options, &option_index);
        
        if (c == -1) {
            break;
        }
        
        switch (c) {
            case 'p':
                port = parse_integer(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    
    /* Open syslog */
    openlog("vsock-shell-server", LOG_PID, LOG_USER);
    VSOCK_LOG_INFO("Starting vsock-shell server");
    
    /* Create signal pipe */
    if (pipe(signal_pipe_fds) < 0) {
        VSOCK_LOG_FATAL("Failed to create signal pipe: %s", strerror(errno));
    }
    
    /* Initialize server */
    terminal_server_init(signal_pipe_fds[1]);
    
    /* Create listen socket */
    listen_socket_fd = create_listen_socket(port);
    
    printf("vsock-shell server started on port %u\n", port);
    printf("Waiting for connections...\n");
    
    /* Run main loop */
    server_main_loop();
    
    /* Cleanup */
    close(listen_socket_fd);
    close(signal_pipe_fds[0]);
    close(signal_pipe_fds[1]);
    closelog();
    
    VSOCK_LOG_INFO("Server shutdown");
    return EXIT_SUCCESS;
}
