#include "list.h"

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

int my_port;
char *remoteaddr;
int remote_port;

pthread_t keyboard, sender, receiver, printer;

sem_t mutex_send, mutex_receive;

int sock_cli, sock_ser;
struct sockaddr_in servaddr, cliaddr, servaddr2;

const int encryption_key = 8;
const int decryption_key = 256 - encryption_key;
int running = 1;

void create_sockets() {

	printf("Creating sockets...\n\n");

	// Server socket
	if ((sock_ser = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Socket creation failed.\n");
		exit(0);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(my_port);

	// Bind server socket
	if (bind(sock_ser, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		printf("Socket binding failed.");
		exit(0);
	}

	// Client socket
	if ((sock_cli = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Socket creation failed.\n");
		exit(0);
	}

	memset(&servaddr2, 0, sizeof(servaddr2));

	servaddr2.sin_family = AF_INET;
	servaddr2.sin_addr.s_addr = inet_addr(remoteaddr);
	servaddr2.sin_port = htons(remote_port);

}

void quit() {
	printf("Cancelling threads...\n");
	pthread_cancel(keyboard);
	pthread_cancel(receiver);
	pthread_cancel(sender);
	pthread_cancel(printer);
	running = 0;
}

void *store_msg(void *send_list) {

	char line[4000];
	int c;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 200000;

	// Set 100ms timeout
	if (setsockopt(sock_cli, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		perror("Error\n");
	}

	printf("Welcome to Lets-Talk! Please type your messages now.\n");

	while (running) {

		// LOCK input
		sem_wait(&mutex_send);

		// Get user input
		if (fgets(line, sizeof(line), stdin) != NULL) {

			if (strcmp(line, "!status\n") == 0) {
				// If user types !status
				int success;
				int n, len;
				char buffer[4000];

				// Send packet to server
				sendto(sock_cli, (const char *)line, strlen(line), MSG_CONFIRM, (struct sockaddr *) &servaddr2, sizeof(servaddr2));

				// If server responds, print ONLINE, else after 100ms with no response, print OFFLINE
				n = recvfrom(sock_cli, (char *)buffer, 4000, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);

				if (n == -1) {
					printf("User is: OFFLINE\n");
				} else {
					printf("%s\n", buffer);
				}
				
			} else if (strcmp(line, "!exit\n") == 0) {
				sendto(sock_cli, (const char *)line, strlen(line), MSG_CONFIRM, (struct sockaddr *) &servaddr2, sizeof(servaddr2));
				quit();
			} else {

				// Encrypt message by key
				for (int i = 0; i < strlen(line) - 1; i++) {

					line[i] = (line[i] + encryption_key) % 256;

				}

				// Appends message to send_list
				List_append(send_list, line);

			}

			// UNLOCK input
			sem_post(&mutex_send);
		}

	}

}

void *print_msg(void *receive_list) {

	while (running) {

		int list_len = List_count(receive_list);

		if (list_len != 0) {
			// Prints message in receive_list and removes it from list
			char *msg = List_remove(receive_list);

			// Decrypt message by key
			for (int i = 0; i < strlen(msg) - 1; i++) {

				msg[i] = (msg[i] + decryption_key) % 256;

			}

			printf("%s: %s", remoteaddr, msg);
			fflush(stdout);

		}

	}

}

void *client(void *send_list) {

	while (running) {

		char *msg = List_curr(send_list);

		if (msg == NULL) {
			// While there is no message that needs to be sent
		} else {
			int len, n;

			// Send message to server
			sendto(sock_cli, (const char *)msg, strlen(msg), MSG_CONFIRM, (struct sockaddr *) &servaddr2, sizeof(servaddr2));

			// Remove message from send_list after sending
			char *temp = List_remove(send_list);
		}

	}

}

void *server(void *receive_list) {

	char buffer[4000];
	char *msg = "User is: ONLINE";

	struct timeval tv;
	tv.tv_sec = 1;

	// Set 1s timeout
	if (setsockopt(sock_ser, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		perror("Error\n");
	}

	while (running) {

		// LOCK server
		sem_wait(&mutex_receive);

		int len, n;
		len = sizeof(cliaddr);

		// Wait until receives from client
		n = recvfrom(sock_ser, (char *) buffer, 4000, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		buffer[n] = '\0';

		if (strcmp(buffer, "!status\n") == 0) {
			// Respond to client if they type !status
			sendto(sock_ser, (const char *)msg, strlen(msg), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
		} else if (strcmp(buffer, "!exit\n") == 0) { 
			quit();
		} else if (n != -1) {
			// Appends message received from client into receive_list
			List_append(receive_list, buffer);
		}

		// UNLOCK server
		sem_post(&mutex_receive);

	}

}

int main(int argc, char **argv) {

	if (argc == 4) {

		// If user types localhost as the remote address, set it to 127.0.0.1
		my_port = atoi(argv[1]);
		remote_port = atoi(argv[3]);
		if (strcmp(argv[2], "localhost") == 0) {
			remoteaddr = "127.0.0.1";
		} else {
			remoteaddr = argv[2];
		}

		// Initialize variables
		int send, receive, input, output;
		int arg1 = 1, arg2 = 2, arg3 = 3, arg4 = 4;

		// Create lists
		List *send_list = List_create();
		List *receive_list = List_create();

		// Initialize semaphores
		sem_init(&mutex_send, 0, 1);
		sem_init(&mutex_receive, 0, 1);

		// Print input parameters information
		printf("Server Port: %d\n", my_port); // Server receives
		printf("Client Port: %d\n", remote_port); // Client sends
		printf("Client Address: %s\n\n", remoteaddr);

		// Create sockets for server and client
		create_sockets();

		// Initialize 4 threads for input, receive, send, and output
		input = pthread_create(&keyboard, NULL, store_msg, (void *) send_list);
		receive = pthread_create(&receiver, NULL, server, (void *) receive_list);
		send = pthread_create(&sender, NULL, client, (void *) send_list);
		output = pthread_create(&printer, NULL, print_msg, (void *) receive_list);
		pthread_join(printer, NULL);
		pthread_join(sender, NULL);
		pthread_join(receiver, NULL);
		pthread_join(keyboard, NULL);

		// Cancel threads and close sockets
		printf("Closing sockets...\n");
		close(sock_cli);
		close(sock_ser);

		printf("Exiting lets-talk...\n");

	} else {
		printf("Not enough arguments.\n");
	}

	return 0;

}