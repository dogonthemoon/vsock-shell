###############################################################################
#    vsock-shell - Library Makefile                                          #
###############################################################################

include ../common.mk

TARGET = libmessagequeue.a
SOURCES = message_queue.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(QUIET_AR)$(AR) rcs $@ $^

message_queue.o: message_queue.c message_queue.h ../include/message.h ../include/common.h

clean:
	$(QUIET_CLEAN)rm -f $(OBJECTS) $(TARGET)
