all: MPListener

MPListener: mplistener.o protocol-tcp.o protocol-udp.o protocol-icmp.o
	gcc -o MPListener mplistener.o protocol-tcp.o protocol-udp.o protocol-icmp.o -lpthread

mplistener.o: mplistener.c
	gcc -c mplistener.c

protocol-tcp.o: protocol-tcp.c
	gcc -c protocol-tcp.c

protocol-udp.o: protocol-udp.c
	gcc -c protocol-udp.c

protocol-icmp.o: protocol-icmp.c
	gcc -c protocol-icmp.c

clean:
	rm MPListener mplistener.o protocol-tcp.o protocol-udp.o protocol-icmp.o
