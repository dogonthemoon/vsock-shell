**[English](README.md), [中文](README_ZH.md)**

# vsock-shell

A lightweight remote shell and file transfer tool based on VM sockets, designed for communication between Linux virtual machines.

## Features

- **Remote Shell Access** - Establish secure shell sessions between virtual machines via VM sockets
- **File Transfer** - Support for uploading and downloading files with efficient data transfer mechanisms
- **Zero Configuration** - Direct communication using VM sockets without additional configuration
- **High Performance** - Optimized message queue and buffer management for efficient data transmission
- **Multi-Session Support** - Server can handle multiple client connections simultaneously
- **Terminal Compatibility** - Full support for terminal control sequences and window size adjustment

## System Requirements

- Linux operating system
- Kernel with VM sockets support (Linux 4.0+)
- GCC compiler
- Make build tool

## Build and Installation

### Clone Repository

```bash
git clone https://github.com/dogonthemoon/vsock-shell.git
cd vsock-shell
```

### Compile

```bash
make
```

This will compile all components: client, server, and shared library.

### Install

```bash
sudo make install
```

Default installation locations:
- Client: `/usr/local/bin/vsock-shell-client`
- Server: `/usr/local/bin/vsock-shell-server`
- Library: `/usr/local/lib/libvsock-shell.so`

### Clean

```bash
make clean
```

## Usage

### Start Server

Start the server on the target virtual machine:

```bash
vsock-shell-server [options]
```

Options:
- `-p, --port PORT` - Specify listening port (default: 5000)
- `-d, --daemon` - Run in daemon mode
- `-v, --verbose` - Enable verbose logging

Examples:
```bash
# Start server on port 5000
vsock-shell-server -p 5000

# Run in daemon mode
vsock-shell-server -d -p 5000
```

### Client Connection

Connect to the server from the client virtual machine:

```bash
vsock-shell-client [options] CID [command]
```

Options:
- `-p, --port PORT` - Specify connection port (default: 5000)
- `-u, --upload LOCAL_PATH:REMOTE_PATH` - Upload file
- `-d, --download REMOTE_PATH:LOCAL_PATH` - Download file

Parameters:
- `CID` - Context ID of the target virtual machine
- `command` - Optional, command to execute on the remote server

Examples:
```bash
# Connect to virtual machine with CID 3, start interactive shell
vsock-shell-client 3

# Execute a single command on the remote server
vsock-shell-client 3 "ls -la /home"

# Upload file to remote server
vsock-shell-client 3 -u /local/file.txt:/remote/file.txt

# Download file from remote server
vsock-shell-client 3 -d /remote/file.txt:/local/file.txt
```

## Architecture Overview

vsock-shell adopts a modular design, mainly including the following components:

```
vsock-shell/
├── client/          # Client implementation
│   ├── main.c              # Main program entry point
│   ├── terminal_client.c   # Terminal client implementation
│   └── file_transfer_client.c # File transfer client implementation
├── server/          # Server implementation
│   ├── main.c              # Main program entry point
│   ├── terminal_server.c   # Terminal server implementation
│   └── file_transfer_server.c # File transfer server implementation
├── lib/             # Shared library
│   ├── message_queue.c     # Message queue implementation
│   └── message_queue.h     # Message queue interface
└── include/         # Public header files
    ├── common.h            # Common definitions and utility functions
    ├── message.h           # Message structure definitions
    └── protocol.h          # Protocol definitions
```

## Protocol Design

vsock-shell uses a custom protocol for communication, which implements reliable transmission based on message queues.

### Message Structure

```c
typedef struct {
    uint32_t magic;      // Protocol magic number (0xCAFEBABE)
    uint8_t type;        // Message type
    uint16_t length;     // Data length
    uint8_t data[MAX_MESSAGE_DATA]; // Data payload
} Message;
```

### Message Types

- `MSG_TYPE_PTY_DATA` - Terminal data
- `MSG_TYPE_OPEN_BASH` - Open Bash session
- `MSG_TYPE_OPEN_CMD` - Execute single command
- `MSG_TYPE_WINDOW_SIZE` - Window size change
- `MSG_TYPE_CLIENT_DATA` - Client input data
- `MSG_TYPE_CLIENT_END` - Session end
- `MSG_TYPE_FILE_UPLOAD_START` - Start file upload
- `MSG_TYPE_FILE_DOWNLOAD_START` - Start file download
- `MSG_TYPE_FILE_DATA` - File data block
- `MSG_TYPE_FILE_DATA_END` - File transfer end

