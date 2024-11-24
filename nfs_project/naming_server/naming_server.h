#ifndef NAMING_SERVER_H
#define NAMING_SERVER_H
#define MAX_FILE_PATH_SIZE 256 
#include <arpa/inet.h>
#include <errno.h>

#define MAX_CONNECTIONS 10

#define MAX_CLIENTS 10   // Define a maximum number of clients

#define SS 1          // For storage server type
#define CLIENT 0      // For client type
#define NS_PORT 8081
#define CREATE 1
#define WRITE 2
#define INFO 3
#define COPY 4
#define LIST 5
#define READ 6
#define STREAM 7

void handleCtrlZ(int signum);
int insert_log(const int type, const int ss_id, const int ss_or_client_port, const int request_type, const char* request_data, const int status_code);
#define REQ_UNSERVICED       600
#define DELETE_FAILED        601
#define CREATE_FAILED        605
#define READ_FAILED          607
#define WRITE_FAILED         608
#define INVALID_DELETION     501
#define FILE_NOT_FOUND       404
#define SERVER_NOT_FOUND     403
// Everything ok
#define OK                   200

int start_naming_server(int port);
void start_accepting_clients(int server_socket_fd);
#endif // NAMING_SERVER_H

