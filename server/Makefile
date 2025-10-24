###############################################################################
#    vsock-shell - Server Makefile                                           #
###############################################################################

include ../common.mk

TARGET = vsock-shell-server
SOURCES = main.c terminal_server.c file_transfer_server.c
OBJECTS = $(SOURCES:.c=.o)

# Link with library
LDFLAGS += -L../lib
LIBS += -lmessagequeue

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) ../lib/libmessagequeue.a
	$(QUIET_LINK)$(CC) $(ALL_CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

main.o: main.c terminal_server.h ../include/common.h

terminal_server.o: terminal_server.c terminal_server.h file_transfer_server.h \
	../lib/message_queue.h ../include/common.h ../include/protocol.h

file_transfer_server.o: file_transfer_server.c file_transfer_server.h \
	terminal_server.h ../lib/message_queue.h ../include/common.h

clean:
	$(QUIET_CLEAN)rm -f $(OBJECTS) $(TARGET)
