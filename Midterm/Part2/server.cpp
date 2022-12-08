// TCP Server
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
#include <sys/shm.h>
#include <sys/ipc.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#define MAXLINE 1024
using namespace std;

// shared memory
struct user_info {
    int cnt;
    bool status[100];
};

int main(int argc, char* argv[]) {

    // shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(struct user_info), IPC_CREAT | 0600);
    void *shm = shmat(shmid, NULL, 0);
    struct user_info* user_db = (struct user_info*)shm;
    user_db->cnt = 0;

    int listenfd, connfd;
    char buffer[MAXLINE];
    socklen_t len;
    struct sockaddr_in cliaddr, servaddr;

    // fill in server info
    bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1]));

    // create listening TCP socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	listen(listenfd, 10);
    cout << "TCP server is running\n";
    // connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);

    while (1) { // handle multiple clients
        
        connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);
        if (!fork()) {

            // assign user id & update number of users
            user_db->cnt += 1;
            int uid = user_db->cnt;
            // change status to online
            user_db->status[uid] = true;

            // output new connection message
            cout << "New connection from " << inet_ntoa(cliaddr.sin_addr) << ":";
            cout << ntohs(cliaddr.sin_port) << " user" << user_db->cnt << "\n";
            // send welcoming message
			string welcome = "Welcome, you are user" + to_string(uid) + "\n";
            bzero(buffer, sizeof(buffer));
			write(connfd, welcome.c_str(), sizeof(welcome));

            while (1) { // receive client commands    

                bzero(buffer, sizeof(buffer));
                read(connfd, buffer, sizeof(buffer));

                // list-users
                if (strcmp(buffer, "list-users\n") == 0) {
                    string msg = "Success:\n";
                    for (int i = 1; i <= user_db->cnt; i++) {
                        if (user_db->status[i] == true)
                            msg = msg + "user" + to_string(i) + "\n";
                    }
                    write(connfd, msg.c_str(), sizeof(msg));
                }

                // get-ip
                else if (strcmp(buffer, "get-ip\n") == 0) {
                    string msg = "Success:\nIP: " + string(inet_ntoa(cliaddr.sin_addr));
                    msg += ":" + to_string(ntohs(cliaddr.sin_port)) + "\n";
                    write(connfd, msg.c_str(), sizeof(msg));
                }

                // exit
                else if (strcmp(buffer, "exit\n") == 0) {
                    // change status to offline
                    user_db->status[uid] = false;
                    // output disconnect message
					cout << "user" << uid << " " << inet_ntoa(cliaddr.sin_addr);
                    cout << ":" << ntohs(cliaddr.sin_port) << " disconnected\n";
                    // send bye message
					string bye = "Success:\nBye, user" + to_string(uid) + "\n";
					write(connfd, bye.c_str(), sizeof(bye));
					break;                    
                }

                // others
                else {
                    string msg = "Invalid Command.\n";
                    write(connfd, msg.c_str(), sizeof(msg));
                }
            }

            close(listenfd);
            close(connfd);
            exit(0);
        }
    }

    close(listenfd);
    close(connfd);
    return 0;
}