#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/termios.h>
#include <sys/select.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdbool.h>

// literaturepage.com:80/read/adventuresofsherlockholmes-1.html

struct termios original_termios;

void disable() {
	if (tcsetattr(0, TCSAFLUSH, &original_termios) == -1) {
		printf("Error: tcsetattr\n");
		exit(-1);
	}
}

void enable() {
	if (tcgetattr(0, &original_termios) == -1) {
		printf("Error: tcgetattr\n");
		exit (-1);
	}
	atexit(disable);
	struct termios new_termios = original_termios;
	new_termios.c_lflag &= ~(ECHO | ICANON);
	if (tcsetattr(0, TCSAFLUSH, &new_termios) == -1) {
		printf("Error: tcsetattr\n");
		exit(-1);
	}
}

int main() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		printf("Error: socket didn't open\n");
		return -1;
	}

	char fullNameHost[100];
	char fullNameHostCopy[100]; 

	if (scanf("%s", fullNameHost) == 0) {
		printf("Error: scanf Error\n");
		return -1;
	}

	strcpy(fullNameHostCopy, fullNameHost);

	char* token = strtok(fullNameHost, "/");
	char* hostname = strtok(token, ":");
	char hostnameCopy[100];
	strcpy(hostnameCopy, hostname);

	printf("\nFull name: %s\n", fullNameHost);

	struct hostent* server = gethostbyname(hostname);
	if (server == NULL) {
		printf("Error: invalid hostname\n");
		return -1;
	}

	hostname = strtok(NULL, ":"); //port
	int port = 80;
	printf("Port: %s\n", hostname);
	if (hostname != NULL) {
		port = atoi(hostname);
	}

	struct sockaddr_in address;			//socket address

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	for (int i = 0; server->h_addr_list[i] != NULL; i++) {
		printf("Server address number: %d\n", i);
		memcpy(&address.sin_addr.s_addr, server->h_addr_list[i], server->h_length);
		if (connect(fd, (struct sockaddr*)&address, sizeof(address)) == 0) {
			printf("Connection done\n\n");
			break;
		}
	}


	char message[1000];
	strcat(message, "GET http://");
	strcat(message, hostnameCopy);
	char* token2 = strtok(fullNameHostCopy, "/");
	token2 = strtok(NULL, "/");

	while (token2 != NULL) {
		strcat(message, "/");
		strcat(message, token2);
		token2 = strtok(NULL, "/");
	}
	strcat(message, " HTTP/1.0\n");
	strcat(message, "Host: ");
	strcat(message, hostnameCopy);
	strcat(message, "\r\n\r\n\0");

	int messageLenght = strlen(message);
	printf("%s\n\n", message);

	if (write(fd, message, messageLenght) != messageLenght) {
		printf("ERROR: write error\n");
		return -1;
	}

	fd_set fd_in, fd_out;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 10;
	int largest_sock = 0 > fd ? 0 : fd;
	
	int printedLines = 0;
	bool flag = false;
	char readBuffer[100];
	int readNow;
	
	enable();

	while (1) {
		FD_SET(0, &fd_in);
		FD_SET(fd, &fd_out);
		
		int ret = select(largest_sock + 1, &fd_in, &fd_out, NULL, &tv);
		if (ret == -1) {
			printf("Error: select\n");
			close(fd);
			return -1;
		}
		else if (ret == 0) {}
		else
		{
			if (FD_ISSET(0, &fd_in)) {
				if (flag) {
					while (1) {
						if (getchar() == ' ') {
							printedLines = 0;
							flag = false;
							break;
						}
					}
				}
			}

			if (FD_ISSET(fd, &fd_out)) {
				if (printedLines < 25) {
					readNow = read(fd, readBuffer, sizeof(readBuffer));
					if (readNow > 0) {
						for (int i = 0; i < readNow; ++i) {
							if (readBuffer[i] == '\n') {
								printedLines++;
							}
							printf("%c", readBuffer[i]);
						}
					}
					else if (readNow == -1) {
						printf("Error: read\n");
						close(fd);
						return -1;
					}
					else if (readNow == 0) {
						printf("Closed connection\n");
						close(fd);
						return 0;
					}
				}
				else if (!flag) {
					flag = true;
					printf("\n\nPress space to scroll down...\n\n");
				}
			}
		}
	}

	close(fd);
	return 0;
}
