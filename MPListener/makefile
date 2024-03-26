compiler_flags := -O2 -g -fmessage-length=0 -Wall -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wwrite-strings

all: MPListener

MPListener: mplistener.o protocol-tcp.o protocol-udp.o protocol-icmp.o
	gcc -o MPListener mplistener.o protocol-tcp.o protocol-udp.o protocol-icmp.o -lpthread

mplistener.o: mplistener.c
	gcc -c $(compiler_flags) mplistener.c

protocol-tcp.o: protocol-tcp.c
	gcc -c $(compiler_flags) protocol-tcp.c

protocol-udp.o: protocol-udp.c
	gcc -c $(compiler_flags) protocol-udp.c

protocol-icmp.o: protocol-icmp.c
	gcc -c $(compiler_flags) protocol-icmp.c

clean:
	rm MPListener mplistener.o protocol-tcp.o protocol-udp.o protocol-icmp.o
