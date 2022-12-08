// UDP Server
#include <iostream>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <filesystem>
#include <vector>
#include <fstream>  
#include <sstream>
#define MAXLINE 1024
using namespace std;

int main(int argc, char *argv[]) {

    int udpfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;

    // Fill in Server Info
    bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1]));

    // Create UDP Socket & Bind to Server Address
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    bind(udpfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    cout << "UDP server is running\n";

    while(1) {
        // Receive Client's Command
        bzero(buffer, sizeof(buffer));
        len = sizeof(cliaddr);
        recvfrom(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, &len);

        // get-file-list
        if (strcmp(buffer, "get-file-list\n") == 0) {
            string path = filesystem::current_path();
            string msg = "File: ";
            for (const auto & entry : filesystem::directory_iterator(".")) {
                string tmp = entry.path();
                tmp.erase(tmp.begin()); 
                tmp.erase(tmp.begin());
                msg += tmp + " ";
            }
            msg += "\n";
            sendto(udpfd, msg.c_str(), sizeof(buffer), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
        }

        // get-file
        else if (strncmp(buffer, "get-file", 8) == 0) {
            // split the received message
            string token;
            vector<string> tokens;
            stringstream ss(buffer);
            while (getline(ss, token, ' ')) tokens.push_back(token);
            int num = tokens.size();
            
            for (int i = 1; i < num; i++) {
                if (tokens[i][tokens[i].length()-1] == '\n') tokens[i].pop_back();
                sendto(udpfd, tokens[i].c_str(), sizeof(tokens[i]), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
                bzero(buffer, sizeof(buffer));
                stringstream ts(tokens[i]), fs;
                ifstream file_in(ts.str());
                fs << file_in.rdbuf();
                string contents = fs.str();
                sendto(udpfd, contents.c_str(), sizeof(contents), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
            }
        }
    }
    return 0;
}
