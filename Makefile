CC      = clang
ARCH    = arm64
CFLAGS  = -arch $(ARCH) -O2 -w -I shim -I orig $(shell sdl2-config --cflags)
LDFLAGS = -arch $(ARCH) $(shell sdl2-config --libs) -lpthread -framework Cocoa

CORE = fetch opcodes satmem keyboard timer portmem engine hpil hpilbuf hpilcmd hpildev
OBJS = $(addprefix build/,$(addsuffix .o,$(CORE))) build/tcpip.o build/mops.o build/video.o build/main.o build/aspect.o

emu71: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

build/%.o: orig/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build/tcpip_mac.c: orig/tcpip.c
	sed -e 's/FD_SET  clientSockSet;/fd_set clientSockSet;/' -e 's/&timeout) > 0);/\&timeout) > 0 \&\& emu_connected(p->sClient));/' $< > $@

build/tcpip.o: build/tcpip_mac.c shim/winsock_compat.h
	$(CC) $(CFLAGS) -I orig -DIPv6_DUAL -include shim/winsock_compat.h -c $< -o $@

# mops.c with one extra case in MountPorts for TYPE_VIDEO (orig/ stays untouched, same idea as tcpip)
build/mops_vid.c: orig/mops.c
	perl -pe 's/^(\s*)case TYPE_HPIL:/$$1case TYPE_VIDEO: { extern HANDLE AllocVideoMem(UINT); extern BOOL AttachVideoMem(PPORTACC *, HANDLE); h = AllocVideoMem(psData->sInfo.nType); AttachVideoMem(&psExtPorts[i],h); bAttached = TRUE; break; }\n$$1case TYPE_HPIL:/' $< > $@
	grep -q TYPE_VIDEO $@

build/mops.o: build/mops_vid.c
	$(CC) $(CFLAGS) -DTYPE_VIDEO=5 -c $< -o $@

build/video.o: src/video.c src/video.h
	$(CC) $(CFLAGS) -c $< -o $@

build/aspect.o: src/aspect.m
	$(CC) $(CFLAGS) -c $< -o $@

build/main.o: src/main.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f build/*.o emu71
