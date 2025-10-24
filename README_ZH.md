# vsock-shell

一个基于VM sockets的轻量级远程shell和文件传输工具，专为Linux虚拟机间通信设计。

## 功能特性

- **远程Shell访问** - 通过VM sockets在虚拟机间建立安全的shell会话
- **文件传输** - 支持上传和下载文件，具有高效的数据传输机制
- **零配置** - 无需额外配置，直接使用VM sockets进行通信
- **高性能** - 优化的消息队列和缓冲区管理，提供高效的数据传输
- **多会话支持** - 服务器可同时处理多个客户端连接
- **终端兼容** - 完整支持终端控制序列和窗口大小调整

## 系统要求

- Linux操作系统
- 支持VM sockets的内核 (Linux 4.0+)
- GCC编译器
- Make构建工具

## 构建与安装

### 克隆仓库

```bash
git clone https://github.com/dogonthemoon/vsock-shell.git
cd vsock-shell
```

### 编译

```bash
make
```

这将编译所有组件：客户端、服务器和共享库。

### 安装

```bash
sudo make install
```

默认安装位置：
- 客户端：`/usr/local/bin/vsock-shell-client`
- 服务器：`/usr/local/bin/vsock-shell-server`
- 库文件：`/usr/local/lib/libvsock-shell.so`

### 清理

```bash
make clean
```

## 使用方法

### 启动服务器

在目标虚拟机上启动服务器：

```bash
vsock-shell-server [选项]
```

选项：
- `-p, --port PORT` - 指定监听端口 (默认: 5000)
- `-d, --daemon` - 以守护进程模式运行
- `-v, --verbose` - 启用详细日志输出

示例：
```bash
# 在端口5000上启动服务器
vsock-shell-server -p 5000

# 以守护进程模式运行
vsock-shell-server -d -p 5000
```

### 客户端连接

在客户端虚拟机上连接到服务器：

```bash
vsock-shell-client [选项] CID [命令]
```

选项：
- `-p, --port PORT` - 指定连接端口 (默认: 5000)
- `-u, --upload LOCAL_PATH:REMOTE_PATH` - 上传文件
- `-d, --download REMOTE_PATH:LOCAL_PATH` - 下载文件

参数：
- `CID` - 目标虚拟机的上下文ID
- `命令` - 可选，在远程服务器上执行的命令

示例：
```bash
# 连接到CID为3的虚拟机，启动交互式shell
vsock-shell-client 3

# 在远程服务器上执行单个命令
vsock-shell-client 3 "ls -la /home"

# 上传文件到远程服务器
vsock-shell-client 3 -u /local/file.txt:/remote/file.txt

# 从远程服务器下载文件
vsock-shell-client 3 -d /remote/file.txt:/local/file.txt
```

## 架构概述

vsock-shell采用模块化设计，主要包含以下组件：

```
vsock-shell/
├── client/          # 客户端实现
│   ├── main.c              # 主程序入口
│   ├── terminal_client.c   # 终端客户端实现
│   └── file_transfer_client.c # 文件传输客户端实现
├── server/          # 服务器实现
│   ├── main.c              # 主程序入口
│   ├── terminal_server.c   # 终端服务器实现
│   └── file_transfer_server.c # 文件传输服务器实现
├── lib/             # 共享库
│   ├── message_queue.c     # 消息队列实现
│   └── message_queue.h     # 消息队列接口
└── include/         # 公共头文件
    ├── common.h            # 公共定义和工具函数
    ├── message.h           # 消息结构定义
    └── protocol.h          # 协议定义
```

## 协议设计

vsock-shell使用自定义协议进行通信，协议基于消息队列实现可靠传输。

### 消息结构

```c
typedef struct {
    uint32_t magic;      // 协议幻数 (0xCAFEBABE)
    uint8_t type;        // 消息类型
    uint16_t length;     // 数据长度
    uint8_t data[MAX_MESSAGE_DATA]; // 数据载荷
} Message;
```

### 消息类型

- `MSG_TYPE_PTY_DATA` - 终端数据
- `MSG_TYPE_OPEN_BASH` - 打开Bash会话
- `MSG_TYPE_OPEN_CMD` - 执行单个命令
- `MSG_TYPE_WINDOW_SIZE` - 窗口大小变化
- `MSG_TYPE_CLIENT_DATA` - 客户端输入数据
- `MSG_TYPE_CLIENT_END` - 会话结束
- `MSG_TYPE_FILE_UPLOAD_START` - 开始文件上传
- `MSG_TYPE_FILE_DOWNLOAD_START` - 开始文件下载
- `MSG_TYPE_FILE_DATA` - 文件数据块
- `MSG_TYPE_FILE_DATA_END` - 文件传输结束

