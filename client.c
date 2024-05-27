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
	if (strncmp(buf, "kill", strlen("kill")) == 0){
		char kill_message[BUFSZ];
		sprintf(kill_message, "REQ_REM %d", id);
		message = kill_message;
	}
	else if (strncmp(buf, "display info se", strlen("display info se")) == 0){
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

//return an integer in case of status being consulted. 0 for low, 1 for moderate, 2 for high
//in cases where the response is not a status, return -1
int parse_response(char* buf, int server){

	int response_code = -1;	
	if (strncmp(buf, "RES_ADD", strlen("RES_ADD")) == 0){

		strtok(buf, " ");	
		int _id = atoi(strtok(NULL, "\0"));
		id = _id;
		if (server == 1){
			printf("Servidor SE New ID: %d\n", id);
		}
		else if (server == 0){
			printf("Servidor SCII New ID: %d\n", id);
		}
	}

	else if(strncmp(buf, "RES_INFOSE", strlen("RES_INFOSE")) == 0){
		strtok(buf, " ");
		int value = atoi(strtok(NULL, "\0"));
		printf("producao atual: %d kWh\n", value);
	
	}
	else if (strncmp(buf, "RES_INFOSCII", strlen("RES_INFOSCII")) == 0){
		strtok(buf, " ");
		int value = atoi(strtok(NULL, "\0"));
		printf("consumo atual: %d%%\n", value);
	}

	//0 for low, 1 for moderate, 2 for high
	else if (strncmp(buf, "RES_STATUS", strlen("RES_STATUS")) == 0){
		strtok(buf, " ");
		char* status = strtok(NULL, "\0");
		printf("estado atual: %s\n", status);
		if (strncmp(status, "baixa", strlen("baixa")) == 0){
			response_code = 0;
		}
		else if (strncmp(status, "moderada", strlen("moderada")) == 0){
			response_code = 1;
		}
		else if (strncmp(status, "alta", strlen("alta")) == 0){
			response_code = 2;
		}
	}

	return response_code;
}

//method to treat status messages
void treat_status(int status, int sock){

	char buf[BUFSZ];
	memset(buf, 0, BUFSZ);

	//baixa
	if (status == 2){
		send(sock, "REQ_UP", strlen("RES_UP"), 0);
		recv(sock, buf, BUFSZ, 0);
		if(strncmp(buf, "RES_UP", strlen("RES_UP")) == 0){
			strtok(buf, " ");
			int old_value = atoi(strtok(NULL, " "));
			int new_value = atoi(strtok(NULL, "\0"));
			printf("consumo antigo: %d\nconsumo atual: %d\n", old_value, new_value);
		}
		
	}
	else if (status == 1){
		send(sock, "REQ_NONE", strlen("REQ_NONE"), 0);
		recv(sock, buf, BUFSZ, 0);

		if(strncmp(buf, "RES_NONE", strlen("RES_NONE")) == 0){
			strtok(buf, " ");
			int current_value = atoi(strtok(NULL, "\0"));
			printf("consumo antigo: %d\n", current_value);
		}
	}
	else if (status == 0){
		send(sock, "REQ_DOWN", strlen("REQ_DOWN"), 0);
		recv(sock, buf, BUFSZ, 0);
			if(strncmp(buf, "RES_DOWN", strlen("RES_DOWN")) == 0){
			strtok(buf, " ");
			int old_value = atoi(strtok(NULL, " "));
			int new_value = atoi(strtok(NULL, "\0"));
			printf("consumo antigo: %d\nconsumo atual: %d\n", old_value, new_value);
		}
	}
}

//method to check availability of the servers
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
	int available = (check_availability(SE_socket) && check_availability(SCII_socket));

	if (!available){
		logerror(01);
	}
	else{
		size_t count = 0;
		char buf[BUFSZ];
		memset(buf, 0, BUFSZ);

		//send request to add a new client
		count = send(SE_socket, "REQ_ADD", strlen("REQ_ADD"), 0);
		count = send(SCII_socket, "REQ_ADD", strlen("REQ_ADD"), 0);

		if(count != strlen("REQ_ADD")){
			logexit("EXIT_FAILURE");
		}

		memset(buf, 0, BUFSZ);

		//print response for SE server initialization
		count = recv(SE_socket, buf, BUFSZ, 0);
		parse_response(buf, 1);

		memset(buf, 0, BUFSZ);
		//print response for SCII server initialization
		count = recv(SCII_socket, buf, BUFSZ, 0);
		parse_response(buf, 0);

	}

	while(available){
		
		char buf[BUFSZ];
		char* message = "";
		int kill = 0;
		int status_flag = 0;

		memset(buf, 0, BUFSZ);
		printf("mensagem> ");
		fgets(buf, BUFSZ-1, stdin);

		message = parse_message(buf, &server);

		//check if the message is a kill message
		if (strncmp(message, "REQ_REM", strlen("REQ_REM")) == 0){
			kill = 1;
		}
		if(strncmp(message, "REQ_STATUS", strlen("REQ_STATUS")) == 0){
			status_flag = 1;
		}

		size_t count = 0;

		//if kill
		if(kill){

			char* kill_message = strdup(message);
			//send message to the SE server
			count = send(SE_socket, kill_message, strlen(kill_message) + 1, 0);

			//send message to the SCII server
			count = send(SCII_socket, message, strlen(kill_message) + 1, 0);
		}

		//if query condition request
		else if (status_flag){

			memset(buf, 0, BUFSZ);

			//send message to the SE server
			count = send(SE_socket, message, strlen(message) + 1, 0);
			//recieve the status from SE server
			count = recv(SE_socket, buf, BUFSZ, 0);
			int response_status = parse_response(buf, 1);
			treat_status(response_status, SCII_socket);

			//helper message for the SE server to update values
			count = send(SE_socket, "REQ_REFRESH", strlen("REQ_REFRESH"), 0);
			memset(buf, 0, BUFSZ);
			count = recv(SE_socket, buf, BUFSZ, 0);
			continue;
		}

		//send message to the current server
		if(!kill && server == 0){
			count = send(SCII_socket, message, strlen(message) + 1, 0);
		}
		else if (!kill && server == 1){
			count = send(SE_socket, message, strlen(message) + 1, 0);
		}

		if(count != strlen(message) + 1){
			logexit("EXIT_FAILURE");
		}

		memset(buf, 0, BUFSZ);

		if(server == 0 || kill == 1){
			count = recv(SCII_socket, buf, BUFSZ, 0);
		}
		else if (server == 1 || kill == 1){
			count = recv(SE_socket, buf, BUFSZ, 0);
		}

		parse_response(buf, server);

		if(kill == 1 && strncmp(buf, "OK(01)", strlen("OK(01)")) == 0){
			logok(01);
			break;
		}
	}

	close(SE_socket);
	close(SCII_socket);

	exit(EXIT_SUCCESS);
}