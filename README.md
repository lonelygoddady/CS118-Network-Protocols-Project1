UID: 504135743
Name: ZHUOQI LI

Library used for server:
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <signal.h>
#include <thread>
#include <iterator>
#include <vector>

Library used for client:
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>

Client Design:
In order to use the client, user needs to specify the server ip, port number and file to be transferred.
$ ./client SERVERIP PORTNUM FILE 

Before sending file content to the server, client set the file descriptor to be non-blocking and use 'Select' to check whether the server is ready. If timeout or returnerror, the client will not connect to theserver and terminate itself directly. If it realizes the server socket file descrptor is ready, then the client will try to 
build the connection use 'connect'. 

After the connection is built, the client will open the file to be transferred, read its content into binary format and save then into a 512-byte buffer using ifstream method read(). It check whether the file has a size more than 100 MiB. If yes, the client will refuse to send it and shut down. Since contents are read into binary format, there is no file type limit on the file to be transferred. After that, 'Select' will be used again to check whether the connection is still good. If it times out, the client will output error string to stderr and terminate. Otherwise the client will now send this 512-byte buffer over the connection to the server. However, if SIGPIPE signal is catched during sending, the client will terminate and output error message because we know the connection is down. After the previous data went through, the client will procede to read next 512 byte from the file and send them again. This process will repeat untill the read hits the end of the file.

Server Design:
To intialize the server, user needs to specify the port number and directory to save transferred file.
$ ./server PORTNUM DIRECTORY
Server has to be started before clients and client's PORTNUM should be the same as the server.

The server will first bind to the port number and IP address of the local machine and be put to listen incoming connections use 'listen()'. A max number of connections to be listened and put in queue is set to be 10 so no client will be accepted if the server has already had 10 connections. 

To handle multiple client connections, the server utilizes multithreads. The main thread will keep listening on whether there is a client trying to connect. If there is, accept the connection, create a thread and this thread will handle the file transfering part. Since we can have 10 connections simultaneously, therefore our server would have 11 threads including the main thread. running at the same time at max capacity. 

In each thread created, it will first create a file 'NUMBER.file' in the directory specified, which NUMBER stands for the client number this thread is currently handling. After the file is successfully created, the thread will first use the select method to check whether the client file descriptor is ready. If timeout happens, the client thread will write error string to the file opened and terminates itself. If the file descriptor is ready, the server will receive data transferred and store them into a 1024-byte buffer using the 'recv()' method. This buffer will susquently be written into the file created. Then, the thread will proceed to the next round of check-recv-write and this process loops repeatively and the thread finally terminate untill recv() returns 0 indicating the client has finish transfering all data.

The server will always be running after start until SIGQUIT/SIGTERM signals are catched.

Problem & Solutions:
1. Received and send file differ while transfering non .txt file
   solution: Use .gcount() instead of strlen to determine the number of bytes to be send through. strlen only count to the position where the character is '\0'. However   , in non-text files, it is not necessary for '\0' character to be always present at the end of the buffer. It is highly probable that there is a '\0' somewhere in th   e mid of the content itself and therefore strlen wont return the actualy number of bytes read by read(). However, ifstream.gcount() will always return the exact numb   er of bytes read from last ifstream.read() operation.   

2. Number of files created do not match the number of files transferred
   Solution: This was because the threadNum global variable increment operation was accidently put in the created threads and caused race condition while multithreading   The threadNum is incremented more than one times before 'threadNUM.file' is created. This is solved by puting the threadNum increment operation into the main thread.

3. Client does not exit the way it is required to when network disconnection happens while transferring
   Solution: A signal handler is created for SIGPIPE for send() error due to network disconnection. 

4. Connection/data transfer timeout setting
   Solution: Use the select function to set timeout.

5. Client blocking and timeout not triggered
   Solution: Use fnctl to set the file descriptor to be non-blocking. 
