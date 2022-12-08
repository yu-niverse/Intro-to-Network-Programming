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
#include <sys/mman.h>
#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#define MAXLINE 1024
#define PORT 8888
using namespace std;

// user data in shared memory
struct user_data {
	int user_cnt;
	char username[20][100];
	char email[20][100];
	char password[20][100];
    int room[20];
	int login_fd[20];
};

// room data in shared memory
struct room_data {
    int room_cnt;
    int id[20];
    bool pub[20]; // public or private
    char manager[20][100];
    bool started[20];
	int invite_code[20];
	int members[20][20];
	int num_of_members[20];
	char answer[20][4];
	int rounds[20];
	int turn[20];
};

// invitation data in shared memory
struct invite_data {
	int invitations_cnt;
	char inviter_name[50][100];
	char inviter_email[50][100];
	char invitee_email[50][100];
	int room_id[50];
	int invitation_code[50];
};

// check login status
int check_login (int fd, struct user_data * U) {
	for (int u = 0; u < U->user_cnt; u++)
		if (U->login_fd[u] == fd) return u;
	return -1;
}

// 4-bit number check in start-game
bool bit_check (string token) {
	// check 4 bit
	if (token.length() != 4) return false;
	// check number
	for (int i = 0; i < 4; i++) { 
		if (!isdigit(token[i])) return false; 
	}
	return true;
}

// generate XAXB hint
string generate_hint (string token, char answer[]) {
	vector<bool> v(4, true);
	int A = 0, B = 0;
	// Count A
	for (int i = 0; i < 4; i++) {
		if (token[i] == answer[i]) {
			A += 1;
			v[i] = false;
		}
	}
	// Count B
	for (int i = 0; i < 4; i++) {
		if (v[i]) {
			for (int j = i + 1; j < i + 4; j++) {
				if (v[j] && token[i] == answer[j % 4]) {
					B += 1; break;
				}
			}
		}
	}
	string result = to_string(A) + "A" + to_string(B) + "B";
	return result;
}

int max (int x, int y) {
	if (x > y) return x;
	else return y;
}

// sort by room id
bool byID (vector<string> A, vector<string> B) {
	return stoi(A[0]) <= stoi(B[0]);
}

// sort by username
bool byUsername (vector<string> A, vector<string> B) {
	return A[0] <= B[0];
}

// delete room when manager leaves
void deleteRoom (int index, struct room_data * R, struct user_data * U) {

	// all room members leave room
	for (int u = 0; u < U->user_cnt; u++) {
		if (U->room[u] == R->id[index]) U->room[u] = -1;
	}	
	// delete room data
	for (int r = index; r < R->room_cnt; r++) {
		R->id[r] = R->id[r + 1];
		R->pub[r] = R->pub[r + 1];
		strcpy(R->manager[r], R->manager[r + 1]);
		R->started[r] = R->started[r + 1];
		R->num_of_members[r] = R->num_of_members[r + 1];
		R->invite_code[r] = R->invite_code[r + 1];
		for (int x = 0; x < 20; x++) R->members[r][x] = R->members[r + 1][x];
	}
	R->room_cnt -= 1;
}

// delete member from a room
void deleteMember (int index, int user_index, int fd, struct room_data * R, struct user_data * U) {
	int member_index;
	for (int r = 0; r < R->num_of_members[index]; r++)
		if (R->members[index][r] == fd) member_index = r;
	for (int r = member_index; r < R->num_of_members[index]; r++)
		R->members[index][r] = R->members[index][r + 1];
	R->num_of_members[index] -= 1;
	U->room[user_index] = -1;
}

// delete invitation
void deleteInvitation (int index, struct invite_data * I) {
	for (int x = index; x < I->invitations_cnt; x++) {
		strcpy(I->inviter_name[x], I->inviter_name[x + 1]);
		strcpy(I->inviter_email[x], I->inviter_email[x + 1]);
		strcpy(I->invitee_email[x], I->invitee_email[x + 1]);
		I->room_id[x] = I->room_id[x + 1];
		I->invitation_code[x] = I->invitation_code[x + 1];
	}
	I->invitations_cnt -= 1;
}