## 远程Shell访问

vsock-shell提供完整的远程shell访问功能，支持交互式会话和单命令执行。

### 交互式Shell会话

```bash
# 启动交互式shell会话
vsock-shell-client 3
```

交互式shell会话特性：
- 完整的终端体验，支持所有终端控制序列
- 实时窗口大小同步
- 支持Tab补全、历史命令等shell功能
- 正确处理信号和中断

### 单命令执行

```bash
# 执行单个命令
vsock-shell-client 3 "ls -la /home"
```

单命令执行特性：
- 命令执行完成后自动退出
- 返回命令执行状态码
- 适合脚本和自动化场景

## 文件传输

vsock-shell支持高效的文件传输功能，使用分块传输和流量控制机制。

### 文件上传

```bash
# 上传文件到远程服务器
vsock-shell-client 3 -u /local/path/file.txt:/remote/path/file.txt
```

### 文件下载

```bash
# 从远程服务器下载文件
vsock-shell-client 3 -d /remote/path/file.txt:/local/path/file.txt
```

文件传输特性：
- 分块传输，支持大文件
- 进度显示
- 错误检测和恢复
- 流量控制，防止网络拥塞

## 技术细节

### 消息队列机制

vsock-shell使用自定义消息队列实现可靠的数据传输：

- **环形缓冲区** - 高效的内存管理
- **流量控制** - 防止发送方压垮接收方
- **错误处理** - 全面的错误检测和恢复机制
- **批量处理** - 减少系统调用，提高性能

### PTY技术

使用伪终端(PTY)技术提供完整的终端体验：

- **终端模式控制** - 支持原始模式和规范模式
- **窗口大小同步** - 实时同步客户端窗口大小到服务器
- **信号处理** - 正确处理终端信号

### 多路复用I/O

基于select()的事件驱动架构：

- **并发处理** - 支持多个客户端连接
- **高效事件处理** - 避免忙等待和轮询
- **资源管理** - 自动清理资源

## 性能优化

vsock-shell经过多项性能优化：

1. **缓冲区管理** - 优化的缓冲区大小和策略
2. **批量处理** - 合并小消息减少系统调用
3. **零拷贝技术** - 减少内存拷贝操作
4. **异步I/O** - 非阻塞I/O提高并发性能

## 安全考虑

vsock-shell设计用于受信任的虚拟机环境，安全特性包括：

- **VM sockets隔离** - 利用VM sockets的天然隔离特性
- **无明文密码** - 不传输认证信息
- **最小权限** - 以最小必要权限运行

## 故障排除

### 常见问题

1. **连接失败**
   - 检查目标虚拟机的CID是否正确
   - 确认服务器是否在指定端口上运行
   - 验证虚拟机间的网络连接

2. **文件传输失败**
   - 检查文件路径是否正确
   - 确认文件权限是否允许访问
   - 验证磁盘空间是否充足

3. **终端显示异常**
   - 确认终端类型设置正确
   - 检查终端大小是否正确同步
   - 验证终端控制序列是否支持

### 调试模式

启用详细日志输出进行调试：

```bash
# 服务器调试模式
vsock-shell-server -v

# 客户端调试模式
vsock-shell-client -v 3
```

## 开发指南

### 添加新功能

1. 在`protocol.h`中定义新的消息类型
2. 在客户端和服务端实现消息处理逻辑
3. 更新消息队列处理代码
4. 添加相应的命令行选项

### 代码风格

项目遵循Linux内核代码风格：
- 使用4空格缩进
- 函数名使用小写和下划线
- 注释使用C风格`/* */`

## 许可证

本项目采用MIT许可证，详见[LICENSE](LICENSE)文件。

## 贡献

欢迎提交问题报告和拉取请求！

1. Fork本项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建拉取请求

## 联系方式

如有问题或建议，请通过以下方式联系：

- 提交Issue: https://github.com/dogonthemoon/vsock-shell/issues

## 致谢

感谢所有为本项目做出贡献的开发者和用户。

**其他语言版本: [English](README.md), [中文](README_zh.md).**
