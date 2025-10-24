###############################################################################
#    vsock-shell - Main Makefile                                             #
###############################################################################

.PHONY: all lib client server clean install

all: lib client server

lib:
	$(MAKE) -C lib

client: lib
	$(MAKE) -C client

server: lib
	$(MAKE) -C server

clean:
	$(MAKE) -C lib clean
	$(MAKE) -C client clean
	$(MAKE) -C server clean

install: all
	@echo "Installing vsock-shell..."
	install -D -m 755 client/vsock-shell-client /usr/local/bin/vsock-shell-client
	install -D -m 755 server/vsock-shell-server /usr/local/bin/vsock-shell-server
	@echo "Installation complete"

uninstall:
	rm -f /usr/local/bin/vsock-shell-client
	rm -f /usr/local/bin/vsock-shell-server
	@echo "Uninstallation complete"

help:
	@echo "vsock-shell build system"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build all components (default)"
	@echo "  lib       - Build message queue library"
	@echo "  client    - Build client executable"
	@echo "  server    - Build server executable"
	@echo "  clean     - Remove all build artifacts"
	@echo "  install   - Install binaries to /usr/local/bin"
	@echo "  uninstall - Remove installed binaries"
	@echo "  help      - Show this help message"
