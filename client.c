#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void usage(int argc, char **argv) {
	printf("usage: %s <server IP> <server port>\n", argv[0]);
	printf("example: %s 127.0.0.1 51511\n", argv[0]);
	exit(EXIT_FAILURE);
}

#define BUFSZ 1024

int id = -1;

//parse the message received from the server
//return the message to be sent to the server
//server is a flag that indicates to which server the message should be sent. 0 for the SCII server and 1 for the SE server
char* parse_message(char* buf, int *server){
	char* message = "";
	if (strncmp(buf, "display info se", strlen("display info se")) == 0){
		message = "REQ_INFOSE";
		*server = 1;
	}
	else if (strncmp(buf, "display info scii", strlen("display info scii")) == 0){
		message = "REQ_INFOSCII";
		*server = 0;
	}
	else if (strncmp(buf, "query condition", strlen("query condition")) == 0){
		message = "REQ_STATUS";
		*server = 1;
	}
	return message;
}

void parse_response(char* buf, int server){
	if(strncmp(buf, "RES_INFOSE", strlen("RES_INFOSE")) == 0){

		printf("SE: %s\n", buf + strlen("RES_INFOSE"));
		strtok(buf, "(");
		int _id = atoi(strtok(NULL, ")"));
		id = _id;
		if (server == 0){
			printf("Servidor SE New ID: %d\n", id);
		}
		else {
			printf("Servidor SCII New ID: %d\n", id);
		}
	}
}

int check_availability(int sock){
	int result = -1;
	char buf[BUFSZ];
	recv(sock, buf, BUFSZ, 0);

	if (strncmp(buf, "ERROR 01", strlen("ERROR 01")) == 0){
		result = 0;
	}
	else {
		result = 1;
	}
	return result;
}

int main(int argc, char **argv) {

	struct sockaddr_storage SE_storage, SCII_storage;

	if(addrparse(argv[1], argv[2], &SE_storage) != 0){
		usage(argc, argv);
	}
	int SE_socket = socket(SE_storage.ss_family, SOCK_STREAM, 0);
	if(SE_socket == -1){
		logexit("socket");
	}
	struct sockaddr *SE_addr = (struct sockaddr *)(&SE_storage);
	if(connect(SE_socket, SE_addr, sizeof(SE_storage)) != 0){
		logexit("connect");
	}

	if(addrparse(argv[1], argv[3], &SCII_storage) != 0){
		usage(argc, argv);
	}

	int SCII_socket = socket(SCII_storage.ss_family, SOCK_STREAM, 0);
	if(SCII_socket == -1){
		logexit("socket");
	}

	struct sockaddr *SCII_addr = (struct sockaddr *)(&SCII_storage);
	if(connect(SCII_socket, SCII_addr, sizeof(SCII_storage)) != 0){
		logexit("connect");
	}
	
	char addrstr_SE[BUFSZ];
	addrtostr(SE_addr, addrstr_SE, BUFSZ);	

	char addrstr_SCII[BUFSZ];
	addrtostr(SCII_addr, addrstr_SCII, BUFSZ);

	int server = 0;
	int available = check_availability(SE_socket) && check_availability(SCII_socket);

	if (!available){
		logerror(01);
	}
	else{
		int count = 0;
		char buf[BUFSZ];
		memset(buf, 0, BUFSZ);

		count = send(SE_socket, "REQ_ADD", strlen("REQ_ADD"), 0);
		count = send(SCII_socket, "REQ_ADD", strlen("REQ_ADD"), 0);
		
		if(count != strlen("REQ_ADD") + 1){
			logexit("EXIT_FAILURE");
		}

		count = recv(SE_socket, buf, BUFSZ, 0);
	}

	while(available){
		char buf[BUFSZ];
		char* message = "";

		memset(buf, 0, BUFSZ);
		printf("mensagem> ");
		fgets(buf, BUFSZ-1, stdin);

		message = parse_message(buf, &server);

		size_t count = 0;
		if(server == 0){
			count = send(SCII_socket, message, strlen(message) + 1, 0);
		}
		else{
			count = send(SE_socket, message, strlen(message) + 1, 0);
		}
		if(count != strlen(message) + 1){
			logexit("EXIT_FAILURE");
		}

		memset(buf, 0, BUFSZ);
		if(server == 0){
			count = recv(SCII_socket, buf, BUFSZ, 0);
		}
		else{
			count = recv(SE_socket, buf, BUFSZ, 0);
		}

		//parse_response(buf);
	}

	close(SE_socket);
	close(SCII_socket);

	exit(EXIT_SUCCESS);
}