int main(int argc, char* argv[]) {

	// shared memory
	int shmid = shmget(IPC_PRIVATE, sizeof(struct user_data), IPC_CREAT | 0600);
    void *shm = shmat(shmid, NULL, 0);
    struct user_data* user_database = (struct user_data*)shm;
	user_database->user_cnt = 0;

	int shmid2 = shmget(IPC_PRIVATE, sizeof(struct room_data), IPC_CREAT | 0600);
    void *shm2 = shmat(shmid2, NULL, 0);
    struct room_data* room_database = (struct room_data*)shm2;
	room_database->room_cnt = 0;

	int shmid3 = shmget(IPC_PRIVATE, sizeof(struct invite_data), IPC_CREAT | 0600);
    void *shm3 = shmat(shmid3, NULL, 0);
    struct invite_data* invitations = (struct invite_data*)shm3;
	invitations->invitations_cnt = 0;

	int listenfd, connfd, udpfd, nready, maxfdp;
	char buffer[MAXLINE];
	fd_set rset;
	ssize_t n;
	socklen_t len;
	struct sockaddr_in cliaddr, servaddr;
	void sig_chld(int);

	// recording all client's fd
	int max_clients = 20;
	vector<int> clients(max_clients, 0);

	// create listening TCP socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	// set server to SO_REUSEADDR
	const int enable = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		cout << "setsockopt(SO_REUSEADDR) failed";
		exit(0);
	}
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);
	bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	listen(listenfd, 20);

	// create UDP socket
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	bind(udpfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	while (1) {
		// clear the descriptor set
		FD_ZERO(&rset);
		// add listenfd and udpfd to ready set
		FD_SET(listenfd, &rset);
		FD_SET(udpfd, &rset);

		// get maxfd
		maxfdp = max(listenfd, udpfd);
		// add connect fd to ready set	
		for (int i = 0; i < max_clients; i++) {
			if (clients[i] > 0) FD_SET(clients[i], &rset);
			if (clients[i] > maxfdp) maxfdp = clients[i];
		}

		// select the ready descriptor
		nready = select(maxfdp + 1, &rset, NULL, NULL, NULL);
		len = sizeof(cliaddr);

		// receive message via UDP
		if (FD_ISSET(udpfd, &rset)) {
			bzero(buffer, sizeof(buffer));
			recvfrom(udpfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, &len);

			// split the received message
			string token;
			vector<string> tokens;
			stringstream ss(buffer);
			while (getline(ss, token, ' ')) {
				if (token != "\n") {
					if (token[token.length() - 1] == '\n') token.pop_back();
					tokens.push_back(token);
				}
			}
			int num = tokens.size();

			// register
			if (tokens[0] == "register") {
				// register <username> <email> <password>
				// if (tokens[3][tokens[3].length() - 1] == '\n') tokens[3].pop_back();
				bool unique = true;
				// check if username and email is unique
				for (int i = 0; i < user_database->user_cnt; i++) {
					if ((strcmp(user_database->username[i], tokens[1].c_str()) == 0) || (strcmp(user_database->email[i], tokens[2].c_str()) == 0)) {
						char msg[MAXLINE] = "Username or Email is already used\n";
						sendto(udpfd, msg, strlen(msg), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
						unique = false;
						break;
					}
				}
				// add data to user_database
				if (unique) {
					strcpy(user_database->username[user_database->user_cnt], tokens[1].c_str());
					strcpy(user_database->email[user_database->user_cnt], tokens[2].c_str());
					strcpy(user_database->password[user_database->user_cnt], tokens[3].c_str());
					user_database->room[user_database->user_cnt] = -1;
					user_database->login_fd[user_database->user_cnt] = 0;
					user_database->user_cnt += 1;
					char msg[MAXLINE] = "Register Successfully\n";
					sendto(udpfd, msg, strlen(msg), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
				}
			}

			// list rooms or users
			if (tokens[0] == "list") {
				// list rooms
				if (tokens[1] == "rooms") {
					string msg = "List Game Rooms\n";

					// no rooms
					if (room_database->room_cnt == 0) {
						msg += "No Rooms\n";
						char final[MAXLINE];
						strcpy(final, msg.c_str());
						sendto(udpfd, final, strlen(final), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
						continue;
					}

					vector<vector<string> > sorted_rooms(room_database->room_cnt);
					for (int i = 0; i < room_database->room_cnt; i++) {
						sorted_rooms[i].resize(3);
						sorted_rooms[i][0] = to_string(room_database->id[i]);
						if (room_database->pub[i]) sorted_rooms[i][1] = "(Public)";
						else sorted_rooms[i][1] = "(Private)";
						if (room_database->started[i]) sorted_rooms[i][2] = " has started playing\n";
						else sorted_rooms[i][2] = " is open for players\n";
					}
					sort(sorted_rooms.begin(), sorted_rooms.end(), byID);

					for (int i = 0; i < sorted_rooms.size(); i++) {
						string tmp = to_string(i + 1) + ". " + sorted_rooms[i][1] + " Game Room " + sorted_rooms[i][0] + sorted_rooms[i][2];
						msg += tmp;
					}
					
					char final[MAXLINE];
					strcpy(final, msg.c_str());
					sendto(udpfd, final, strlen(final), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
				}

				// list users
				else {
					string msg = "List Users\n";

					// no users
					if (user_database->user_cnt == 0) {
						msg += "No Users\n";
						char final[MAXLINE];
						strcpy(final, msg.c_str());
						sendto(udpfd, final, strlen(final), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
						continue;								
					}

					vector<vector<string> > sorted_users(user_database->user_cnt);
					for (int i = 0; i < user_database->user_cnt; i++) {
						sorted_users[i].resize(3);
						sorted_users[i][0] = string(user_database->username[i]);
						sorted_users[i][1] = string(user_database->email[i]);
						if (user_database->login_fd[i] != 0) sorted_users[i][2] = " Online\n";
						else sorted_users[i][2] = " Offline\n";
					}
					sort(sorted_users.begin(), sorted_users.end(), byUsername);

					for (int i = 0; i < sorted_users.size(); i++) {
						string tmp = to_string(i + 1) + ". " + sorted_users[i][0] + "<" + sorted_users[i][1] + ">" + sorted_users[i][2];
						msg += tmp;
					}
					
					char final[MAXLINE];
					strcpy(final, msg.c_str());
					sendto(udpfd, final, strlen(final), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
				}
			}
		}

		// new TCP connection
		if (FD_ISSET(listenfd, &rset)) {
			connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);
			for (int i = 0; i < max_clients; i++) {
				if (clients[i] == 0) {
					clients[i] = connfd;
					break;
				}
			}
		}

		// receive message from current TCP connections
		for (int k = 0; k < max_clients; k++) {
			if (FD_ISSET(clients[k], &rset)) {
				bzero(buffer, sizeof(buffer));
				if ((read(clients[k], buffer, sizeof(buffer))) == 0) {											
					int user_index = check_login(clients[k], user_database);
					int room_id = -1;
					if (user_index != -1) {
						user_database->login_fd[user_index] = 0;
						if (user_database->room[user_index] != -1) {
							room_id = user_database->room[user_index];
						}
					}
					if (room_id != -1) {
						for (int i = 0; i < room_database->room_cnt; i++) {
							if (room_database->id[i] == room_id) {
								// room manager
								if (strcmp(room_database->manager[i], user_database->username[user_index]) == 0) {
									deleteRoom(i, room_database, user_database);
									for (int j = 0; j < invitations->invitations_cnt; j++) {
										if (strcmp(invitations->inviter_name[j], user_database->username[user_index]) == 0) {
											deleteInvitation(j, invitations);
										}
									}
								}
								// non manager
								else deleteMember(i, user_index, clients[k], room_database, user_database);
								break;
							}
						}
					}
					close(clients[k]);
					clients[k] = 0;
				}
				else {
					int clifd = clients[k];
					// split the received message
					string token;
					vector<string> tokens;
					stringstream ss(buffer);
					while (getline(ss, token, ' ')) {
						if (token != "\n") {
							if (token[token.length() - 1] == '\n') token.pop_back();
							tokens.push_back(token);
						}
					}					
					int num = tokens.size();

					// login
					if (tokens[0] == "login") {

						// check user exists and password
						bool exist = false;
						bool password_correct = false;
						int user_index = -1; // record the user's index if exist
						for (int i = 0; i < user_database->user_cnt; i++) {
							if (strcmp(user_database->username[i], tokens[1].c_str()) == 0) {
								exist = true;
								if (strcmp(user_database->password[i], tokens[2].c_str()) == 0) password_correct = true;
								user_index = i;
								break;
							}
						}

						// Fail(1)  Username not found
						if (!exist) {
							char msg[MAXLINE] = "Username does not exist\n";
							write(clifd, msg, strlen(msg));
							continue;                                
						}

						// Fail(2)  Already logined as another account
						int existing_user_index = check_login(clifd, user_database);
						if (existing_user_index != -1) {
							string tmp = "You already logged in as " + string(user_database->username[existing_user_index]) + "\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
							continue;
						}

						// Fail(4) Password is incorrect
						if (!password_correct) {
							char msg[MAXLINE] = "Wrong password\n";
							write(clifd, msg, strlen(msg));
							continue;
						}

						// Success
						if (user_database->login_fd[user_index] == 0) { // not occupied
							user_database->login_fd[user_index] = clifd;
							string tmp = "Welcome, " + string(user_database->username[user_index]) + "\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
						} 
						// Fail(3) Account is already logged in
						else {
							string tmp = "Someone already logged in as " + string(user_database->username[user_index]) + "\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
						}		
					}

					// logout
					else if (tokens[0] == "logout") {

						// Fail(1) User not logged in
						int user_index = check_login(clifd, user_database);
						if (user_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));	
							continue;						
						}

						// Fail(2) User is in game room
						if (user_database->room[user_index] != -1) {
							string tmp = "You are already in game room " + to_string(user_database->room[user_index]) + ", please leave game room\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
							continue;													
						}

						// Success
						user_database->login_fd[user_index] = 0;
						string tmp = "Goodbye, " + string(user_database->username[user_index]) + "\n";
						char msg[MAXLINE];
						strcpy(msg, tmp.c_str());
						write(clifd, msg, strlen(msg));
					}

					// create room
					else if (tokens[0] == "create") {
						int user_index = check_login(clifd, user_database);
						bool existing_id = false;
						for (int i = 0; i < room_database->room_cnt; i++) {
							if (to_string(room_database->id[i]) == tokens[3]) {
								existing_id = true;
								break;
							}
						}

						// Fail(1) User not logged in
						if (user_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));
							continue;                       
						}

						// Fail(2) User is in game room already
						if (user_database->room[user_index] != -1) {
							string tmp = "You are already in game room " + to_string(user_database->room[user_index]) + ", please leave game room\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
							continue;
						}

						// Fail(3) Existing room ID
						if (existing_id) {
							char msg[MAXLINE] = "Game room ID is used, choose another one\n";
							write(clifd, msg, strlen(msg));
							continue;     
						}

						string tmp;
						// public room
						if (tokens[1] == "public") {
							tmp = "You create public game room " + tokens[3] + "\n";
							room_database->pub[room_database->room_cnt] = true;
							room_database->invite_code[room_database->room_cnt] = -1;
						}
						// private room
						else {
							tmp = "You create private game room " + tokens[3] + "\n";
							room_database->pub[room_database->room_cnt] = false;
							room_database->invite_code[room_database->room_cnt] = stoi(tokens[4]);
						}
						strcpy(room_database->manager[room_database->room_cnt], user_database->username[user_index]);
						room_database->id[room_database->room_cnt] = stoi(tokens[3]);
						room_database->started[room_database->room_cnt] = false;
						room_database->members[room_database->room_cnt][0] = user_database->login_fd[user_index];
						room_database->num_of_members[room_database->room_cnt] += 1;	
						user_database->room[user_index] = stoi(tokens[3]);
						room_database->room_cnt += 1;	
						char msg[MAXLINE];
						strcpy(msg, tmp.c_str());
						write(clifd, msg, strlen(msg));
					}

					// join room
					else if (tokens[0] == "join") {
						int user_index = check_login(clifd, user_database), room_index = -1;
						for (int i = 0; i < room_database->room_cnt; i++) {
							if (to_string(room_database->id[i]) == tokens[2]) {
								room_index = i;
								break;
							}
						}

						// Fail(1) User not logged in
						if (user_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));
							continue;							
						}

						// Fail(2) Already in game room
						if (user_database->room[user_index] != -1) {
							string tmp = "You are already in game room " + to_string(user_database->room[user_index]) + ", please leave game room\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
							continue;
						}

						// Fail(3) Room ID does not exist
						if (room_index == -1) {
							string tmp = "Game room " + tokens[2] + " is not exist\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
							continue;							
						}

						// Fail(4) Private game room
						if (!room_database->pub[room_index]) {
							char msg[MAXLINE] = "Game room is private, please join game by invitation code\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(5) Game is started
						if (room_database->started[room_index]) {
							char msg[MAXLINE] = "Game has started, you can't join now\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}
						
						// Success
						string tmp = "You join game room " + tokens[2] + "\n";
						char msg[MAXLINE];
						strcpy(msg, tmp.c_str());
						write(clifd, msg, strlen(msg));
						// broadcast message
						tmp = "Welcome, " + string(user_database->username[user_index]) + " to game!\n";
						char broadcast[MAXLINE];
						strcpy(broadcast, tmp.c_str());
						for (int i = 0; i < room_database->num_of_members[room_index]; i++) {
							write(room_database->members[room_index][i], broadcast, strlen(broadcast));
						}
						user_database->room[user_index] = stoi(tokens[2]);
						// add member to room data
						room_database->members[room_index][room_database->num_of_members[room_index]] = clifd;
						room_database->num_of_members[room_index] += 1;
					}

					// invite
					else if (tokens[0] == "invite") {

						// Fail(1) Inviter not logged in
						int inviter_index = check_login(clifd, user_database);
						if (inviter_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(2) Inviter not in any room
						if (user_database->room[inviter_index] == -1) {
							char msg[MAXLINE] = "You did not join any game room\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(3) Inviter is not private room manager
						bool valid = true;
						int room_index = -1;
						for (int i = 0; i < room_database->room_cnt; i++) {
							if (room_database->id[i] == user_database->room[inviter_index]) {
								room_index = i;
								if (room_database->pub[i] || (strcmp(room_database->manager[i], user_database->username[inviter_index]) != 0)) 
									valid = false;
								break;							
							}
						}
						if (!valid) {
							char msg[MAXLINE] = "You are not private game room manager\n";
							write(clifd, msg, strlen(msg));	
							continue;
						}

						// Fail(4) Invitee not logged in
						bool not_logined = false;
						int invitee_index = -1;
						for (int i = 0; i < user_database->user_cnt; i++) {
							if (string(user_database->email[i]) == tokens[1]) {
								invitee_index = i;
								if (user_database->login_fd[i] == 0) not_logined = true;
								break;
							}
						}
						if (not_logined) {
							char msg[MAXLINE] = "Invitee not logged in\n";
							write(clifd, msg, strlen(msg));	
							continue;							
						}

						// ignore duplicated invitations
						bool unique = true;
						for (int i = 0; i < invitations->invitations_cnt; i++) {
							if (strcmp(invitations->inviter_name[i], user_database->username[inviter_index]) == 0) {
								if (strcmp(invitations->invitee_email[i], user_database->email[invitee_index]) == 0) {
									unique = false; break;
								}
							}
						}

						// Success
						// add invitation data
						if (unique) {
							strcpy(invitations->invitee_email[invitations->invitations_cnt], user_database->email[invitee_index]);
							strcpy(invitations->inviter_email[invitations->invitations_cnt], user_database->email[inviter_index]);
							strcpy(invitations->inviter_name[invitations->invitations_cnt], user_database->username[inviter_index]);
							invitations->room_id[invitations->invitations_cnt] = room_database->id[room_index];
							invitations->invitation_code[invitations->invitations_cnt] = room_database->invite_code[room_index];
							invitations->invitations_cnt += 1;
						}
						// send message to inviter
						string tmp = "You send invitation to " + string(user_database->username[invitee_index])\
										 + "<" + tokens[1] + ">\n";
						char msg[MAXLINE];
						strcpy(msg, tmp.c_str());
						write(clifd, msg, strlen(msg));
						// broadcast message to invitee
						tmp = "You receive invitation from " + string(user_database->username[inviter_index])\
								 + "<" + user_database->email[inviter_index] + ">\n";
						char broadcast[MAXLINE];
						strcpy(broadcast, tmp.c_str());
						write(user_database->login_fd[invitee_index], broadcast, strlen(broadcast));											
					}

					// list invitations
					else if (tokens[0] == "list") {
						// Fail(1) Not logged in
						int user_index = check_login(clifd, user_database);
						if (user_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));
							continue;															
						}

						string tmp = "List invitations\n";
						// no invitations
						if (invitations->invitations_cnt == 0) {
							tmp += "No Invitations\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));	
							continue;						
						}
						// at least one invitations
						else {
							vector<vector<string> > sorted_invitations;
							vector<string> info(4);
							for (int i = 0; i < invitations->invitations_cnt; i++) {
								if (strcmp(invitations->invitee_email[i], user_database->email[user_index]) == 0) {
									info[0] = to_string(invitations->room_id[i]);
									info[1] = string(invitations->inviter_name[i]);
									info[2] = string(invitations->inviter_email[i]);
									info[3] = to_string(invitations->invitation_code[i]);
									sorted_invitations.push_back(info);
									// cout << info[0] << " " << info[1] << " " << info[2] << " " << info[3] << "\n";
								}
							}
							sort(sorted_invitations.begin(), sorted_invitations.end(), byID);
							
							for (int i = 0; i < sorted_invitations.size(); i++) {
								tmp += to_string(i + 1) + ". " + sorted_invitations[i][1] + "<" + sorted_invitations[i][2]\
										 + "> invite you to join game room " + sorted_invitations[i][0] + ", invitation code is "\
										 + sorted_invitations[i][3] + "\n";
							}
							// cout << tmp;
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
						}
					}

					// accept invitations
					else if (tokens[0] == "accept") {
						// Fail(1) Not logged in
						int user_index = check_login(clifd, user_database);
						if (user_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(2) Already in game room
						if (user_database->room[user_index] != -1) {
							string tmp = "You are already in game room " + to_string(user_database->room[user_index])\
											 + ", please leave game room\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
							continue;
						}

						// Fail(3) Invitation not exist
						int invite_index = -1;
						for (int i = 0; i < invitations->invitations_cnt; i++) {
							if (strcmp(invitations->invitee_email[i], user_database->email[user_index]) == 0) {
								if (strcmp(invitations->inviter_email[i], tokens[1].c_str()) == 0) {
									invite_index = i;
									break;
								}
							}
						}	
						if (invite_index == -1) {
							char msg[MAXLINE] = "Invitation not exist\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(4) Incorrect code
						if (to_string(invitations->invitation_code[invite_index]) != tokens[2]) {
							char msg[MAXLINE] = "Your invitation code is incorrect\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(5) Game started
						int room_index = -1;
						for (int i = 0; i < room_database->room_cnt; i++) {
							if (room_database->id[i] == invitations->room_id[invite_index]) {
								room_index = i;
								break;
							}
						}						
						if (room_database->started[room_index]) {
							char msg[MAXLINE] = "Game has started, you can't join now\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Success
						user_database->room[user_index] = invitations->room_id[invite_index];						
						string tmp = "You join game room " + to_string(invitations->room_id[invite_index]) + "\n";
						char msg[MAXLINE];
						strcpy(msg, tmp.c_str());
						write(clifd, msg, strlen(msg));
						// broadcast message
						tmp = "Welcome, " + string(user_database->username[user_index]) + " to game!\n";
						char broadcast[MAXLINE];
						strcpy(broadcast, tmp.c_str());
						for (int i = 0; i < room_database->num_of_members[room_index]; i++) {
							if (room_database->members[room_index][i] != clifd)
								write(room_database->members[room_index][i], broadcast, strlen(broadcast));
						}
						// add member to room data
						room_database->members[room_index][room_database->num_of_members[room_index]] = clifd;
						room_database->num_of_members[room_index] += 1;
					}

					// start game
					else if (tokens[0] == "start") {
						// Fail(1) Not logged in
						int user_index = check_login(clifd, user_database);
						if (user_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(2) Not in any game room
						if (user_database->room[user_index] == -1) {
							char msg[MAXLINE] = "You did not join any game room\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(3) Not room manager						
						bool manager = false;
						int room_index = -1;
						for (int i = 0; i < room_database->room_cnt; i++) {
							if (room_database->id[i] == user_database->room[user_index]) {
								room_index = i;
								if (strcmp(room_database->manager[i], user_database->username[user_index]) == 0) 
									manager = true;
								break;							
							}
						}
						if (!manager) {
							char msg[MAXLINE] = "You are not game room manager, you can't start game\n";
							write(clifd, msg, strlen(msg));	
							continue;
						}

						// Fail(4) Game has started
						if (room_database->started[room_index]) {
							char msg[MAXLINE] = "Game has started, you can't start again\n";
							write(clifd, msg, strlen(msg));	
							continue;							
						}

						// Fail(5) Check 4 digit
						if (num == 4 && !bit_check(tokens[3])) {
							char msg[MAXLINE] = "Please enter 4 digit number with leading zero\n";
							write(clifd, msg, strlen(msg));	
							continue;									
						}
						// Success
						room_database->rounds[room_index] = stoi(tokens[2]);
						room_database->turn[room_index] = 0;
						room_database->started[room_index] = true;										
						// guess number given
						if (num == 4) strcpy(room_database->answer[room_index], tokens[3].c_str());
						else { // not given, generate random number
							srand(time(NULL));
							string answer = "";
							for (int i = 0; i < 4; i++) {
								int random_number = rand() % 10;
								answer += to_string(random_number);
							}
							strcpy(room_database->answer[room_index], answer.c_str());
						}
						// broadcast message
						string tmp = "Game start! Current player is " + string(user_database->username[user_index]) + "\n";
						char msg[MAXLINE];
						strcpy(msg, tmp.c_str());													
						for (int i = 0; i < room_database->num_of_members[room_index]; i++) {
							write(room_database->members[room_index][i], msg, strlen(msg));	
						}
					}

					// guess
					else if (tokens[0] == "guess") {
						// Fail(1) Not logged in
						int user_index = check_login(clifd, user_database);
						if (user_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(2) Not in any game room
						if (user_database->room[user_index] == -1) {
							char msg[MAXLINE] = "You did not join any game room\n";
							write(clifd, msg, strlen(msg));
							continue;								
						}

						// Fail(3) Game not started						
						bool manager = false;
						int room_index = -1;
						for (int i = 0; i < room_database->room_cnt; i++) {
							if (room_database->id[i] == user_database->room[user_index]) {
								room_index = i;
								if (strcmp(room_database->manager[i], user_database->username[user_index]) == 0) 
									manager = true;
								break;							
							}
						}
						if (!room_database->started[room_index]) {
							if (manager) {
								char msg[MAXLINE] = "You are game room manager, please start game first\n";
								write(clifd, msg, strlen(msg));	
								continue;
							}
							else {
								char msg[MAXLINE] = "Game has not started yet\n";
								write(clifd, msg, strlen(msg));	
								continue;
							}
						}

						// Fail(4) Not your turn to guess
						if (room_database->members[room_index][room_database->turn[room_index]] != clifd) {
							int turn_index = -1;
							for (int i = 0; i < user_database->user_cnt; i++) {
								if (room_database->members[room_index][room_database->turn[room_index]] == user_database->login_fd[i]) {
									turn_index = i; break;
								}
							}
							string tmp = "Please wait..., current player is " + string(user_database->username[turn_index]) + "\n";
							char msg[MAXLINE];
							strcpy(msg, tmp.c_str());
							write(clifd, msg, strlen(msg));
							continue;							
						}

						// Fail(5) Check 4 digit
						if (!bit_check(tokens[1])) {
							char msg[MAXLINE] = "Please enter 4 digit number with leading zero\n";
							write(clifd, msg, strlen(msg));	
							continue;									
						}

						// Success
						string hint = generate_hint(tokens[1], room_database->answer[room_index]);
						string guesser(user_database->username[user_index]);
						string message;
						// check if no more chances
						bool end = false;
						room_database->turn[room_index] += 1;
						if (room_database->turn[room_index] == room_database->num_of_members[room_index]) {
							room_database->turn[room_index] = 0;
							room_database->rounds[room_index] -= 1;
							if (room_database->rounds[room_index] == 0) end = true;
						}
						// Bingo
						if (hint == "4A0B") {
							message = guesser + " guess '" + tokens[1] +"' and got Bingo!!! "\
										 + guesser + " wins the game, game ends\n";
							room_database->started[room_index] = false;
						}
						// Not Bingo
						else {
							message = guesser + " guess '" + tokens[1] +"' and got '" + hint + "'\n";
							if (end) {
								message += "Game ends, no one wins\n";
								room_database->started[room_index] = false;
							}
						}

						// broadcast message
						char msg[MAXLINE];
						strcpy(msg, message.c_str());
						for (int i = 0; i < room_database->num_of_members[room_index]; i++)
							write(room_database->members[room_index][i], msg, strlen(msg));
					}

					// leave room
					else if (tokens[0] == "leave") {
						// Fail(1) You are not logged in
						int user_index = check_login(clifd, user_database);
						if (user_index == -1) {
							char msg[MAXLINE] = "You are not logged in\n";
							write(clifd, msg, strlen(msg));
							continue;                       
						}

						// Fail(2) You are not in game room
						if (user_database->room[user_index] == -1) {
							char msg[MAXLINE] = "You did not join any game room\n";
							write(clifd, msg, strlen(msg));
							continue;													
						}						

						// Success
						for (int i = 0; i < room_database->room_cnt; i++) {
							if (room_database->id[i] == user_database->room[user_index]) {
								// room manager
								if (strcmp(room_database->manager[i], user_database->username[user_index]) == 0) {
									string tmp = "You leave game room " + to_string(user_database->room[user_index]) + "\n";
									char msg[MAXLINE];
									strcpy(msg, tmp.c_str());
									write(clifd, msg, strlen(msg));
									// broadcast message
									tmp = "Game room manager leave game room " + to_string(user_database->room[user_index])\
											 + ", you are forced to leave too\n";
									char broadcast[MAXLINE];
									strcpy(broadcast, tmp.c_str());									
									for (int j = 0; j < room_database->num_of_members[i]; j++) {
										if (room_database->members[i][j] != clifd)
											write(room_database->members[i][j], broadcast, strlen(broadcast));
									}
									deleteRoom(i, room_database, user_database);
									for (int j = 0; j < invitations->invitations_cnt; j++) {
										if (strcmp(invitations->inviter_name[j], user_database->username[user_index]) == 0) {
											deleteInvitation(j, invitations);
										}
									}
								}
								// non-manager
								else {
									// game started
									if (room_database->started[i]) {
										string tmp = "You leave game room " + to_string(user_database->room[user_index]) + ", game ends\n";
										char msg[MAXLINE];
										strcpy(msg, tmp.c_str());
										write(clifd, msg, strlen(msg));
										// broadcast message
										tmp = string(user_database->username[user_index]) + " leave game room "\
												 + to_string(user_database->room[user_index]) + ", game ends\n";
										char broadcast[MAXLINE];
										strcpy(broadcast, tmp.c_str());										
										for (int j = 0; j < room_database->num_of_members[i]; j++) {
											if (room_database->members[i][j] != clifd)
												write(room_database->members[i][j], broadcast, strlen(broadcast));
										}
										room_database->started[i] = false;	
										deleteMember(i, user_index, clifd, room_database, user_database);									
									}
									// not started
									else {
										// cout << "here\n";
										string tmp = "You leave game room " + to_string(user_database->room[user_index]) + "\n";
										char msg[MAXLINE];
										strcpy(msg, tmp.c_str());
										write(clifd, msg, strlen(msg));
										// broadcast message
										tmp = string(user_database->username[user_index]) + " leave game room " + to_string(user_database->room[user_index]) + "\n";
										char broadcast[MAXLINE];
										strcpy(broadcast, tmp.c_str());										
										for (int j = 0; j < room_database->num_of_members[i]; j++) {
											if (room_database->members[i][j] != clifd)
												write(room_database->members[i][j], broadcast, strlen(broadcast));
										}
										deleteMember(i, user_index, clifd, room_database, user_database);											
									}
								}
								break;
							}
						}
					}

					// exit
					else if (tokens[0] == "exit" || num == 0) {
						int user_index = check_login(clifd, user_database);
						int room_id = -1;
						if (user_index != -1) {
							user_database->login_fd[user_index] = 0;
							if (user_database->room[user_index] != -1) {
								room_id = user_database->room[user_index];
							}
						}
						if (room_id != -1) {
							for (int i = 0; i < room_database->room_cnt; i++) {
								if (room_database->id[i] == room_id) {
									// room manager
									if (strcmp(room_database->manager[i], user_database->username[user_index]) == 0) {
										deleteRoom(i, room_database, user_database);
										for (int j = 0; j < invitations->invitations_cnt; j++) {
										if (strcmp(invitations->inviter_name[j], user_database->username[user_index]) == 0) {
											deleteInvitation(j, invitations);
										}
									}
									}
									// non manager
									else deleteMember(i, user_index, clifd, room_database, user_database);
									break;
								}
							}
						}
						break;
					}
				}
			}
		}
	}
    return 0;
}

