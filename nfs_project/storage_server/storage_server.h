#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H

#include <arpa/inet.h>




typedef struct {
    const char *nm_ip;
    int nm_port;
    int client_port;
    int nm_port_to_recieve;
    const char *ip;
    const char* homedir;
    int ss_port;
} ServerParams;

void start_storage_server(void* params);

#define DELETE_FAILED        601
#define READ_FAILED          607
#define WRITE_FAILED         608
#define INVALID_DELETION     501
#define FILE_NOT_FOUND       404
#define OK                   200

#endif // STORAGE_SERVER_H

