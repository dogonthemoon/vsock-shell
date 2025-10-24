###############################################################################
#    vsock-shell - Common build configuration                                #
###############################################################################

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I../include
LDFLAGS = -L../lib

# Debug build
ifdef DEBUG
CFLAGS += -g -DDEBUG -O0
endif

# Libraries
LIBS = -lutil

# Common flags
COMMON_CFLAGS = -D_GNU_SOURCE -std=gnu99

# Combine flags
ALL_CFLAGS = $(CFLAGS) $(COMMON_CFLAGS)

# Silent compilation (use V=1 for verbose)
ifndef V
QUIET_CC = @echo "  CC      $@";
QUIET_LINK = @echo "  LINK    $@";
QUIET_AR = @echo "  AR      $@";
QUIET_CLEAN = @echo "  CLEAN   $(TARGET)";
endif

# Build rules
%.o: %.c
	$(QUIET_CC)$(CC) $(ALL_CFLAGS) -c -o $@ $<
