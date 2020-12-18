all: MPShell.exe

MPShell.exe: mpshell.o protocol-tcp.o protocol-udp.o protocol-icmp.o
	gcc -o MPShell.exe mpshell.o protocol-tcp.o protocol-udp.o protocol-icmp.o -lws2_32 -lIphlpapi

mpshell.o: mpshell.c
	gcc -c mpshell.c

protocol-tcp.o: protocol-tcp.c
	gcc -c protocol-tcp.c

protocol-udp.o: protocol-udp.c
	gcc -c protocol-udp.c

protocol-icmp.o: protocol-icmp.c
	gcc -c protocol-icmp.c

clean:
	rm MPShell.exe mpshell.o protocol-tcp.o protocol-udp.o protocol-icmp.o
