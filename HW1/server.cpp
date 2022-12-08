// Server Program
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

// user data in shared memory
struct user_data {
	int user_cnt;
	char username[20][100];
	char email[20][100];
	char password[20][100];
	pid_t login_process[100];
};

// 4-bit number check in start-game
bool bit_check(const char buffer[]) {
	// check number
	for (int i = 0; i < 4; i++) { if (!isdigit(buffer[i])) return false; }
	// check 4 bit
	if (buffer[4] == '\n') return true;
	return false;
}

// generate XAXB hint
string generate_hint(char buffer[], char answer[]) {
	vector<bool> v(10, false);
	int A = 0, B = 0;
	// Count A
	for (int i = 0; i < 4; i++) {
		if (buffer[i] == answer[i]) {
			A += 1;
			v[buffer[i] - '0'] = true;
		}
	}
	// Count B
	for (int i = 0; i < 4; i++) {
		int n = buffer[i] - '0';
		if (v[n]) continue; // already an A
		else v[n] = true;
		for (int j = i + 1; j < i + 4; j++) {
			if (buffer[i] == answer[j % 4]) {
				B += 1;
				v[n] = true;
				break;
			}
		}
	}
	string result = to_string(A) + "A" + to_string(B) + "B";
	return result;
}

int max(int x, int y) {
	if (x > y) return x;
	else return y;
}

