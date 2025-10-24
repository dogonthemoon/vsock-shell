###############################################################################
#    vsock-shell - Client Makefile                                           #
###############################################################################

include ../common.mk

TARGET = vsock-shell-client
SOURCES = main.c terminal_client.c file_transfer_client.c
OBJECTS = $(SOURCES:.c=.o)

# Link with library
LDFLAGS += -L../lib
LIBS += -lmessagequeue

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) ../lib/libmessagequeue.a
	$(QUIET_LINK)$(CC) $(ALL_CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

main.o: main.c terminal_client.h file_transfer_client.h ../include/common.h

terminal_client.o: terminal_client.c terminal_client.h \
	../lib/message_queue.h ../include/common.h ../include/protocol.h

file_transfer_client.o: file_transfer_client.c file_transfer_client.h \
	../lib/message_queue.h ../include/common.h ../include/protocol.h

clean:
	$(QUIET_CLEAN)rm -f $(OBJECTS) $(TARGET)
