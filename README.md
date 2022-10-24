# chat_server
A Multithreaded TCP/IP Server. Currently setup to run on localhost 127.0.0.1.

Uses multiplexing via poll() to assign multiple clients to a single thread.
You can change the max clients that can connect by chaning the value of MAX_CONNS. 
