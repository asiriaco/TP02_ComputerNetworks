#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 1024

#define MAX_CLIENTS 3

int total_clients = 0;
char* port = "-1";
int available_clients[MAX_CLIENTS] = {0};
int info[MAX_CLIENTS] = {0};


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
    srand(time(NULL));
    //generate random value between 20 and 50
    return rand() % 31 + 20;
}

int generate_random_SCII_value(){
    srand(time(NULL));
    //generate random value between 0 and 100
    return rand() % 101;
}

char* eval_SE_value(int value){
    if(value >= 41){
        return "alta";
    }
    else if(value >= 31 && value <= 40){
        return "moderada";
    }
    else{
        return "baixa";
    }
}

char* parse_message(char* buf, int* kill, int *id){

    char* message = "";
    if (strncmp(buf, "REQ_ADD", strlen("REQ_ADD")) == 0){
        char ok_message[BUFSZ];
        *id = find_first_available_client();
        available_clients[*id] = 1;
        if(strncmp(port, "12345", strlen("12345")) == 0){
            info[*id] = generate_random_SE_value();
            printf("Value: %d\n", info[*id]);
        }
        else if (strncmp(port, "54321", strlen("54321")) == 0){
            info[*id] = generate_random_SCII_value();
        }
        sprintf(ok_message, "RES_ADD %d", *id);
        printf("Client %d added\n", *id);
        message = ok_message;
    }

    else if(strncmp(buf, "REQ_REM", strlen("REQ_REM")) == 0){
        *kill = 1;
        strtok(buf, " ");
        char* id_str = strtok(NULL, "\0");
        printf("ID: %s\n", id_str);
        int id = atoi(id_str);
        if(available_clients[id] == 1){
            available_clients[id] = 0;
            info[id] = 0;
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
        sprintf(value_message, "RES_INFOSE %d", info[*id]);
        message = value_message;
    }

    else if (strncmp(buf, "REQ_INFOSCII", strlen("REQ_INFOSCII")) == 0){
        char value_message[BUFSZ];
        sprintf(value_message, "RES_INFOSCII %d", info[*id]);
        message = value_message;
    }

    else if (strncmp(buf, "REQ_STATUS", strlen("REQ_STATUS")) == 0){
        char status_message[BUFSZ];
        sprintf(status_message, "RES_STATUS %s", eval_SE_value(info[*id]));
        message = status_message;
    }
    else if (strncmp(buf, "REQ_UP", strlen("REQ_UP")) == 0){
        int old_value = info[*id];
        int new_value = old_value + rand()%(100-old_value);
        info[*id] = new_value;
        char new_value_message[BUFSZ];
        sprintf(new_value_message, "RES_UP %d %d", old_value, new_value);
        message = new_value_message;
    }
    else if (strncmp(buf, "REQ_NONE", strlen("REQ_NONE")) == 0){
        char none_message[BUFSZ];
        sprintf(none_message, "RES_NONE %d", info[*id]);
    }
    else if(strncmp(buf, "REQ_DOWN", strlen("REQ_DOWN")) == 0){
        int old_value = info[*id];
        int new_value = rand()%old_value;
        info[*id] = new_value;
        char new_value_message[BUFSZ];
        sprintf(new_value_message, "RES_DOWN %d %d", old_value, new_value);
        message = new_value_message;
    }
    else if (strncmp(buf, "REQ_REFRESH", strlen("REQ_REFRESH")) == 0){
        info[*id] = generate_random_SE_value();
        message = "REFRESH OK";
    }
    return message;
}

void * client_thread(void *data) {
    int id;
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
        char* response = parse_message(buf, &kill, &id);
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

    port = argv[2];

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
