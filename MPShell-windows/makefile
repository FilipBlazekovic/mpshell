compiler_flags := -O2 -g -fmessage-length=0 -Wall -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wwrite-strings

all: MPShell.exe

MPShell.exe: mpshell.o protocol-tcp.o protocol-udp.o protocol-icmp.o mpshell-common.o
	gcc -o MPShell.exe mpshell.o protocol-tcp.o protocol-udp.o protocol-icmp.o mpshell-common.o -lws2_32 -lIphlpapi

mpshell.o: mpshell.c
	gcc -c $(compiler_flags) mpshell.c

protocol-tcp.o: protocol-tcp.c
	gcc -c $(compiler_flags) protocol-tcp.c

protocol-udp.o: protocol-udp.c
	gcc -c $(compiler_flags) protocol-udp.c

protocol-icmp.o: protocol-icmp.c
	gcc -c $(compiler_flags) protocol-icmp.c

mpshell-common.o: ../MPCommon/mpshell-common.c
	gcc -c $(compiler_flags) ../MPCommon/mpshell-common.c -o mpshell-common.o

clean:
	rm MPShell.exe mpshell.o protocol-tcp.o protocol-udp.o protocol-icmp.o mpshell-common.o
