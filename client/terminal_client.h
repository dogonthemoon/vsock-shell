/*****************************************************************************/
/*    vsock-shell - Terminal client interface                               */
/*****************************************************************************/
#ifndef VSOCK_SHELL_TERMINAL_CLIENT_H
#define VSOCK_SHELL_TERMINAL_CLIENT_H

/* Terminal modes */
void terminal_enter_raw_mode(void);
void terminal_restore_mode(void);
void terminal_show_cursor(void);
void terminal_hide_cursor(void);

/* Terminal session */
void terminal_session_run(int socket_fd, const char *command);

/* Window size handling */
void terminal_send_window_size(int socket_fd);
void terminal_setup_sigwinch_handler(int signal_pipe_fd);

/* Initialization */
void terminal_server_init(int signal_pipe_fd);

#endif /* VSOCK_SHELL_TERMINAL_CLIENT_H */
