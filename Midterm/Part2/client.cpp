// TCP Client 
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <strings.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#define MAXLINE 1024
using namespace std;

int main(int argc, char* argv[]) {

    int tcpfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr;
	socklen_t len;

    // fill in server info
    bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	servaddr.sin_addr.s_addr = inet_addr(argv[1]);

    // create TCP socket file descriptor
    tcpfd = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(tcpfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		cout << "Error: TCP Connect Failed\n";
        exit(0);
	}

    // receive welcoming message
    bzero(buffer, sizeof(buffer));
    read(tcpfd, buffer, sizeof(buffer));
    cout << buffer;

    while (1) {
        // receive user command
        cout << "% ";
		bzero(buffer, sizeof(buffer));
		fgets(buffer, MAXLINE, stdin);
        // exit flag
        bool leave = false;
        if (strcmp(buffer, "exit\n") == 0) leave = true;

        // send to the server & get response
        write(tcpfd, buffer, sizeof(buffer));
        bzero(buffer, sizeof(buffer));
        read(tcpfd, buffer, sizeof(buffer));
        cout << buffer;
        // exit
        if (leave) break;
    }

    close(tcpfd);
    return 0;
}