## Remote Shell Access

vsock-shell provides complete remote shell access functionality, supporting both interactive sessions and single command execution.

### Interactive Shell Session

```bash
# Start interactive shell session
vsock-shell-client 3
```

Interactive shell session features:
- Complete terminal experience with support for all terminal control sequences
- Real-time window size synchronization
- Support for shell features like Tab completion and command history
- Proper handling of signals and interrupts

### Single Command Execution

```bash
# Execute a single command
vsock-shell-client 3 "ls -la /home"
```

Single command execution features:
- Automatically exits after command execution
- Returns command execution status code
- Suitable for scripting and automation scenarios

## File Transfer

vsock-shell supports efficient file transfer functionality, using chunked transmission and flow control mechanisms.

### File Upload

```bash
# Upload file to remote server
vsock-shell-client 3 -u /local/path/file.txt:/remote/path/file.txt
```

### File Download

```bash
# Download file from remote server
vsock-shell-client 3 -d /remote/path/file.txt:/local/path/file.txt
```

File transfer features:
- Chunked transmission, supports large files
- Progress display
- Error detection and recovery
- Flow control to prevent network congestion

## Technical Details

### Message Queue Mechanism

vsock-shell uses a custom message queue to implement reliable data transmission:

- **Ring Buffer** - Efficient memory management
- **Flow Control** - Prevent sender from overwhelming receiver
- **Error Handling** - Comprehensive error detection and recovery mechanisms
- **Batch Processing** - Reduce system calls and improve performance

### PTY Technology

Uses pseudo-terminal (PTY) technology to provide complete terminal experience:

- **Terminal Mode Control** - Support for raw mode and canonical mode
- **Window Size Synchronization** - Real-time synchronization of client window size to server
- **Signal Handling** - Proper handling of terminal signals

### Multiplexed I/O

Event-driven architecture based on select():

- **Concurrent Processing** - Support for multiple client connections
- **Efficient Event Handling** - Avoid busy waiting and polling
- **Resource Management** - Automatic resource cleanup

## Performance Optimization

vsock-shell has undergone multiple performance optimizations:

1. **Buffer Management** - Optimized buffer size and strategies
2. **Batch Processing** - Merge small messages to reduce system calls
3. **Zero-Copy Technology** - Reduce memory copy operations
4. **Asynchronous I/O** - Non-blocking I/O to improve concurrent performance

## Security Considerations

vsock-shell is designed for trusted virtual machine environments, with security features including:

- **VM Sockets Isolation** - Leverages the natural isolation characteristics of VM sockets
- **No Plain Text Passwords** - No authentication information is transmitted
- **Minimum Privileges** - Runs with minimum necessary privileges

## Troubleshooting

### Common Issues

1. **Connection Failure**
   - Check if the target virtual machine's CID is correct
   - Confirm the server is running on the specified port
   - Verify network connection between virtual machines

2. **File Transfer Failure**
   - Check if file paths are correct
   - Confirm file permissions allow access
   - Verify sufficient disk space

3. **Terminal Display Abnormalities**
   - Confirm terminal type is set correctly
   - Check if terminal size is synchronized correctly
   - Verify terminal control sequence support

### Debug Mode

Enable verbose logging for debugging:

```bash
# Server debug mode
vsock-shell-server -v

# Client debug mode
vsock-shell-client -v 3
```

## Development Guide

### Adding New Features

1. Define new message types in `protocol.h`
2. Implement message handling logic in client and server
3. Update message queue processing code
4. Add corresponding command-line options

### Code Style

The project follows Linux kernel code style:
- Use 4-space indentation
- Function names use lowercase and underscores
- Comments use C style `/* */`

## License

This project is licensed under the MIT License, see the [LICENSE](LICENSE) file for details.

## Contributing

Issue reports and pull requests are welcome!

1. Fork this project
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Create a pull request

## Contact

For questions or suggestions, please contact through:

- Submit Issue: https://github.com/dogonthemoon/vsock-shell/issues

## Acknowledgments

Thanks to all developers and users who have contributed to this project.

