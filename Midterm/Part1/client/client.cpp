// UDP Client
#include <iostream>
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
#include <vector>
#include <strings.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <fstream>  
#define MAXLINE 1024
using namespace std;

int main(int argc, char *argv[]) {

    int udpfd;
	char buffer[MAXLINE];
	struct sockaddr_in servaddr;
    socklen_t len;

	// Create UDP Socket File Descriptor
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&servaddr, sizeof(servaddr));

	// Fill in Server Info
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	servaddr.sin_addr.s_addr = inet_addr(argv[1]);

    // Receive User Commands
    string token;
    vector<string> tokens;
    while (1) {

        bzero(buffer, sizeof(buffer));
        cout << "% ";
        fgets(buffer, MAXLINE, stdin);
        tokens.clear();
		stringstream ss(buffer);
		while (getline(ss, token, ' ')) tokens.push_back(token);
		int num = tokens.size();

        // get-file-list
        if (strcmp(buffer, "get-file-list\n") == 0) {
            sendto(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
            bzero(buffer, sizeof(buffer));
            recvfrom(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&servaddr, &len);
            cout << buffer;
        }

        // get-file
        else if (strncmp(buffer, "get-file", 8) == 0) {
            sendto(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
            bzero(buffer, sizeof(buffer));
            int cnt = num - 1;
            while (cnt--) {
                recvfrom(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&servaddr, &len);
                stringstream fs(buffer);
                ofstream file_output(fs.str());
                bzero(buffer, sizeof(buffer));
                recvfrom(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&servaddr, &len);
                file_output << buffer;
            }
        }

        // exit
        else if (strcmp(buffer, "exit\n") == 0) {
            sendto(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
            break;
        }

        else cout << "Invalid Command\n";
    }

    close(udpfd);
    return 0;
}