int main(int argc, char* argv[]) {

	// shared memory
	int shmid = shmget(IPC_PRIVATE, sizeof(struct user_data), IPC_CREAT | 0600);
    void *shm = shmat(shmid, NULL, 0);
    struct user_data* user_database = (struct user_data*)shm;
	user_database->user_cnt = 0;

	int listenfd, connfd, udpfd, nready, maxfdp;
	char buffer[MAXLINE];
	fd_set rset;
	ssize_t n;
	socklen_t len;
	struct sockaddr_in cliaddr, servaddr;
	char message[MAXLINE] = "*****Welcome to Game 1A2B*****";
	void sig_chld(int);

	// Create Listening TCP Socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	// Set Server to SO_REUSEADDR
	const int enable = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		printf("setsockopt(SO_REUSEADDR) failed");
		exit(0);
	}
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1]));
	bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	listen(listenfd, 10);
    cout << "TCP server is running\n";

	// Create UDP Socket
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	bind(udpfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    cout << "UDP server is running\n";

	// clear the descriptor set
	FD_ZERO(&rset);
	// get maxfd
	maxfdp = max(listenfd, udpfd) + 1;

	while (1) {

		// set listenfd and udpfd in readset
		FD_SET(listenfd, &rset);
		FD_SET(udpfd, &rset);
		// select the ready descriptor
		nready = select(maxfdp, &rset, NULL, NULL, NULL);
		// sending welcoming message
		len = sizeof(cliaddr);
		connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);

		if (!fork()) { // handle new client
			cout << "New connection.\n";
			write(connfd, (const char*)message, sizeof(buffer));
			while (1) { // handle the client's commands

				FD_SET(connfd, &rset);
				FD_SET(udpfd, &rset);
				maxfdp = max(connfd, udpfd) + 1;
				nready = select(maxfdp, &rset, NULL, NULL, NULL);

				// Receive message via TCP
				if (FD_ISSET(connfd, &rset)) {
					bzero(buffer, sizeof(buffer));
					read(connfd, buffer, sizeof(buffer));

					// split the received message
					string token;
					vector<string> tokens;
					stringstream ss(buffer);
					while (getline(ss, token, ' ')) {
						if (token != "\n") tokens.push_back(token);
					}
					int num = tokens.size();

					// login
					if (tokens[0] == "login" || tokens[0] == "login\n") {
						// check format
						if (num == 3) { // correct
							// check if already logined
							bool logined = false;
							for (int i = 0; i < user_database->user_cnt; i++) {
								if (user_database->login_process[i] == getpid()) {
                                    logined = true; break;
                                }
							}
							if (logined) {
								char msg[MAXLINE] = "Please logout first.";
                                write(connfd, msg, sizeof(buffer));
                                continue;
							}
							// check user exists and password
							bool exist = false;
							bool password_correct = false;
							int user_index = -1; // record the user's index if exist
							for (int i = 0; i < user_database->user_cnt; i++) {
								if (strcmp(user_database->username[i], tokens[1].c_str()) == 0) {
									exist = true;
									if (strcmp(user_database->password[i], tokens[2].c_str()) == 0)
										password_correct = true;
									user_index = i;
									break;
								}
							}
							if (exist && password_correct) {
								// different clients cannot login the same account at the same time
								if (user_database->login_process[user_index] == 0) { // not occupied
									user_database->login_process[user_index] = getpid();
									string tmp = "Welcome, " + string(user_database->username[user_index]) + ".";
									char msg[MAXLINE];
									strcpy(msg, tmp.c_str());
									write(connfd, msg, sizeof(buffer));
								} else { // occupied
									char msg[MAXLINE] = "Please logout first.";
									write(connfd, msg, sizeof(buffer));
								}		
							} else if (exist) { // wrong password
								char msg[MAXLINE] = "Password not correct.";
								write(connfd, msg, sizeof(buffer));
							} else { // user not exist
								char msg[MAXLINE] = "Username not found.";
								write(connfd, msg, sizeof(buffer));								
							}
						} else { // incorrect
							char msg[MAXLINE] = "Usage: login <username> <password>";
							write(connfd, msg, sizeof(buffer));
						}
					}

					// logout
					else if (tokens[0] == "logout" || tokens[0] == "logout\n") {
						// check format
						if (num == 1) { // correct
							// check if logined
							bool logined = false;
							int user_index = -1;
							for (int i = 0; i < user_database->user_cnt; i++) {
                                if (user_database->login_process[i] == getpid()) {
                                    logined = true;
									user_index = i;
                                    break;
                                }
							}
							if (logined) {
								user_database->login_process[user_index] = 0;
								string tmp = "Bye, " + string(user_database->username[user_index]) + ".";
								char msg[MAXLINE];
								strcpy(msg, tmp.c_str());
								write(connfd, msg, sizeof(buffer));
							} else {
								char msg[MAXLINE] = "Please login first.";
								write(connfd, msg, sizeof(buffer));
							}

						} else { // incorrect
							char msg[MAXLINE] = "Usage: logout";
							write(connfd, msg, sizeof(buffer));
						}
					}

					// start-game
					else if (tokens[0] == "start-game" || tokens[0] == "start-game\n") {
						// check format
						if (num == 1 || num == 2) { // correct
							// check logined
							bool logined = false;
							for (int i = 0; i < user_database->user_cnt; i++) {
                                if (user_database->login_process[i] == getpid()) {
                                    logined = true;
                                    break;
                                }
                            }
							if (logined) {
								if (num == 2) { // answer given
									if (!bit_check(tokens[1].c_str())) {
                                        char msg[MAXLINE] = "Usage: start-game <4-digit number>";
                                        write(connfd, msg, sizeof(buffer));
                                        continue;										
									}
									char msg[MAXLINE] = "Please typing a 4-digit number:";
									write(connfd, msg, sizeof(buffer));
									char answer[MAXLINE]; strcpy(answer, tokens[1].c_str());
									int cnt = 5;
									while (1) { // start guessing
										bzero(buffer, sizeof(buffer));
										read(connfd, buffer, sizeof(buffer));
										// check 4 bit number
										if (!bit_check(buffer)) {
											char msg[MAXLINE] = "Your guess should be a 4-digit number.";
											write(connfd, msg, sizeof(buffer));
											continue;
										}
										// receive user response
										if (strcmp(buffer, answer) == 0) { // correct
											char msg[MAXLINE] = "You got the answer!";
											write(connfd, msg, sizeof(buffer));
											break;
										}
										// out of guesses
										cnt -= 1;
										if (cnt == 0) {
											char signal[MAXLINE] = "LOSE";
											write(connfd, signal, sizeof(buffer));
											char msg1[MAXLINE];
											strcpy(msg1, generate_hint(buffer, answer).c_str()); // hint
											write(connfd, msg1, sizeof(buffer));
											char msg2[MAXLINE] = "You lose the game!"; // lose message
											write(connfd, msg2, sizeof(buffer));
											break;											
										}
										// send hint
										char msg[MAXLINE];
										strcpy(msg, generate_hint(buffer, answer).c_str()); // hint
										write(connfd, msg, sizeof(buffer));
									}
								} else { // answer not given
									char msg[MAXLINE] = "Please typing a 4-digit number:";
									write(connfd, msg, sizeof(buffer));
									char answer[MAXLINE] = "1234\n";
									int cnt = 5;
									while (1) { // start guessing
										bzero(buffer, sizeof(buffer));
										read(connfd, buffer, sizeof(buffer));
										// check 4 bit number
										if (!bit_check(buffer)) {
											char msg[MAXLINE] = "Your guess should be a 4-digit number.";
											write(connfd, msg, sizeof(buffer));
											continue;
										}
										// receive user response
										if (strcmp(buffer, answer) == 0) { // correct
											char msg[MAXLINE] = "You got the answer!";
											write(connfd, msg, sizeof(buffer));
											break;
										}
										// out of guesses
										cnt -= 1;
										if (cnt == 0) {
											char signal[MAXLINE] = "LOSE";
											write(connfd, signal, sizeof(buffer));
											char msg1[MAXLINE];
											strcpy(msg1, generate_hint(buffer, answer).c_str()); // hint
											write(connfd, msg1, sizeof(buffer));
											char msg2[MAXLINE] = "You lose the game!"; // lose message
											write(connfd, msg2, sizeof(buffer));		
											break;									
										}
										// send hint
										char msg[MAXLINE];
										strcpy(msg, generate_hint(buffer, answer).c_str()); // hint
										write(connfd, msg, sizeof(buffer));
									}
								}
							} else { // not logined
								char msg[MAXLINE] = "Please login first.";
								write(connfd, msg, sizeof(buffer));
							}

						} else { // incorrect
							char msg[MAXLINE] = "Usage: start-game <4-digit number>";
							write(connfd, msg, sizeof(buffer));
						}
					}

					// exit
					else if (tokens[0] == "exit" || tokens[0] == "exit\n") {
						// check format
						if (num == 1) { // correct
							cout << "tcp get msg: exit\n";
							// if logined, logout client
							bool logined = false;
							int user_index = -1;
							for (int i = 0; i < user_database->user_cnt; i++) {
                                if (user_database->login_process[i] == getpid()) {
                                    logined = true;
									user_index = i;
                                    break;
                                }
							}
							if (logined) user_database->login_process[user_index] = 0;
							break;
						} else { // incorrect
							char msg[MAXLINE] = "Usage: exit";
							write(connfd, msg, sizeof(buffer));
						}
					}
				}
				// Receive message via UDP
				if (FD_ISSET(udpfd, &rset)) {
					bzero(buffer, sizeof(buffer));
					recvfrom(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, &len);

					// split the received message
					string token;
					vector<string> tokens;
					stringstream ss(buffer);
					while (getline(ss, token, ' ')) {
						if (token != "\n") tokens.push_back(token);
					}
					int num = tokens.size();

					// register
					if (tokens[0] == "register" || tokens[0] == "register\n") {
						// register <username> <email> <password>
						if (num == 4) {
							bool valid = true;
							// check if unique
							for (int i = 0; i < user_database->user_cnt; i++) {
								if (strcmp(user_database->username[i], tokens[1].c_str()) == 0) {
									char msg1[MAXLINE] = "Username is already used.";
									sendto(udpfd, msg1, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
									valid = false;
									break;
								}
								if (strcmp(user_database->email[i], tokens[2].c_str()) == 0) {
									char msg2[MAXLINE] = "Email is already used.";
									sendto(udpfd, msg2, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
									valid = false;
									break;
								}
							}
							// add to user_database
							if (valid) {
								strcpy(user_database->username[user_database->user_cnt], tokens[1].c_str());
                                strcpy(user_database->email[user_database->user_cnt], tokens[2].c_str());
                                strcpy(user_database->password[user_database->user_cnt], tokens[3].c_str());
                                user_database->login_process[user_database->user_cnt] = 0;
								user_database->user_cnt += 1;
								char msg[MAXLINE] = "Register successfully.";
								sendto(udpfd, msg, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
							}
						} else {
							char msg[MAXLINE] = "Usage: register <username> <email> <password>";
							sendto(udpfd, msg, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
						}
					}

					// game-rule
					else if (tokens[0] == "game-rule" || tokens[0] == "game-rule\n") {
						// check format
						if (num == 1) { // correct
							char rule[MAXLINE] = "1. Each question is a 4-digit secret number.\n"
							                     "2. After each guess, you will get a hint with the following information:\n"
							                     "2.1 The number of \"A\", which are digits in the guess that are in the correct position.\n"
												 "2.2 The number of \"B\", which are digits in the guess that are in the answer but are in the wrong position.\n"
							                     "The hint will be formatted as \"xAyB\".\n"
							                     "3. 5 chances for each question.";
							sendto(udpfd, rule, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
						} else { // incorrect
							char msg[MAXLINE] = "Usage: game-rule";
							sendto(udpfd, msg, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
						}
					}			
				}
			}
			close(connfd);
			return 0;
		}
		close(connfd);
	}
    return 0;
}
