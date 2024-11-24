#ifndef CLIENT_H
#define CLIENT_H

#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#define MAX_FILE_PATH_SIZE 256 
#define MAX_WRITE_SIZE 4096

void connect_to_server(const char *ip, int port, int *sock);

void read_file(int sock, const char *file_path);

void write_file(int sock, const char *file_path);

void get_file_info(int sock, const char *file_path);

void stream_audio_file(int sock, const char *file_path);
void create_file(int sock, const char *file_path, const char *file_name);
void copy_file(int sock, const char *source_path, const char *dest_path);
int print_usage(char *program_name);

#define DELETE_FAILED        601
#define CREATE_FAILED        605
#define INFO_RETRIEVAL_FAILED 606
#define READ_FAILED          607
#define WRITE_FAILED         608
#define INVALID_DELETION     501
#define FILE_NOT_FOUND       404
#define OK                   200




#endif // CLIENT_Hs