// Client Program
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
	int tcpfd, udpfd;
	char buffer[MAXLINE];
	struct sockaddr_in servaddr;
	socklen_t len;

	// Creating TCP socket file descriptor
	if ((tcpfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		cout << "TCP socket creation failed";
		exit(0);
	}

	// Creating UDP socket file descriptor
	if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		cout << "UDP socket creation failed";
		exit(0);
	}

	memset(&servaddr, 0, sizeof(servaddr));

	// Filling Server Information
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	servaddr.sin_addr.s_addr = inet_addr(argv[1]);

	if (connect(tcpfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		cout << "Error: TCP Connect Failed\n";
        exit(0);
	}
	// Receive Welcoming Message
    bzero(buffer, sizeof(buffer));
	read(tcpfd, buffer, sizeof(buffer));
	cout << buffer << "\n";

    string token;
    vector<string> tokens;
	while (1) {
		// Receive User Command
		bzero(buffer, sizeof(buffer));
		fgets(buffer, MAXLINE, stdin);
		tokens.clear();
		stringstream ss(buffer);
		while (getline(ss, token, ' ')) tokens.push_back(token);
		int num = tokens.size();

		// register
		if (tokens[0] == "register" || tokens[0] == "register\n") {
			sendto(udpfd, buffer, sizeof(buffer), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
			bzero(buffer, sizeof(buffer));
			recvfrom(udpfd, buffer, MAXLINE, 0, (struct sockaddr*)&servaddr, &len);
			puts(buffer);
		}

		// login
		else if (tokens[0] == "login" || tokens[0] == "login\n") {
			write(tcpfd, buffer, sizeof(buffer));
			bzero(buffer, sizeof(buffer));
			read(tcpfd, buffer, sizeof(buffer));
			puts(buffer);
		}

		// logout
		else if (tokens[0] == "logout" || tokens[0] == "logout\n") {
			write(tcpfd, buffer, sizeof(buffer));
			bzero(buffer, sizeof(buffer));
			read(tcpfd, buffer, sizeof(buffer));
			puts(buffer);
		}

		// game-rule
		else if (tokens[0] == "game-rule" || tokens[0] == "game-rule\n") {
			sendto(udpfd, buffer, sizeof(buffer), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
			bzero(buffer, sizeof(buffer));
			recvfrom(udpfd, buffer, MAXLINE, 0, (struct sockaddr*)&servaddr, &len);
			puts(buffer);
		}

		// start-game
		else if (tokens[0] == "start-game" || tokens[0] == "start-game\n") {
			write(tcpfd, buffer, sizeof(buffer)); // o
			bzero(buffer, sizeof(buffer));
			read(tcpfd, buffer, sizeof(buffer));
			puts(buffer);

			if (strcmp(buffer, "Please typing a 4-digit number:") == 0) {
				while (1) { // start guessing
					// taking a guess
					bzero(buffer, sizeof(buffer));
					fgets(buffer, MAXLINE, stdin);
					write(tcpfd, buffer, sizeof(buffer));
					// get the response
					bzero(buffer, sizeof(buffer));
					read(tcpfd, buffer, sizeof(buffer));
					// see if correct
					if (strcmp(buffer, "You got the answer!") == 0) {
						puts(buffer);
						break;
					}
					if (strcmp(buffer, "LOSE") == 0) {
						// get last guess hint
						bzero(buffer, sizeof(buffer));
						read(tcpfd, buffer, sizeof(buffer));
						puts(buffer);
						// get lose message
						bzero(buffer, sizeof(buffer));
						read(tcpfd, buffer, sizeof(buffer));
						puts(buffer);
						break;					
					}
					// print the hint
					puts(buffer);
				}
			}
		}

		// exit
		else if (tokens[0] == "exit" || tokens[0] == "exit\n") {
			write(tcpfd, buffer, sizeof(buffer));
			if (num == 1) break;
			else {
				bzero(buffer, sizeof(buffer));
				read(tcpfd, buffer, sizeof(buffer));
				puts(buffer);
			}
		}
		else cout << "Invalid command.\n";
	}
	close(tcpfd);
	close(udpfd);
	return 0;
}
