include Makefile.cfg
all:
	$(MAKE) -C src/
clean:
	$(MAKE) -C src/ clean
install:
	$(MAKE) -C src/ install
	install -d $(PREFIX)/share/man/man1
	install ctunnel.1 $(PREFIX)/share/man/man1
uninstall:
	$(MAKE) -C src/ uninstall
	rm -f $(PREFIX)/share/man/man1/ctunnel.1
