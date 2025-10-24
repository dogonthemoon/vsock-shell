/*****************************************************************************/
/*    vsock-shell - Client main program                                     */
/*****************************************************************************/
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include "terminal_client.h"
#include "file_transfer_client.h"
#include "common.h"

static void print_usage(const char *program_name)
{
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  --cid CID          Guest VM context ID (required)\n");
    printf("  --port PORT        Server port number (default: 9999)\n");
    printf("  --cmd COMMAND      Execute command instead of shell\n");
    printf("  --upload FILE      Upload file to guest\n");
    printf("  --download FILE    Download file from guest\n");
    printf("  --remote-dir DIR   Remote directory for upload (default: /tmp)\n");
    printf("  --local-dir DIR    Local directory for download (default: ./)\n");
    printf("  --help             Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s --cid 3 --port 9999\n", program_name);
    printf("  %s --cid 3 --cmd \"ls -la /tmp\"\n", program_name);
    printf("  %s --cid 3 --upload file.txt --remote-dir /tmp\n", program_name);
    printf("  %s --cid 3 --download /etc/hostname --local-dir ./\n", program_name);
}

static int connect_to_server(unsigned int cid, unsigned int port)
{
    int sock_fd;
    struct sockaddr_vm addr;
    
    /* Create vsock socket */
    sock_fd = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        VSOCK_LOG_FATAL("Failed to create socket: %s", strerror(errno));
    }
    
    /* Configure server address */
    memset(&addr, 0, sizeof(addr));
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = cid;
    addr.svm_port = port;
    
    /* Connect to server */
    printf("Connecting to CID %u on port %u...\n", cid, port);
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        VSOCK_LOG_FATAL("Failed to connect: %s", strerror(errno));
    }
    
    printf("Connected successfully\n");
    return sock_fd;
}

int main(int argc, char *argv[])
{
    int option_index = 0;
    int c;
    unsigned int cid = 0;
    unsigned int port = 9999;
    char *command = NULL;
    char *upload_file = NULL;
    char *download_file = NULL;
    char *remote_dir = "/tmp";
    char *local_dir = ".";
    int sock_fd;
    
    static struct option long_options[] = {
        {"cid",        required_argument, 0, 'c'},
        {"port",       required_argument, 0, 'p'},
        {"cmd",        required_argument, 0, 'x'},
        {"upload",     required_argument, 0, 'u'},
        {"download",   required_argument, 0, 'd'},
        {"remote-dir", required_argument, 0, 'r'},
        {"local-dir",  required_argument, 0, 'l'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Parse command line arguments */
    while (1) {
        c = getopt_long(argc, argv, "c:p:x:u:d:r:l:h", 
                       long_options, &option_index);
        
        if (c == -1) {
            break;
        }
        
        switch (c) {
            case 'c':
                cid = parse_integer(optarg);
                break;
            case 'p':
                port = parse_integer(optarg);
                break;
            case 'x':
                command = optarg;
                break;
            case 'u':
                upload_file = optarg;
                break;
            case 'd':
                download_file = optarg;
                break;
            case 'r':
                remote_dir = optarg;
                break;
            case 'l':
                local_dir = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    
    /* Validate required arguments */
    if (cid == 0) {
        fprintf(stderr, "Error: --cid is required\n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    /* Open syslog */
    openlog("vsock-shell-client", LOG_PID, LOG_USER);
    
    /* Connect to server */
    sock_fd = connect_to_server(cid, port);
    
    /* Execute requested operation */
    if (upload_file) {
        printf("Uploading '%s' to '%s' on guest...\n", upload_file, remote_dir);
        file_transfer_run_upload_loop(sock_fd, upload_file, remote_dir);
    } else if (download_file) {
        printf("Downloading '%s' to '%s' on host...\n", download_file, local_dir);
        file_transfer_run_download_loop(sock_fd, download_file, local_dir);
    } else {
        /* Terminal session */
        if (command) {
            printf("Executing: %s\n", command);
        } else {
            printf("Starting interactive shell...\n");
        }
        terminal_session_run(sock_fd, command);
    }
    
    /* Cleanup */
    close(sock_fd);
    closelog();
    
    return EXIT_SUCCESS;
}
