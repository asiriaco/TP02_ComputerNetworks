#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 1024

#define MAX_CLIENTS 3

int total_clients = 0;
int available_clients[MAX_CLIENTS] = {0};
int SE_values[MAX_CLIENTS] = {0};


void usage(int argc, char **argv) {
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data {
    int csock;
    struct sockaddr_storage storage;
};

int find_first_available_client(){
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(available_clients[i] == 0){
            return i;
        }
    }
    return -1;
}

int generate_random_SE_value(){
    //generate random value between 20 and 50
    return rand() % 31 + 20;
}

int generate_random_SCII_value(){
    //generate random value between 0 and 100
    return rand() % 101;
}

char* parse_message(char* buf, int* kill){

    char* message = "";
    if (strncmp(buf, "REQ_ADD", strlen("REQ_ADD")) == 0){
        char ok_message[BUFSZ];
        int id = find_first_available_client();
        available_clients[id] = 1;
        sprintf(ok_message, "RES_ADD(%d)", id);
        message = ok_message;
    }

    else if(strncmp(buf, "REQ_REM", strlen("REQ_REM")) == 0){
        *kill = 1;
        char* id_str = strtok(buf, "(");
        id_str = strtok(NULL, ")");
        printf("ID: %s\n", id_str);
        int id = atoi(id_str);
        if(available_clients[id] == 1){
            available_clients[id] = 0;
            message = "OK(01)";
            total_clients--;
            printf("Client %d removed\n", id);
        }
        else{
            logerror(02);
            sprintf(message, "ERROR 02");
        }
    }

    else if (strncmp(buf, "REQ_INFOSE", strlen("REQ_INFOSE")) == 0){
        char value_message[BUFSZ];
        int value = generate_random_SE_value();
        sprintf(value_message, "RES_INFOSE(%d)", value);
        message = value_message;
    }

    else if (strncmp(buf, "REQ_INFOSCII", strlen("REQ_INFOSCII")) == 0){
        char value_message[BUFSZ];
        int value = generate_random_SCII_value();
        sprintf(value_message, "RES_INFOSCII(%d)", value);
        message = value_message;
    }

    return message;
}

void * client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);
    int kill = 0;
    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr);

    while (1)
    {
        char buf[BUFSZ];
        memset(buf, 0, BUFSZ);
        size_t count = recv(cdata->csock, buf, BUFSZ - 1, 0);
        printf("[msg] %s, %d bytes: %s\n", caddrstr, (int)count, buf);

        char* response = parse_message(buf, &kill);

        count = send(cdata->csock, response, strlen(response) + 1, 0);

        if (count != strlen(response) + 1) {
            logexit("send");
        }
        
        if(kill) break;

    }
    close(cdata->csock);
    pthread_exit(EXIT_SUCCESS);

}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("bind");
    }

    if (0 != listen(s, 10)) {
        logexit("listen");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    while (1) {
        printf("[log] waiting connection on %s\n", addrstr);
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("accept");
        }

        if(total_clients >= MAX_CLIENTS){
            logerror(01);
            send(csock, "ERROR 01", strlen("ERROR 01"), 0);
            close(csock);
            continue;
        }

        else{
            total_clients++;
            send(csock, "OK", strlen("OK"), 0);
        }

	struct client_data *cdata = malloc(sizeof(*cdata));
	if (!cdata) {
		logexit("malloc");
	}
	cdata->csock = csock;
	memcpy(&(cdata->storage), &cstorage, sizeof(cstorage));

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata);
    }

    exit(EXIT_SUCCESS);
}
