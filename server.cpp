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
using namespace std;

int max_size = 100*1024*1024;
struct timeval tv;
int port_num = -1;
int client_num = 10;
int buf_s = 8192;
char* dir;
struct sigaction sa;
int thread_num = 0;
int time_out = 10;
int sig_flag = 0;

void sigQ_handler(int s)
{
   cout << "SIGQUIT signal receive! Terminate Server" << endl;
   sig_flag = 1;
}

void sigT_handler(int s)
{
   cout << "SIGTERM signal receive! Terminate Server" << endl;
   sig_flag = 1;
}

void sigI_handler(int s)
{
  cout << "SIGINT signal receive! Terminate Server" <<endl;
  sig_flag = 1;
}

void client_handler(int clientSockfd, int th_num);

int main(int argc, char *argv[])
{
  if (argc != 3){
        cerr << "ERROR: Incorrect argument number!" << endl;
        exit(EXIT_FAILURE);
  }

  //assign ports and dir name from user input
  vector<thread> client_threads; 
  port_num = atoi(argv[1]);
  if((port_num > 65535) || (port_num < 2000)){
      cerr << "ERROR: Please enter a port number between 2000 - 65535" << endl;
      exit(EXIT_FAILURE);
  }

  dir = argv[2];

  // create a socket using TCP IP
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  // allow others to reuse the address
  int yes = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    cerr << "ERROR: Error seting sockopt" << endl;
    exit(2);
  }
  
  //new way to set parameter
  struct addrinfo hints, *servinfo, *p;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP address

  if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      exit(EXIT_FAILURE);
  } 
  
  for(p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype,
              p->ai_protocol)) == -1) {
          perror("socket");
          continue;
      }

      if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
          close(sockfd);
          perror("bind");
          continue;
      }
      break; // if we get here, we must have connected successfully
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL)  {
        cerr << "ERROR: server failed to bind" << endl;
        exit(EXIT_FAILURE);
  }

  cout << "Server set up, waiting for connection" << endl;
  cout << "Port number is: " << port_num << endl;
  
  // set socket to listen status
  if (listen(sockfd, client_num) == -1) {
    cerr << "ERROR: error listen to connection" << endl; 
    exit(EXIT_FAILURE);
  }
  
  signal(SIGQUIT,sigQ_handler);
  signal(SIGTERM,sigT_handler);
  //signal(SIGINT,sigI_handler);

  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);      
 
  while (1){
      //terminate the server if sig_flag is set
      if (sig_flag) 
       	break;
	
      struct sockaddr_in clientAddr;
      int clientSockfd = -1; 
      socklen_t clientAddrSize = sizeof(clientAddr);

      clientSockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrSize);
    	
      if (clientSockfd > 0){ 
    		//increment thread_num connected
    		thread_num ++;
    		
    		//get the client ip address and port
    		char ipstr[INET_ADDRSTRLEN] = {'\0'};
        inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
        cout << "Accept a connection from: " << ipstr << ":" << ntohs(clientAddr.sin_port) << endl;
        client_threads.push_back(thread(client_handler,clientSockfd,thread_num));
      }

}

cout << "number of threads: " << thread_num << endl;

//wait for all threads to finish
//for(int i = 0; i < thread_num; i++){
for (vector<thread>::iterator iter = client_threads.begin(); iter != client_threads.end(); iter++){  
    iter->join();
}

    close(sockfd);
    cout << "All connection closed. Server terminates." << endl;

    exit(0);
}

//void client_handler(int clientSockfd,struct sockaddr_in clientAddr){
void client_handler(int clientSockfd, int th_num){
    //set client fd non-blocking mode
    int flags = fcntl(clientSockfd, F_GETFL, 0);
    fcntl(clientSockfd, F_SETFL, flags | O_NONBLOCK);

    if (clientSockfd == -1) {
        cerr << "ERROR: Error accepting connection" << endl;
        exit(EXIT_FAILURE);
    }

    // read/write data from/into the connection
    char buffer[buf_s];
    //std::vector<unsigned char> buffer(buf_s);
    char log_file[buf_s];
    int file_length = 0;

    //file name for this thread
    strcpy(log_file, dir);
    strcat(log_file, "/");
    strcat(log_file, to_string(th_num).c_str());
    strcat(log_file, ".file");
    cout << "file name is: " << log_file << endl;
   
    //open file to be written
    ofstream file(log_file, ios::out|ios::binary|ios::ate);

    if (file.fail()){
        cerr << "ERROR: Error creating/open file" << endl;
        return;    
    }

    while (1) {
        //clear the buffer
        memset(buffer, '\0', buf_s);
        if (sig_flag){
            cout << "Force terminate client handler threads by signal" << endl; 
            return;
        }
        //if cannot connect to server immediately, set a 10-second timer
        fd_set read_fds;
        FD_ZERO(&read_fds);            //Zero out the file descriptor set
        FD_SET(clientSockfd, &read_fds);     //Set the current socket file descriptor into the set

        //We are going to use select to wait for the socket to connect
        tv.tv_sec = time_out;                
        tv.tv_usec = 0;                

        int select_ret = select(clientSockfd+1, &read_fds, NULL, NULL, &tv);
        if(select_ret == 0){
            file.close();
            //reopen the file to overwirte content
            ofstream file(log_file, ios::out);
            strcpy(buffer, "ERROR: Data receiving timeout!");
            file.write(buffer, strlen(buffer));
            cerr << "ERROR: client " << th_num << "data receiving timeout. Abort client connection " << th_num << endl;
            break;
        }
        else if (select_ret == -1){
            close(clientSockfd);
            cerr<<"ERROR: Error with select() method!"<<endl;
            break;
            //exit(EXIT_FAILURE);
        }
        else {
           int rec_val = recv(clientSockfd, buffer, buf_s, 0);
           if (rec_val < 0){
               cerr << "ERROR: Error reading from client socket" << endl;
               close(clientSockfd);
               //exit(EXIT_FAILURE);
               break;
           }

          //cout << buffer << endl;

           //if there is data in the buffer 
           if (rec_val){
                file_length += rec_val;
                if (file_length > max_size){
                    cerr << "ERROR: Cannot receive more than 100 MiB data" << endl;
                    break;
                    //exit(EXIT_FAILURE);
                }
                //write to file
                file.write(buffer, rec_val);
		   
                if (file.fail()){  //check whether reading from file succeed 
                    cerr << "ERROR: Error durring writting to file" << endl;
                    break;
                }
           }

           //client disconnect
           if (rec_val == 0){
               cout << "Client disconnected" << endl;
               break;
           } 
        }     
    }

    //cout << "Receive file size: " << file_length << endl;
    close(clientSockfd);
    return;
}
