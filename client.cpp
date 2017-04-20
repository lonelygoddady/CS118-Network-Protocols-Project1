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

using namespace std;

int max_file_size = 100*1024*1024; //max file size to be sent is 100 MiB
int port_num = -1; 
char* host_ip;
char* f_name;
int buf_s = 1024; //buffer size to read from file
int time_out = 10;
struct timeval tv;              //Time value struct declaration
fd_set write_fds;

void Socket_send(int sockfd, char* buffer, int send_size); 

//if connection is down, send() return sigpipe signal
void Sigpipe_handler(int s){
    if (s == SIGPIPE){
        cerr << "ERROR: Server is down. Abort." << endl;
        exit(EXIT_FAILURE);
    }   
} 

int main(int argc, char *argv[])
{
  signal(SIGPIPE,Sigpipe_handler);
    
  // check argument number
  if (argc != 4){
      cerr<<"ERROR: Incorrect argument numbers!"<<endl;
      exit(1);
  }

  //assign user specified port_num, IP and filename
  host_ip = argv[1];//gethostbyname(argv[1]);
  port_num = atoi(argv[2]);
  f_name = argv[3]; 
  
  // create a socket (new way)
  struct addrinfo hints;
  struct addrinfo *res;
  int sockfd;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(argv[1], argv[2], &hints, &res) != 0){
      cerr << "ERROR: get addressinfo fails" << endl;
      exit(EXIT_FAILURE);
  }
  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  
  // connect to the server
  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
  
 // if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)   {
  if (connect(sockfd, res->ai_addr, res->ai_addrlen))  { 
      //some other connection error
      if (errno != EINPROGRESS){
          cerr << "ERROR: Error connecting to server" << endl;
          exit(EXIT_FAILURE);    
      }

      //if cannot connect to server immediately, set a 10-second timer
      FD_ZERO(&write_fds);            //Zero out the file descriptor set
      FD_SET(sockfd, &write_fds);     //Set the current socket file descriptor into the set

      //We are going to use select to wait for the socket to connect
      tv.tv_sec = time_out;                  //The second portion of the struct
      tv.tv_usec = 0;                 //The microsecond portion of the struct
        
      int select_ret = select(sockfd+1, NULL, &write_fds, NULL, &tv);
      if(select_ret == 0)
        {
            //connection timeout
            close(sockfd);
            cerr<<"ERROR: Attempt to connect to the server timedout!"<<endl;
            exit(EXIT_FAILURE);
        }
      else if (select_ret < 0 )
        {
            close(sockfd);
            cerr<<"ERROR: Error with select() method!"<<endl;
            exit(EXIT_FAILURE);
        }
      else{ //select_ret > 0
          // Socket selected for write 
          socklen_t lon = sizeof(int); 
          int valopt;
          if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) { 
            cerr << "ERROR: getsockopt" << endl;  
            exit(EXIT_FAILURE); 
          } 
          // Check the value returned... 
          if (valopt) { 
            cerr << "ERROR: Cannot connect o server" << endl;  
            exit(EXIT_FAILURE); 
          } 
      }
  }
  
  //check the result of succeed connection attempt 
  struct sockaddr_in clientAddr;
  socklen_t clientAddrLen = sizeof(clientAddr);
  if (getsockname(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen) == -1)   {
    cerr<<"ERROR: Unable to get sockname"<<endl;
    exit(EXIT_FAILURE);
  }

  char ipstr[INET_ADDRSTRLEN] = {'\0'}; 
  inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
  cout << "Set up a connection from: " << ipstr << ":" << ntohs(clientAddr.sin_port) << endl;

  // send/receive data to/from connection
  char buffer[buf_s];
  ifstream file(f_name, ios::binary|ios::ate);
  int length;
  
  if (file.good()){ //check whether the file is openable
        length = file.tellg(); //get the file size
        if (length > max_file_size){ //check whether file is within the limit
            cerr << "Error: File too large to be sent. Abort"<<endl;
            exit(EXIT_FAILURE);
        }
        file.seekg (0,file.beg); //reset file reading position
  }
  else {
      cerr << "ERROR: Fail to open the file" << endl;
      exit(EXIT_FAILURE);
  }
  
  while (1) {
        memset(buffer, '\0', buf_s);
        file.read(buffer, buf_s);
        if (file.fail() && !file.eof()){ //&& !file.eof()){  //check whether reading from file succeeds 
            cerr << "ERROR: Error durring reading from file" << endl;
            exit(EXIT_FAILURE);
        }
         
        //cout << "Send data from file:" << buffer << endl;
        //cout << "Read size: " << file.gcount() << endl;
	    Socket_send(sockfd, buffer, file.gcount());

        //cout << file.tellg() << endl;
        if (file.eof()){ //stop sending if has reached the end of file
            cout << "file length is: "<< length << endl;
            break;
        }            
  }   
  
  close(sockfd);
  exit(0);
}

void Socket_send(int sockfd, char* buffer, int send_size){
     //if cannot connect to server immediately, set a 10-second timer
      FD_ZERO(&write_fds);            //Zero out the file descriptor set
      FD_SET(sockfd, &write_fds);     //Set the current socket file descriptor into the set

      //We are going to use select to wait for the socket to connect
      tv.tv_sec = 10;                  //The second portion of the struct
      tv.tv_usec = 0;                 //The microsecond portion of the struct
        
      int select_ret = select(sockfd+1, NULL, &write_fds, NULL, &tv);
      if(select_ret == 0)
        {
            //connection timeout
            close(sockfd);
            cerr<<"ERROR: Attempt to connect to the server timedout!"<<endl;
            exit(EXIT_FAILURE);
        }
      else if (select_ret == -1)
        {
            close(sockfd);
            cerr<<"ERROR: Error with select() method!"<<endl;
            exit(EXIT_FAILURE);
        }
      else{
            int byte_s = send(sockfd, buffer, send_size, 0);
            if (byte_s < 0){
                cerr << "ERROR: Error sending data" << endl;
                exit(EXIT_FAILURE);
            }
            //check whether msg pass through 
            //cout << buffer << endl;
            //cout << "bytes sent: " << byte_s << endl;
            //cout << "bytes should be sent " << strlen(buffer) << endl;
      }
}
