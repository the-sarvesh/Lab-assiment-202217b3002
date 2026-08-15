# Computer Networks Lab Assignment

## Problem 1: TCP Campus Announcement System

```bash
cd Problem1_TCP
gcc -Wall -Wextra server.c -o server -pthread
gcc -Wall -Wextra client.c -o client -pthread
```

Run in separate terminals:

```bash
./server
./client STUDENT1
./client STUDENT2
./client FACULTY
```

Faculty announcement example:

```text
ANNOUNCE The Computer Networks class begins at 10 AM tomorrow.
```

Type `exit` in `STUDENT2` to demonstrate the leave notification.

## Problem 2: UDP Live Sports Score Broadcasting System

```bash
cd Problem2_UDP
gcc -Wall -Wextra server.c -o server -pthread
gcc -Wall -Wextra client.c -o client
```

Run in separate terminals:

```bash
./server
./client VIEWER1
./client VIEWER2
```

After both clients display their registration confirmation, press Enter in the
server terminal. It broadcasts six updates at three-second intervals, including
Live, Innings Break, and Finished states. Both clients exit after the final
match status.

## Screenshot checklist

1. TCP server with multiple joined clients.
2. TCP announcement visible in Faculty and student terminals.
3. TCP join and leave notifications.
4. UDP server startup and two registered clients.
5. UDP server broadcasting updates and both clients receiving them.
6. UDP Finished status and graceful client exit.
