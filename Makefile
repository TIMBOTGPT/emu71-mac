CC      = clang
ARCH    = arm64
CFLAGS  = -arch $(ARCH) -O2 -w -I shim -I orig $(shell sdl2-config --cflags)
LDFLAGS = -arch $(ARCH) $(shell sdl2-config --libs) -lpthread -framework Cocoa

CORE = fetch opcodes mops satmem keyboard timer portmem engine hpil hpilbuf hpilcmd hpildev
OBJS = $(addprefix build/,$(addsuffix .o,$(CORE))) build/tcpip.o build/main.o build/aspect.o

emu71: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

build/%.o: orig/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build/tcpip_mac.c: orig/tcpip.c
	sed -e 's/FD_SET  clientSockSet;/fd_set clientSockSet;/' -e 's/&timeout) > 0);/\&timeout) > 0 \&\& emu_connected(p->sClient));/' $< > $@

build/tcpip.o: build/tcpip_mac.c shim/winsock_compat.h
	$(CC) $(CFLAGS) -I orig -DIPv6_DUAL -include shim/winsock_compat.h -c $< -o $@

build/aspect.o: src/aspect.m
	$(CC) $(CFLAGS) -c $< -o $@

build/main.o: src/main.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f build/*.o emu71
