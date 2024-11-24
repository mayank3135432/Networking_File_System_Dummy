#include "naming_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include<sys/stat.h>
#include<dirent.h>
#include <signal.h>
//#include "../common/utils.h"
#include <stdbool.h>
#include "naming_utils.c"
static StorageServerInfo storage_servers[MAX_CONNECTIONS];
static int server_count = 0;
static int filecount = 0;


void handleCtrl(int signum) {
  
  FILE* fptr = fopen("log.txt", "r");
     if (fptr == NULL) {
        fprintf(stderr, "Error opening log file: %s (Error Code: %d)\n", strerror(errno), REQ_UNSERVICED);
        return ; 
    }

    char buffer[BUFFER_SIZE] = {0};
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, fptr)) > 0) {
        fwrite(buffer, 1, bytesRead, stdout);
    }

    fclose(fptr);
}

int insert_log(const int type, const int ss_id, const int ss_or_client_port, const int request_type, const char* request_data, const int status_code) {
    FILE* fptr = fopen("./log.txt", "a");
    if (fptr == NULL) {
        fprintf(stderr, "Error opening log file: %s\n", strerror(errno));
        return 0;
    }

    fprintf(fptr, "Communicating with %s\n", (type == SS) ? "Storage Server" : "Client");

if (type == SS) {
    fprintf(fptr, "Storage Server ID                 : %d\n", ss_id);
    fprintf(fptr, "Storage Server Port number        : %d\n", ss_or_client_port);
} else { // CLIENT
    fprintf(fptr, "NS & client Port number           : %d\n", NS_PORT);
}

fprintf(fptr, "NS Port number                    : %d\n", NS_PORT);
fprintf(fptr, "Request type                      : %d\n", request_type);
fprintf(fptr, "Request data                      : %s\n", request_data);
fprintf(fptr, "Status                            : %d\n", status_code);
fprintf(fptr, "\n");

    fclose(fptr);
    return 1;
}
static Trie *file_trie;
//Haandle create command

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    char ss_ip[INET_ADDRSTRLEN];
    int ss_port;
    char file_path[MAX_FILE_PATH_SIZE];
    int status;
    int active;
} ClientInfo;

int check_active_server(const char *ip, int port) {
    for (int i = 0; i < server_count; i++) {
        if (strcmp(storage_servers[i].ip, ip) == 0 && storage_servers[i].port_client == port) {
            return storage_servers[i].is_active;
        }
    }
    return 0;
}
static ClientInfo write_clients[MAX_CONNECTIONS];
static int write_client_count = 0;
void addcommand(const char* ip , int port , char * command)
{
      for (int i = 0; i < server_count; i++) {
        if (strcmp(storage_servers[i].ip, ip) == 0 && storage_servers[i].port_client == port) {
            int z=storage_servers[i].command_count;
            strcpy(storage_servers[i].commands[z],command);
            storage_servers[i].command_count+=1;
            break;
        }
    }
    return ;
}
void handle_command_create(ClientConnection *client_conn, char *command, char *arg1, char *arg2) {
    printf("Processing CREATE command for file: %s\n", arg2);
    FILEINFO *dir_info = find_directory_info(file_trie, arg1);
    char full_path[MAX_FILE_PATH_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", arg1, arg2);
    FILEINFO *file_info = find_file_info(file_trie, full_path);
    if (file_info != NULL) {
        char *response = "File already exists\n";
        send(client_conn->socket_fd, response, strlen(response), 0);
        insert_log(CLIENT, client_conn->socket_fd, NS_PORT, CREATE, "File already exists", FILE_NOT_FOUND);
        return;
    }
    if (dir_info == NULL) {
        char *response = "Directory not found\n";
        send(client_conn->socket_fd, response, strlen(response), 0);
         insert_log(CLIENT, client_conn->socket_fd, NS_PORT, CREATE, "Directory not found", FILE_NOT_FOUND); 

        return;
    }
    printf("Directory found at IP: %s, Port: %d\n", dir_info->ip, dir_info->port_client);
    int main_active=check_active_server(dir_info->ip,dir_info->port_client);

    int nm_port = -1;
    int backup1_nm_port=-1;
    int backup2_nm_port=-1;
    char backupip1[INET_ADDRSTRLEN];
     char backupip2[INET_ADDRSTRLEN];
    for (int i = 0; i < server_count; i++) {
        if ((strcmp(storage_servers[i].ip, dir_info->ip) == 0) && (storage_servers[i].port_client == dir_info->port_client)) {
            nm_port = storage_servers[i].port_nm;
            backup1_nm_port=storage_servers[i].backup_port1;
            backup2_nm_port=storage_servers[i].backup_port2;
            strcpy(backupip1,storage_servers[i].backup_ip1);
            strcpy(backupip2,storage_servers[i].backup_ip2);
            break;
        }
    }

    if (nm_port == -1) {
        perror("No matching storage server found");
        insert_log(CLIENT, client_conn->socket_fd, NS_PORT, CREATE, "No matching storage server found", SERVER_NOT_FOUND); // Log failed command

        return;
    }
 if(main_active == 0)
{
    char *a = "SERVER NOT ACTIVE";
    send(client_conn->socket_fd, a, strlen(a), 0);
    return;
}
    int storage_sock;
    struct sockaddr_in storage_addr;
    if ((storage_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        insert_log(CLIENT, client_conn->socket_fd, nm_port, CREATE, "Socket creation failed", 500); // Log failed command

        return;
    }

    storage_addr.sin_family = AF_INET;
    storage_addr.sin_port = htons(nm_port);

    if (inet_pton(AF_INET, dir_info->ip, &storage_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(storage_sock);
        insert_log(CLIENT, client_conn->socket_fd, nm_port, CREATE, "Invalid address", 400); // Log failed command
        return;
    }

    if (connect(storage_sock, (struct sockaddr *)&storage_addr, sizeof(storage_addr)) < 0) {
        perror("Connection to storage server failed");
        close(storage_sock);
        insert_log(CLIENT, client_conn->socket_fd, nm_port, CREATE, "Connection failed", 500); // Log failed command
        return;
    }

    printf("Connected to storage server at IP: %s, Port: %d\n", dir_info->ip, nm_port);
    send(storage_sock, command, strlen(command), 0);

    char storage_response[BUFFER_SIZE];
    int bytes_received = recv(storage_sock, storage_response, sizeof(storage_response), 0);
    if (bytes_received < 0) {
        perror("Failed to receive response from storage server");
        insert_log(CLIENT, client_conn->socket_fd, nm_port, CREATE, "Failed to receive response", 500); // Log failed command
        close(storage_sock);
        return;
    }

    storage_response[bytes_received] = '\0';
    printf("Response from storage server: %s\n", storage_response);

    if (strstr(storage_response, "success") != NULL) {
        char full_path[MAX_FILE_PATH_SIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", arg1, arg2);
        insert_log(CLIENT, client_conn->socket_fd, nm_port, CREATE, "File created successfully", OK); // Log success
        update_file_info_fromdir(file_trie, dir_info, full_path);
    }
    close(storage_sock);
if(backup1_nm_port!=0 && backup1_nm_port!= -1 &&  backupip1[0]!= '\0' )
    { 
         int backup1_active=check_active_server(backupip1,backup1_nm_port);
      char *src_folder;
    char arg[1000];
    // printf("\n%d\n",backup1_nm_port);
      for (int i = 0; i < server_count; i++) {
        // printf(" %d %s %d\n",i,storage_servers[i].ip,storage_servers[i].port_client);
        if ((strcmp(storage_servers[i].ip, backupip1) == 0) && (storage_servers[i].port_client == backup1_nm_port)) {
           src_folder = strtok(storage_servers[i].accessible_paths, ",");
           printf("%s\n",src_folder);
           strcpy(arg1,src_folder);
            break;
        }
    }
     char name[100];
    char dir_path[1000];
    char file_name [100];
    sscanf(command,"%s %s %s",name,dir_path,file_name);
     char command2[5000];
    snprintf(command2,sizeof(command2),"%s %s/%s %s",name,arg1,dir_path,file_name);
    if(backup1_active == 0 )
    {
        addcommand(backupip1,backup1_nm_port,command2);
    }
    else{
         int backup_storage_sock1;
    struct sockaddr_in storage_addr;
    if ((backup_storage_sock1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "Socket creation failed", 500); // Log failed command

        return;
    }

    storage_addr.sin_family = AF_INET;
    storage_addr.sin_port = htons(backup1_nm_port);

    if (inet_pton(AF_INET, backupip1, &storage_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(backup_storage_sock1);
        insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "Invalid address", 400); // Log failed command
        return;
    }

    if (connect(backup_storage_sock1, (struct sockaddr *)&storage_addr, sizeof(storage_addr)) < 0) {
        perror("Connection to storage server failed");
        close(backup_storage_sock1);
        insert_log(CLIENT, client_conn->socket_fd,backup1_nm_port , CREATE, "Connection failed", 500); // Log failed command
        return;
    }

    printf("Connected to storage server at IP: %s, Port: %d\n", backupip1, backup1_nm_port);

    // printf("%s\n",command2);
    send(backup_storage_sock1, command2, strlen(command2), 0);

    char storage_response[BUFFER_SIZE];
    int bytes_received = recv(backup_storage_sock1, storage_response, sizeof(storage_response), 0);
    if (bytes_received < 0) {
        perror("Failed to receive response from storage server");
        insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "Failed to receive response", 500); // Log failed command
        close(backup_storage_sock1);
        return;
    }

    storage_response[bytes_received] = '\0';
    printf("Response from storage server: %s\n", storage_response);

    if (strstr(storage_response, "success") != NULL) {
        char full_path[MAX_FILE_PATH_SIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", arg1, arg2);
        insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "File created successfully", OK); // Log success
        // update_file_info_fromdir(file_trie, dir_info, full_path);
    }
    close(backup_storage_sock1);
    }
    }
if(backup2_nm_port!=0 && backup2_nm_port!= -1 &&  backupip2[0]!= '\0' )
    {  int backup2_active=check_active_server(backupip2,backup2_nm_port);
    char name[100];
    char dir_path[1000];
    char file_name [100];
    sscanf(command,"%s %s %s",name,dir_path,file_name);
    char *src_folder;
    char arg[1000];
    // printf("\n%d\n",backup2_nm_port);
      for (int i = 0; i < server_count; i++) {
        // printf(" %d %s %d\n",i,storage_servers[i].ip,storage_servers[i].port_client);
        if ((strcmp(storage_servers[i].ip, backupip2) == 0) && (storage_servers[i].port_client == backup2_nm_port)) {
           src_folder = strtok(storage_servers[i].accessible_paths, ",");
           printf("%s\n",src_folder);
           strcpy(arg1,src_folder);
            break;
        }
    }
    char command2[5000];
    snprintf(command2,sizeof(command2),"%s %s/%s %s",name,arg1,dir_path,file_name);
    if(backup2_active == 0 )
    {
        addcommand(backupip2,backup2_nm_port,command2);
    }
    else{
         int backup_storage_sock2;
    struct sockaddr_in storage_addr;
    if ((backup_storage_sock2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        insert_log(CLIENT, client_conn->socket_fd, backup2_nm_port, CREATE, "Socket creation failed", 500); // Log failed command

        return;
    }

    storage_addr.sin_family = AF_INET;
    storage_addr.sin_port = htons(backup2_nm_port);

    if (inet_pton(AF_INET, backupip2, &storage_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(backup_storage_sock2);
        insert_log(CLIENT, client_conn->socket_fd, backup2_nm_port, CREATE, "Invalid address", 400); // Log failed command
        return;
    }

    if (connect(backup_storage_sock2, (struct sockaddr *)&storage_addr, sizeof(storage_addr)) < 0) {
        perror("Connection to storage server failed");
        close(backup_storage_sock2);
        insert_log(CLIENT, client_conn->socket_fd,backup2_nm_port , CREATE, "Connection failed", 500); // Log failed command
        return;
    }

    printf("Connected to storage server at IP: %s, Port: %d\n", backupip2, backup2_nm_port);

    // printf("%s\n",command2);
    send(backup_storage_sock2, command2, strlen(command2), 0);

    char storage_response[BUFFER_SIZE];
    int bytes_received = recv(backup_storage_sock2, storage_response, sizeof(storage_response), 0);
    if (bytes_received < 0) {
        perror("Failed to receive response from storage server");
        insert_log(CLIENT, client_conn->socket_fd, backup2_nm_port, CREATE, "Failed to receive response", 500); // Log failed command
        close(backup_storage_sock2);
        return;
    }

    storage_response[bytes_received] = '\0';
    printf("Response from storage server: %s\n", storage_response);

    if (strstr(storage_response, "success") != NULL) {
        char full_path[MAX_FILE_PATH_SIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", arg1, arg2);
        insert_log(CLIENT, client_conn->socket_fd, backup2_nm_port, CREATE, "File created successfully", OK); // Log success
        // update_file_info_fromdir(file_trie, dir_info, full_path);
    }
    close(backup_storage_sock2);
    }
         }
    send(client_conn->socket_fd, storage_response, bytes_received, 0);
    
}

void handle_command_write(ClientConnection *client_conn, char *command, char *arg1, char* arg2) {
    printf("Processing WRITE command for file: %s\n", command);
    printf("Processing WRITE command for file: %s\n", arg1);
    FILEINFO *file_info = find_file_info(file_trie, arg1);
    if (file_info == NULL) {
        char *response = "File not found\n";
        send(client_conn->socket_fd, response, strlen(response), 0);
        insert_log(CLIENT, client_conn->socket_fd, NS_PORT, WRITE, "File not found", FILE_NOT_FOUND); // Log failed command

        return;
    }
    // Check if the command has --sync at the end
    printf("File found at IP: %s, Port: %d\n", file_info->ip, file_info->port_client);
      int check_active=check_active_server(file_info->ip,file_info->port_client);
    if(check_active==0)
    {

    }
    send(client_conn->socket_fd, file_info, sizeof(FILEINFO), 0);
    insert_log(CLIENT, client_conn->socket_fd, file_info->port_client, WRITE, "Write request processed", OK); // Log success

    bool has_sync = strstr(command, "--sync") != NULL;

    if (!has_sync) {
        // Append the client to the list of write clients
        if (write_client_count < MAX_CONNECTIONS) {
            strcpy(write_clients[write_client_count].ip, inet_ntoa(client_conn->client_addr.sin_addr));
            write_clients[write_client_count].port = ntohs(client_conn->client_addr.sin_port);
            strcpy(write_clients[write_client_count].ss_ip, file_info->ip);
            write_clients[write_client_count].ss_port = file_info->port_client;
            strcpy(write_clients[write_client_count].file_path, arg1);
            write_clients[write_client_count].status = 0;
            write_clients[write_client_count].active = 1;
            write_client_count++;
            while(1){
                if(write_clients[write_client_count-1].status == 1 || write_clients[write_client_count-1].status == 2){
                    if (write_clients[write_client_count-1].status == 2){
                        char *response = "Write failed\n";
                        send(client_conn->socket_fd, response, strlen(response), 0);
                    }
                    else{
                    char *response = "Write completed\n";
                    send(client_conn->socket_fd, response, strlen(response), 0);
                    write_clients[write_client_count-1].active = 0;
                    break;
                }
            }
            
        } 
       
    }
    }
     else {
            printf("Maximum write clients reached.\n");
        }
    
    // Implement the logic to handle WRITE command
}
void handle_command_info(ClientConnection *client_conn, char *arg1) {
    printf("Processing INFO command for file: %s\n", arg1);
    FILEINFO *file_info = find_file_info(file_trie, arg1);
    FILEINFO empty_file_info;

    if (file_info == NULL) {
        memset(&empty_file_info, 0, sizeof(FILEINFO));
        send(client_conn->socket_fd, &empty_file_info, sizeof(FILEINFO), 0);
        insert_log(CLIENT, client_conn->socket_fd, NS_PORT, INFO, "File not found",FILE_NOT_FOUND); // Log failed command

        return;
    }
     int check_active=check_active_server(file_info->ip,file_info->port_client);
    if(check_active==0)
    {
         char *a = "SERVER NOT ACTIVE";
    send(client_conn->socket_fd, a, strlen(a), 0);
    return;
    }
    printf("File found at IP: %s, Port: %d\n", file_info->ip, file_info->port_client);
    send(client_conn->socket_fd, file_info, sizeof(FILEINFO), 0);
    insert_log(CLIENT, client_conn->socket_fd, file_info->port_client, INFO, "File info sent", OK); // Log success

}
void handle_command_copy(ClientConnection *client_conn, char *arg1, char *arg2) {
    printf("Processing COPY command from %s to %s\n", arg1, arg2);
    char cwd[BUFFER_SIZE];
    if (!getcwd(cwd, sizeof(cwd))) {
        perror("Unable to retrieve current working directory");
        char *response = "Server error: Unable to retrieve current working directory\n";
        send(client_conn->socket_fd, response, strlen(response), 0);
        return;
    }
    char base_path[5000];
    snprintf(base_path, sizeof(base_path), "%s/../storage_server", cwd);
    // Append arguments to the working directory
    char src_path[5100];
    char dest_path[5100];
    snprintf(src_path, sizeof(src_path), "%s/%s", base_path, arg1);
    snprintf(dest_path, sizeof(dest_path), "%s/%s", base_path, arg2);
    printf("%s\n",src_path);
    // Check if src_path is a valid file or directory
    struct stat source_stat;
    if (stat(src_path, &source_stat) < 0) {
        perror("Source path error");
        char *response = "Source path not found\n";
        // send(client_conn->socket_fd, response, strlen(response), 0);
        return;
    }
    struct stat dest_stat;
    if (stat(dest_path, &dest_stat) < 0) {
        perror("Dest path error");
        char *response = "Dest path not found\n";
        // send(client_conn->socket_fd, response, strlen(response), 0);
        return;
    }
    int is_source_dir=0;
    int is_dest_dir=0;
    if (S_ISDIR(source_stat.st_mode)) {
        // printf("The source path %s is a directory\n", src_path);
        is_source_dir=1;
    } else {
        // printf("The source path %s is not a directory\n", src_path);
    }
        // return;
    if (S_ISDIR(dest_stat.st_mode)) {
        // printf("The dest path %s is a directory\n", dest_path);
        is_dest_dir=1;
    } else {
        // printf("The dest path %s is not a directory\n", src_path);
    }
    // Retrieve file information for source
    if (is_dest_dir && is_source_dir ){    
        FILEINFO *copied_file_info;
        FILEINFO *source_file_info = find_file_info(file_trie, arg1);
        if (source_file_info != NULL) {
        // Allocate memory for a new FILEINFO structure
        copied_file_info = malloc(sizeof(FILEINFO));
        if (copied_file_info == NULL) {
            perror("Failed to allocate memory for FILEINFO copy");
            exit(EXIT_FAILURE);
        }

        // Copy the contents of source_file_info into copied_file_info
        memcpy(copied_file_info, source_file_info, sizeof(FILEINFO));  
        }
        if (source_file_info == NULL) {
            char *response = "Source File not found\n";
            send(client_conn->socket_fd, response, strlen(response), 0);
            return;
        }
    
    int check_active2=check_active_server(source_file_info->ip,source_file_info->port_client);
    if(check_active2==0)
    {
         char *a = "SERVER NOT ACTIVE";
    send(client_conn->socket_fd, a, strlen(a), 0);
    return;
    }
        int source_socket = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in source_addr;
        source_addr.sin_family = AF_INET;
        source_addr.sin_port = htons(source_file_info->port_client);
        inet_pton(AF_INET, source_file_info->ip, &source_addr.sin_addr);

        if (connect(source_socket, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
            perror("Error connecting to source server");
            return;
        }
    
        
        // printf("%s\n",arg2);
        FILEINFO *dest_file_info = find_file_info(file_trie, arg2);
        if (dest_file_info == NULL) {
            char *response = "Dest File not found\n";
            send(client_conn->socket_fd, response, strlen(response), 0);
        
            return;
        }
        printf("Destination File found at IP: %s, Port: %d\n", 
        dest_file_info->ip, dest_file_info->port_client);
        int check_active=check_active_server(dest_file_info->ip,dest_file_info->port_client);
    if(check_active==0)
    {
         char *a = "SERVER NOT ACTIVE";
    send(client_conn->socket_fd, a, strlen(a), 0);
    return;
    }
        printf("Source File found at IP: %s, Port: %d, Path: %s\n", 
        copied_file_info->ip, copied_file_info->port_client, copied_file_info->file_path); 
        int flag=0;
        if(copied_file_info->port_client==dest_file_info->port_client && strcmp(copied_file_info->ip,dest_file_info->ip)==0){
            flag=1;
        }      
        char request[BUFFER_SIZE];
        memset(request,0,sizeof(request));
        snprintf(request, sizeof(request), "COPY_FOLDER %s %s %d %s %d", copied_file_info->file_path,dest_file_info->ip,dest_file_info->port_client ,dest_file_info->file_path,flag); 
        // printf("%s\n",request);
        
        copy_directory_trie(file_trie, arg1, arg2);


        send(source_socket, request, strlen(request), 0);
        sleep(0.5);
        char msg[1024];
        int z=recv(source_socket,msg,strlen(msg)-1,0);
        msg[z]='\0';
        printf("%s\n",msg);
        send(client_conn->socket_fd, msg, sizeof(msg), 0);
        // printf("Sending Destination File found at IP: %s, Port: %d\n", 
        //        dest_file_info->ip, dest_file_info->port_client);
        // sleep(0.5);
        // send(source_socket,dest_file_info,sizeof(FILEINFO),0);

        
        close (source_socket);
        return;
    }
    if(is_source_dir && !is_dest_dir){
        char *response = "SOURCE IS A DIRECTORY AND DEST IS A FILE\n";
        send(client_conn->socket_fd, response, strlen(response), 0);
        return;
    }

// if(!is_source_dir && is_dest_dir)
// {
// char pp[BUFFER_SIZE];
// snprintf(pp,sizeof(pp),"touch ")

// }
    FILEINFO *source_file_info = find_file_info(file_trie, arg1);
    if (source_file_info == NULL) {
        char *response = "Source File not found\n";
        send(client_conn->socket_fd, response, strlen(response), 0);
        return;
    }
    printf("Source File found at IP: %s, Port: %d, Path: %s\n", 
           source_file_info->ip, source_file_info->port_client, source_file_info->file_path);
            int check_active=check_active_server(source_file_info->ip,source_file_info->port_client);
    if(check_active==0)
    {
         char *a = "SOURCE SERVER NOT ACTIVE";
    send(client_conn->socket_fd, a, strlen(a), 0);
    return;
    }
    // char request[BUFFER_SIZE];
    // snprintf(request, sizeof(request), "COPY_FOLDER %s", source_file_info->file_path);
    int source_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in source_addr;
    source_addr.sin_family = AF_INET;
    source_addr.sin_port = htons(source_file_info->port_client);
    inet_pton(AF_INET, source_file_info->ip, &source_addr.sin_addr);

    if (connect(source_socket, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
        perror("Error connecting to source server");
        return;
    }
    char request[BUFFER_SIZE];
    // Request the file from the source server
    snprintf(request, sizeof(request), "READ %s", source_file_info->file_path);
    send(source_socket, request, strlen(request), 0);
 
    FILEINFO *dest_file_info = find_file_info(file_trie, arg2);
    if (dest_file_info == NULL) {
        char *response = "Dest File not found\n";
        send(client_conn->socket_fd, response, strlen(response), 0);
     
        return;
    }
    printf("Destination File found at IP: %s, Port: %d\n", 
           dest_file_info->ip, dest_file_info->port_client);
             int check_active2=check_active_server(dest_file_info->ip,dest_file_info->port_client);
    if(check_active2==0)
    {
         char *a = "DESTINATION SERVER NOT ACTIVE";
    send(client_conn->socket_fd, a, strlen(a), 0);
    return;
    }
               int dest_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_file_info->port_client);
    inet_pton(AF_INET, dest_file_info->ip, &dest_addr.sin_addr);

    if (connect(dest_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("Error connecting to destination server");
        close(source_socket);
        return;
    }


    char request2[BUFFER_SIZE];
    snprintf(request2, sizeof(request2), "COPY_FILE|%s", dest_file_info->file_path);
    send(dest_socket, request2, strlen(request2), 0);
    char buffer[BUFFER_SIZE];
    char full_data[BUFFER_SIZE * 10];  // Accumulate data from source
    int total_bytes_received = 0;
    ssize_t bytes_read;

    // Receive the data from the source server
    while ((bytes_read = recv(source_socket, buffer, sizeof(buffer), 0)) > 0) {
        // Accumulate data in full_data buffer
        if (total_bytes_received + bytes_read < sizeof(full_data)) {
            memcpy(full_data + total_bytes_received, buffer, bytes_read);
            total_bytes_received += bytes_read;
        } else {
            printf("Data buffer overflow. Exceeded allocated size.\n");
            break;
        }
    }

    if (bytes_read < 0) {
        perror("Error receiving data from source server");
    }
    printf("%s",full_data);
    // Send accumulated data to destination server
    if (total_bytes_received > 0) {
        if (send(dest_socket, full_data, total_bytes_received, 0) == -1) {
            perror("Error sending data to destination server");
        } else {
            printf("Successfully sent %d bytes to destination server.\n", total_bytes_received);
        }
    }
    // send(ss_sock, data, strlen(data), 0);
    char response4[BUFFER_SIZE];
    int bytes_received = recv(dest_socket, response4, sizeof(response4), 0);
    if (bytes_received > 0) {
        response4[bytes_received] = '\0';
        printf("Response from storage server: %s\n", response4);
    }
    char *sync_flag = strstr(arg2, " --sync");
    if (sync_flag == NULL) {
        printf("Waiting for synchronous write confirmation\n");
        char sync_response[BUFFER_SIZE];
        int bytes_received = recv(dest_socket, sync_response, sizeof(sync_response), 0);
        if (bytes_received > 0) {
            sync_response[bytes_received] = '\0';
            printf("Synchronous write response: %s\n", sync_response);
        }
    }
    close(source_socket);
    close (dest_socket);

}

void backup(char* arg1 , char* arg2)
{
    FILEINFO *copied_file_info;
        FILEINFO *source_file_info = find_file_info(file_trie, arg1);
        if (source_file_info != NULL) {
    // Allocate memory for a new FILEINFO structure
copied_file_info = malloc(sizeof(FILEINFO));
    if (copied_file_info == NULL) {
        perror("Failed to allocate memory for FILEINFO copy");
        exit(EXIT_FAILURE);
    }

    // Copy the contents of source_file_info into copied_file_info
    memcpy(copied_file_info, source_file_info, sizeof(FILEINFO));
          }
    if (source_file_info == NULL) {
        char *response = "Source File not found\n";
        // send(client_conn->socket_fd, response, strlen(response), 0);
        return;
    }
     int source_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in source_addr;
    source_addr.sin_family = AF_INET;
    source_addr.sin_port = htons(source_file_info->port_client);
    inet_pton(AF_INET, source_file_info->ip, &source_addr.sin_addr);

    if (connect(source_socket, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
        perror("Error connecting to source server");
        return;
    }
   
    
    printf("%s\n",arg2);
    FILEINFO *dest_file_info = find_file_info(file_trie, arg2);
    if (dest_file_info == NULL) {
        char *response = "Dest File not found\n";
        printf("%s",response);
        // send(client_conn->socket_fd, response, strlen(response), 0);
    
        return;
    }
    printf("Destination File found at IP: %s, Port: %d\n", 
           dest_file_info->ip, dest_file_info->port_client);
     printf("Source File found at IP: %s, Port: %d, Path: %s\n", 
           copied_file_info->ip, copied_file_info->port_client, copied_file_info->file_path); 
           int flag=0;
        if(copied_file_info->port_client==dest_file_info->port_client && strcmp(copied_file_info->ip,dest_file_info->ip)==0)
        {
            flag=1;
        }  
         char request[BUFFER_SIZE];
    memset(request,0,sizeof(request));
    snprintf(request, sizeof(request), "COPY_FOLDER %s %s %d %s %d", copied_file_info->file_path,dest_file_info->ip,dest_file_info->port_client ,dest_file_info->file_path,flag); 
    printf("%s\n",request);

    send(source_socket, request, strlen(request), 0);
    sleep(0.5);  
     char msg[1024];
    int z=recv(source_socket,msg,strlen(msg)-1,0);
    msg[z]='\0';
    printf("%s\n",msg);
      close (source_socket);
      return;
}

void handle_command_delete(ClientConnection *client_conn, char *command, char *arg1) {
    // Check if the path is a file or directory by extension
    bool is_file = strchr(arg1, '.') != NULL;

    if (is_file) {
        // Search for file
        FILEINFO *file_info = find_file_info(file_trie, arg1);
        if (file_info == NULL) {
            char *response = "File not found\n";
            send(client_conn->socket_fd, response, strlen(response), 0);
            return;
        }
        printf("File found at IP: %s, Port: %d\n", file_info->ip, file_info->port_client);
           int check_active=check_active_server(file_info->ip,file_info->port_client);
    if(check_active==0)
    {
         char *a = "SERVER NOT ACTIVE";
    send(client_conn->socket_fd, a, strlen(a), 0);
    return;
    }

        // Connect to the storage server with the obtained IP and Port
        int nm_port = -1;
        int backup1_nm_port=-1;
    int backup2_nm_port=-1;
    char backupip1[INET_ADDRSTRLEN];
     char backupip2[INET_ADDRSTRLEN];
        for (int i = 0; i < server_count; i++) {
            if ((strcmp(storage_servers[i].ip, file_info->ip) == 0) && (storage_servers[i].port_client == file_info->port_client)) {
                nm_port = storage_servers[i].port_nm;
                backup1_nm_port=storage_servers[i].backup_port1;
            backup2_nm_port=storage_servers[i].backup_port2;
            strcpy(backupip1,storage_servers[i].backup_ip1);
            strcpy(backupip2,storage_servers[i].backup_ip2);
                break;
            }
        }
        if (nm_port == -1) {
            perror("No matching storage server found");
            return;
        }
        int storage_sock;
        struct sockaddr_in storage_addr;
        // Create socket
        if ((storage_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation error");
            return;
        }
        storage_addr.sin_family = AF_INET;
        storage_addr.sin_port = htons(nm_port);
        // Convert IP address from text to binary form
        if (inet_pton(AF_INET, file_info->ip, &storage_addr.sin_addr) <= 0) {
            perror("Invalid address/ Address not supported");
            close(storage_sock);
            return;
        }
        // Connect to the storage server
        if (connect(storage_sock, (struct sockaddr *)&storage_addr, sizeof(storage_addr)) < 0) {
            perror("Connection to storage server failed");
            close(storage_sock);
            return;
        }
        printf("Connected to storage server at IP: %s, Port: %d\n", file_info->ip, nm_port);
        // Send the command to the storage server
        send(storage_sock, command, strlen(command), 0);
        // Receive the response from the storage server
        char storage_response[BUFFER_SIZE];
        int bytes_received = recv(storage_sock, storage_response, sizeof(storage_response), 0);
        if (bytes_received < 0) {
            perror("Failed to receive response from storage server");
            close(storage_sock);
            return;
        }
        storage_response[bytes_received] = '\0'; // Null-terminate the response
        printf("Response from storage server: %s\n", storage_response);
        // If deletion was successful, remove the file from the cache
        remove_from_cache(file_info->file_path);
        // If deletion was successful, remove the file from the files trie
        delete_file_info(file_trie, file_info->file_path);

        // Send the response back to the client
        send(client_conn->socket_fd, storage_response, bytes_received, 0);
        close(storage_sock);
                   
if(backup1_nm_port!=0 && backup1_nm_port!= -1 &&  backupip1[0]!= '\0' )
    {  int backup1_active=check_active_server(backupip1,backup1_nm_port);
        char name[100];
    char dir_path[1000];
    char file_name [100];
    sscanf(command,"%s %s",name,dir_path);
    char *src_folder;
    char arg[1000];
    // printf("\n%d\n",backup1_nm_port);
      for (int i = 0; i < server_count; i++) {
        // printf(" %d %s %d\n",i,storage_servers[i].ip,storage_servers[i].port_client);
        if ((strcmp(storage_servers[i].ip, backupip1) == 0) && (storage_servers[i].port_client == backup1_nm_port)) {
           src_folder = strtok(storage_servers[i].accessible_paths, ",");
           printf("%s\n",src_folder);
           strcpy(arg1,src_folder);
            break;
        }
    }
    char command2[5000];
    snprintf(command2,sizeof(command2),"%s %s/%s",name,arg1,dir_path);
    if(backup1_active == 0 )
    {
        addcommand(backupip1,backup1_nm_port,command2);
    }
    else{      
         int backup_storage_sock1;
    struct sockaddr_in storage_addr;
    if ((backup_storage_sock1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "Socket creation failed", 500); // Log failed command

        return;
    }

    storage_addr.sin_family = AF_INET;
    storage_addr.sin_port = htons(backup1_nm_port);

    if (inet_pton(AF_INET, backupip1, &storage_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(backup_storage_sock1);
        insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "Invalid address", 400); // Log failed command
        return;
    }

    if (connect(backup_storage_sock1, (struct sockaddr *)&storage_addr, sizeof(storage_addr)) < 0) {
        perror("Connection to storage server failed");
        close(backup_storage_sock1);
        insert_log(CLIENT, client_conn->socket_fd,backup1_nm_port , CREATE, "Connection failed", 500); // Log failed command
        return;
    }

    printf("Connected to storage server at IP: %s, Port: %d\n", backupip1, backup1_nm_port);

    // printf("%s\n",command2);
    send(backup_storage_sock1, command2, strlen(command2), 0);

    close(backup_storage_sock1);
    }
    }
    
if(backup2_nm_port!=0 && backup2_nm_port!= -1 &&  backupip2[0]!= '\0' )
    {   int backup2_active=check_active_server(backupip2,backup2_nm_port);
        char name[100];
    char dir_path[1000];
    char file_name [100];
    sscanf(command,"%s %s",name,dir_path);
    char *src_folder;
    char arg[1000];
    // printf("\n%d\n",backup2_nm_port);
      for (int i = 0; i < server_count; i++) {
        // printf(" %d %s %d\n",i,storage_servers[i].ip,storage_servers[i].port_client);
        if ((strcmp(storage_servers[i].ip, backupip2) == 0) && (storage_servers[i].port_client == backup2_nm_port)) {
           src_folder = strtok(storage_servers[i].accessible_paths, ",");
           printf("%s\n",src_folder);
           strcpy(arg1,src_folder);
            break;
        }
    }
    char command2[5000];
    snprintf(command2,sizeof(command2),"%s %s/%s",name,arg1,dir_path);
    if(backup2_active == 0 )
    {
        addcommand(backupip2,backup2_nm_port,command2);
    }
    else{
         int backup_storage_sock2;
    struct sockaddr_in storage_addr;
    if ((backup_storage_sock2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        insert_log(CLIENT, client_conn->socket_fd, backup2_nm_port, CREATE, "Socket creation failed", 500); // Log failed command

        return;
    }

    storage_addr.sin_family = AF_INET;
    storage_addr.sin_port = htons(backup2_nm_port);

    if (inet_pton(AF_INET, backupip2, &storage_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(backup_storage_sock2);
        insert_log(CLIENT, client_conn->socket_fd, backup2_nm_port, CREATE, "Invalid address", 400); // Log failed command
        return;
    }

    if (connect(backup_storage_sock2, (struct sockaddr *)&storage_addr, sizeof(storage_addr)) < 0) {
        perror("Connection to storage server failed");
        close(backup_storage_sock2);
        insert_log(CLIENT, client_conn->socket_fd,backup2_nm_port , CREATE, "Connection failed", 500); // Log failed command
        return;
    }

    printf("Connected to storage server at IP: %s, Port: %d\n", backupip2, backup2_nm_port);
 
    send(backup_storage_sock2, command2, strlen(command2), 0);

    close(backup_storage_sock2);
    }
    }
    }
}

void handle_command_stream(ClientConnection *client_conn, char *arg1) {
    printf("Processing STREAM command for file: %s\n", arg1);
    FILEINFO *file_info = find_file_info(file_trie, arg1);
    FILEINFO empty_file_info;
    if (file_info == NULL) {
        memset(&empty_file_info, 0, sizeof(FILEINFO));
        printf("%doporordef\n", empty_file_info.port_client);
        empty_file_info.port_client = 0;
        strcpy(empty_file_info.file_path, "File not found");
        send(client_conn->socket_fd, &empty_file_info, sizeof(FILEINFO), 0);
    insert_log(CLIENT, client_conn->socket_fd, file_info->port_client, STREAM, "File not found", FILE_NOT_FOUND); // Log success
        return;
    }

    printf("File found at IP: %s, Port: %d\n", file_info->ip, file_info->port_client);
       int check_active=check_active_server(file_info->ip,file_info->port_client);
    if(check_active==0)
    {
         memset(&empty_file_info, 0, sizeof(FILEINFO));
        // printf("%doporordef\n", empty_file_info.port_client);
        empty_file_info.port_client = 0;
        strcpy(empty_file_info.file_path, "SERVER NOT ACTIVE");
        send(client_conn->socket_fd, &empty_file_info, sizeof(FILEINFO), 0);
    return;
    }
    send(client_conn->socket_fd, file_info, sizeof(FILEINFO), 0);
    insert_log(CLIENT, client_conn->socket_fd, file_info->port_client, STREAM, "Stream request processed", OK); // Log success

}


char* getaccesspath(char* ip , int port)
{   
    for(int i=0;i<server_count;i++)
    {
        if(storage_servers[i].port_client==port && strcmp(storage_servers[i].ip,ip)==0)
        {
            //  char *src_folder = strtok(storage_servers[i].accessible_paths, ",");
              char* src_folder = strtok(storage_servers[i].accessible_paths, ",");
            if (src_folder) {
                // Allocate memory for the path and return it
                char* result = malloc(strlen(src_folder) + 1);
                if (result) {
                    strcpy(result, src_folder);
                    return result;
                }
            }
        }
    }
    return NULL;
}

void handle_command_read(ClientConnection *client_conn, char *arg1) {
    printf("Processing READ command for file: %s\n", arg1);
    FILEINFO *file_info = find_file_info(file_trie, arg1);
    FILEINFO response_info = {0};
    FILEINFO empty_file_info;
    if (file_info == NULL) {
        char *response = "File not found\n";
        memset(&empty_file_info, 0, sizeof(FILEINFO));
        send(client_conn->socket_fd, &empty_file_info, sizeof(FILEINFO), 0);
        insert_log(CLIENT, client_conn->socket_fd, NS_PORT, READ, "File not found", FILE_NOT_FOUND); // Log failed command
        return;
    }
    printf("FILEINFO:\n");
    printf("  IP: %s\n", file_info->ip);
    printf("  Port: %d\n", file_info->port_client);
    printf("  File Path: %s\n", file_info->file_path);

    strcpy(response_info.file_path, file_info->file_path);
    strcpy(response_info.ip, file_info->ip);
    response_info.port_client = file_info->port_client;
int active_status=1;
          for (int i = 0; i < server_count; i++) { 
    if (strcmp(storage_servers[i].ip, file_info->ip) == 0 &&
        storage_servers[i].port_client == file_info->port_client) 
        {
    
            if(storage_servers[i].is_active == 0)
            { printf("SERVER NOT ACTIVE SEARCHING FOR BACKUP_SERVERS");
                active_status = 0;   
                if (check_active_server(storage_servers[i].backup_ip1, storage_servers[i].backup_port1)) {
                    strcpy(response_info.ip, storage_servers[i].backup_ip1);
                    response_info.port_client = storage_servers[i].backup_port1;
                    char* a=getaccesspath(response_info.ip,response_info.port_client);
                    char command[2000];
                    snprintf(command,sizeof(command),"%s/%s",a,arg1);
                    strcpy(response_info.file_path,command);
                    active_status=1;
                    break;
                }
                else if (check_active_server(storage_servers[i].backup_ip2, storage_servers[i].backup_port2)) {
                    strcpy(response_info.ip, storage_servers[i].backup_ip2);
                    response_info.port_client = storage_servers[i].backup_port2;
                    char* a=getaccesspath(response_info.ip,response_info.port_client);
                    char command[2000];
                    snprintf(command,sizeof(command),"%s/%s",a,arg1);
                    strcpy(response_info.file_path,command);
                    active_status=1;
                    break;
                }
            }
            break;
        }
 
          }
    if(active_status)
    {
    printf("File found at IP: %s, Port: %d\n", response_info.ip,response_info.port_client);
    printf("File Path: %s\n", response_info.file_path);
    printf("IP: %s\n", response_info.ip);
    printf("Port: %d\n", response_info.port_client);
    send(client_conn->socket_fd, &response_info, sizeof(FILEINFO), 0);
        insert_log(CLIENT, client_conn->socket_fd, NS_PORT, READ, "Read request processed", OK); // Log failed command
    }
    else{
        char *response = "File not found\n";
        memset(&empty_file_info, 0, sizeof(FILEINFO));
        send(client_conn->socket_fd, &empty_file_info, sizeof(FILEINFO), 0);
        insert_log(CLIENT, client_conn->socket_fd, NS_PORT, READ, "File not found", FILE_NOT_FOUND); // Log failed command
        return;
    }
}


// Function to process commands from the client
void process_client_command(ClientConnection *client_conn, char *command) {
    char cmd[BUFFER_SIZE], arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];
    sscanf(command, "%s %s %s", cmd, arg1, arg2);
    printf("IN THE FUNCTION !! Command: %s\n", cmd);
    

    //printf("strcmp between <%s> and <%s> is %d\n",cmd, "WRITE", strncmp(cmd, "WRITE",5)); // debug line


    if (strcmp(cmd, "READ") == 0) {
        handle_command_read(client_conn, arg1);
    } else if (strncmp(cmd, "write",5) == 0) {
        handle_command_write(client_conn, command, arg1, arg2);
    } else if (strcmp(cmd, "INFO") == 0) {
        handle_command_info(client_conn, arg1);
    } else if (strcmp(cmd, "STREAM") == 0) {
        handle_command_stream(client_conn, arg1);
    } else if (strcmp(cmd, "CREATE") == 0) {
        handle_command_create(client_conn, command, arg1, arg2);
    }else if (strcmp(cmd, "COPY") == 0) {
        printf("is it coming here ?\n");
        handle_command_copy(client_conn, arg1, arg2);
    } else if (strcmp(cmd, "DELETE") == 0) {
        handle_command_delete(client_conn, command, arg1);
    }
    else if(strcmp(cmd,"list")==0){
        printf("list command to %s\n", arg1);
        list_files(file_trie, arg1, client_conn->socket_fd);
        sleep(1);
        char* response = "STOP\n";
        send(client_conn->socket_fd, response, strlen(response), 0);
    }
}


void replay_commands(const char* ip , int port)
{    
   
    for (int i =0;i<server_count;i++)
    {
        if (storage_servers[i].port_client==port && strcmp(storage_servers[i].ip ,ip)==0)
        {
            for ( int j=0;j<storage_servers[i].command_count;j++)
            {    int backup_storage_sock1;
    struct sockaddr_in storage_addr;
    if ((backup_storage_sock1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        // insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "Socket creation failed", 500); // Log failed command

        return;
    }

    storage_addr.sin_family = AF_INET;
    storage_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &storage_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(backup_storage_sock1);
        // insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "Invalid address", 400); // Log failed command
        return;
    }

    if (connect(backup_storage_sock1, (struct sockaddr *)&storage_addr, sizeof(storage_addr)) < 0) {
        perror("Connection to storage server failed");
        close(backup_storage_sock1);
        // insert_log(CLIENT, client_conn->socket_fd,backup1_nm_port , CREATE, "Connection failed", 500); // Log failed command
        return;
    }
                printf("EXECUTING COMMAND : %s \n",storage_servers[i].commands[j]);
                send(backup_storage_sock1, storage_servers[i].commands[j],strlen(storage_servers[i].commands[j]),0);
             char storage_response[BUFFER_SIZE];
    int bytes_received = recv(backup_storage_sock1, storage_response, sizeof(storage_response), 0);
    if (bytes_received < 0) {
        perror("Failed to receive response from storage server");
        // insert_log(CLIENT, client_conn->socket_fd, backup1_nm_port, CREATE, "Failed to receive response", 500); // Log failed command
        close(backup_storage_sock1);
        return;

    }
    storage_response[bytes_received] = '\0';
    printf("Response from storage server: %s\n", storage_response);
    sleep(1);
        }
    }
}
}
// Function to handle each storage server connection in a new thread
void *handle_storage_server(void *socket_desc) {
    int sock = *(int *)socket_desc;
    free(socket_desc); // Free dynamically allocated memory
    char buffer100[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer100, sizeof(buffer100) - 1, 0);
    if (bytes_received < 0) {
        perror("Failed to receive message from storage server");
        close(sock);
        return NULL;
    }
    buffer100[bytes_received] = '\0'; // Null-terminate the received string
    printf("Received message from storage server: %s\n", buffer100);
    if (strcmp(buffer100, "REGISTER") == 0) {
        StorageServerInfo ss_info;
        read(sock, &ss_info, sizeof(ss_info));
        int mode = 1;
        // Store storage server info
        
        for (int i = 0; i < server_count; i++) {
            printf("Checking storage server: IP: %s, NM Port: %d, Client Port: %d, SS Port: %d, Active: %d\n", storage_servers[i].ip, storage_servers[i].port_nm, storage_servers[i].port_client, storage_servers[i].ss_port, storage_servers[i].is_active); // debug line
            printf("Incoming storage server: IP: %s, NM Port: %d, Client Port: %d, SS Port: %d\n", ss_info.ip, ss_info.port_nm, ss_info.port_client, ss_info.ss_port); // debug line
            if (storage_servers[i].is_active == 0 &&
                strcmp(storage_servers[i].ip, ss_info.ip) == 0 &&
                storage_servers[i].port_nm == ss_info.port_nm &&
                storage_servers[i].port_client == ss_info.port_client &&
                storage_servers[i].ss_port == ss_info.ss_port) {
                storage_servers[i].is_active = 1;
                printf("Storage Server Reconnected:\n");
                replay_commands(storage_servers[i].ip, storage_servers[i].port_client);
                storage_servers[i].command_count = 0;
                mode = 0;
                break;
            }
        }
        if(mode == 1){
            if (server_count < MAX_CONNECTIONS) {
                storage_servers[server_count++] = ss_info;
                printf("Storage Server Registered:\n");
                printf("IP: %s\n", ss_info.ip);
                printf("NM Port: %d\n", ss_info.port_nm);
                printf("Client Port: %d\n", ss_info.port_client);
                printf("Accessible Paths: %s\n", ss_info.accessible_paths);
                printf ("Storage Server Port: %d\n", ss_info.ss_port);
                storage_servers[server_count-1].is_active=1;
                strcpy(storage_servers[server_count - 1].backup_ip1, "");
                storage_servers[server_count-1].backup_port1=0;
                strcpy(storage_servers[server_count - 1].backup_ip2, "");
                storage_servers[server_count-1].backup_port2=0;
                storage_servers[server_count-1].command_count=0;
                printf("backup ports and ips are : %s %d %s %d\n",storage_servers[server_count-1].backup_ip1,storage_servers[server_count-1].backup_port1,storage_servers[server_count-1].backup_ip2,storage_servers[server_count-1].backup_port2);
                char *token = strtok(ss_info.accessible_paths, ",");
                while (token != NULL) {
                    //update_file_info(files, &filecount, &ss_info, token);
                    update_file_info(file_trie, &ss_info, token);
                    token = strtok(NULL, ",");
                }
            } else {
                printf("Maximum storage servers reached.\n");
        }
        }
         if(mode ==1)
        {
                if(server_count==3)
        {
            printf("Triggering backup for 1 and 2 also \n");
            char src_path[BUFFER_SIZE];
            char *src_folder = strtok(storage_servers[0].accessible_paths, ",");
            printf("%s   hahah \n",src_folder);
              StorageServerInfo *dest_server1 = NULL;
    StorageServerInfo *dest_server2 = NULL;
    if(storage_servers[1].is_active)
{dest_server1 = &storage_servers[1];}
    if(storage_servers[2].is_active)
{dest_server2 = &storage_servers[2];}
   
    if(dest_server1)
    {   strcpy(storage_servers[0].backup_ip1,dest_server1->ip);
        storage_servers[0].backup_port1=dest_server1->port_client;
        printf("Backing up to %s %d\n",storage_servers[0].backup_ip1,storage_servers[0].backup_port1);
        // printf("Replicating to Server 1 (IP: %s, Port: %d)\n", 
        //        dest_server1->ip, dest_server1->ss_port);
        char arg1[2000];
        strcpy(arg1,src_folder);
        char dest_path[BUFFER_SIZE];
            char *dest_folder = strtok(dest_server1->accessible_paths, ",");
            char arg2[2000];
        strcpy(arg2,dest_folder);
        printf("%s %s\n",arg1,arg2);
        backup(arg1,arg2);
    }
    if(dest_server2)
    {
       strcpy(storage_servers[0].backup_ip2,dest_server2->ip);
        storage_servers[0].backup_port2=dest_server2->port_client;
        printf("Backing up to %s %d\n",storage_servers[0].backup_ip2,storage_servers[0].backup_port2);
        char arg1[2000];
        strcpy(arg1,src_folder);
        char dest_path[BUFFER_SIZE];
            char *dest_folder = strtok(dest_server2->accessible_paths, ",");
            char arg2[2000];
        strcpy(arg2,dest_folder);
        printf("%s %s\n",arg1,arg2);
        backup(arg1,arg2);
    }
    else{
        printf("no dest server found");
    }
     char src_path2[BUFFER_SIZE];
            char *src_folder2 = strtok(storage_servers[1].accessible_paths, ",");
            printf("%s   hahah \n",src_folder2);
              StorageServerInfo *dest_server3 = NULL;
    StorageServerInfo *dest_server4 = NULL;
    if(storage_servers[0].is_active)
{dest_server3 = &storage_servers[0];}
    if(storage_servers[2].is_active)
{dest_server4 = &storage_servers[2];}
   
    if(dest_server3)
    {   strcpy(storage_servers[1].backup_ip1,dest_server3->ip);
        storage_servers[1].backup_port1=dest_server3->port_client;
        printf("Backing up to %s %d\n",storage_servers[1].backup_ip1,storage_servers[1].backup_port1);
        // printf("Replicating to Server 1 (IP: %s, Port: %d)\n", 
        //        dest_server1->ip, dest_server1->ss_port);
        char arg1[2000];
        strcpy(arg1,src_folder2);
        char dest_path[BUFFER_SIZE];
            char *dest_folder = strtok(dest_server3->accessible_paths, ",");
            char arg2[2000];
        strcpy(arg2,dest_folder);
        printf("%s %s\n",arg1,arg2);
        backup(arg1,arg2);
    }
    if(dest_server4)
    {
       strcpy(storage_servers[1].backup_ip2,dest_server4->ip);
        storage_servers[1].backup_port2=dest_server4->port_client;
        printf("Backing up to %s %d\n",storage_servers[1].backup_ip2,storage_servers[1].backup_port2);
        char arg1[2000];
        strcpy(arg1,src_folder2);
        char dest_path[BUFFER_SIZE];
            char *dest_folder = strtok(dest_server4->accessible_paths, ",");
            char arg2[2000];
        strcpy(arg2,dest_folder);
        printf("%s %s\n",arg1,arg2);
        backup(arg1,arg2);
    }
    else{
        printf("no dest server found");
    }
        }
         if(server_count>=3)
        {
            printf("Triggering backup\n");
            char src_path[BUFFER_SIZE];
            char *src_folder = strtok(storage_servers[server_count-1].accessible_paths, ",");
            printf("%s   hahah \n",src_folder);
              StorageServerInfo *dest_server1 = NULL;
    StorageServerInfo *dest_server2 = NULL;

    for (int i = server_count - 2; i >= 0; i--) {
        // printf("%d active or not \n",storage_servers[i].is_active);
        if (storage_servers[i].is_active) {
            if (!dest_server1) {
                dest_server1 = &storage_servers[i];
            } else if (!dest_server2) {
                dest_server2 = &storage_servers[i];
                break;
            }
        }
    }
    if(dest_server1)
    {   strcpy(storage_servers[server_count-1].backup_ip1,dest_server1->ip);
        storage_servers[server_count-1].backup_port1=dest_server1->port_client;
        printf("Backing up to %s %d\n",storage_servers[server_count-1].backup_ip1,storage_servers[server_count-1].backup_port1);
        // printf("Replicating to Server 1 (IP: %s, Port: %d)\n", 
        //        dest_server1->ip, dest_server1->ss_port);
        char arg1[2000];
        strcpy(arg1,src_folder);
        char dest_path[BUFFER_SIZE];
            char *dest_folder = strtok(dest_server1->accessible_paths, ",");
            char arg2[2000];
        strcpy(arg2,dest_folder);
        printf("%s %s\n",arg1,arg2);
        backup(arg1,arg2);
    }
    if(dest_server2)
    {
       strcpy(storage_servers[server_count-1].backup_ip2,dest_server2->ip);
        storage_servers[server_count-1].backup_port2=dest_server2->port_client;
        printf("Backing up to %s %d\n",storage_servers[server_count-1].backup_ip2,storage_servers[server_count-1].backup_port2);
        char arg1[2000];
        strcpy(arg1,src_folder);
        char dest_path[BUFFER_SIZE];
            char *dest_folder = strtok(dest_server2->accessible_paths, ",");
            char arg2[2000];
        strcpy(arg2,dest_folder);
        printf("%s %s\n",arg1,arg2);
        backup(arg1,arg2);
    }
    else{
        printf("no dest server found");
    }
            // strcp
        }
        }
        char initial_message[BUFFER_SIZE] = "STORAGE_SERVER";
        printf("Sending initial message to storage server%ld\n",strlen(initial_message));
        send(sock, initial_message, strlen(initial_message), 0);
        char heartbeat[BUFFER_SIZE];
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);

        while (1) {
            int bytes_received = recv(sock, heartbeat, sizeof(heartbeat) - 1, 0);
            if (bytes_received <= 0) {
                printf("Heartbeat not received. Breaking the loop.\n");
                break;
            }
            heartbeat[bytes_received] = '\0';
            //printf("Heartbeat received: %s\n", heartbeat);
        }
              
        for (int i = 0; i < server_count; i++) {
printf("%s %d %d %d\n",storage_servers[i].ip,storage_servers[i].port_nm,storage_servers[i].port_client,storage_servers[i].ss_port);
    if (strcmp(storage_servers[i].ip, ss_info.ip) == 0 &&
        storage_servers[i].port_nm == ss_info.port_nm &&
        storage_servers[i].port_client == ss_info.port_client &&
        storage_servers[i].ss_port == ss_info.ss_port) 
    {
        storage_servers[i].is_active = 0;  // Mark server as inactive
        printf("Marked storage server %s:%d as inactive.\n", storage_servers[i].ip, storage_servers[i].port_client);
        break;
    }
        }
        ss_info.is_active=0;
        for (int i = 0; i < write_client_count; i++) {
            if (write_clients[i].active && strcmp(write_clients[i].ss_ip, ss_info.ip) == 0 && write_clients[i].ss_port == ss_info.port_client) {
                write_clients[i].status = 2;
            }
        }
        printf("storage server disconnected\n");
        close(sock);
        
    }
    else
    {
        if (strncmp(buffer100, "write", 5) == 0) {
            char write_command[BUFFER_SIZE];
            char arg1[BUFFER_SIZE];
            sscanf(buffer100, "%s %s", write_command, arg1);

            for (int i = 0; i < write_client_count; i++) {
                if (strcmp(write_clients[i].file_path, arg1) == 0 && write_clients[i].active) {
                    write_clients[i].status = 1;
                    break;
                }
            }
            
        }
         else
        {
            char write_command[BUFFER_SIZE];
            char arg1[BUFFER_SIZE];
            char ss_ip[INET_ADDRSTRLEN];
            char file_path[BUFFER_SIZE];
            int ss_port=0;
            printf("Invalid message received from storage server THIS IS THE BUFFER:\n%s\n",buffer100);
            char additional_message[BUFFER_SIZE];
            // int additional_bytes_received = recv(sock, additional_message, sizeof(additional_message) - 1, 0);
            // if (additional_bytes_received > 0) {
            //     additional_message[additional_bytes_received] = '\0';
            //     printf("Additional message received from storage server: %s\n", additional_message);
            // } else {
            //     perror("Failed to receive additional message from storage server");
            // }
            char *token = strtok(buffer100, " ");
            if (token != NULL) {
                strcpy(ss_ip, token);
                token = strtok(NULL, " ");
            }
            if (token != NULL) {
                ss_port = atoi(token);
                token = strtok(NULL, " ");
            }
            if (token != NULL) {
                strcpy(file_path, token);
            }
            //sscanf(buffer100, "%s %d %s", ss_ip, &ss_port, file_path);

            printf("File write operation completed. Storage Server IP: %s, Port: %d %s\n", ss_ip, ss_port, file_path);

            /*
            Invalid message received from storage server THIS IS THE BUFFER:
            File write operation completed. Storage Server IP: 10.2.128.164, Port: 20021 ./test/temp5/test/temp5/temp6/test/inner/temp.txt
            */
            int backup1_nm_port = -1;
            int backup2_nm_port = -1;
            char backupip1[INET_ADDRSTRLEN] = {0};
            char backupip2[INET_ADDRSTRLEN] = {0};
            for (int i = 0; i < server_count; i++) {
                printf("Comparing: %s %s %d %d %d %d\n", storage_servers[i].ip, ss_ip, storage_servers[i].port_nm, ss_port, storage_servers[i].port_client, storage_servers[i].ss_port);
                if (strcmp(storage_servers[i].ip, ss_ip) == 0 && storage_servers[i].port_nm == ss_port) {
                    printf("EVERYTHING IS EQUAL WHAT IS THE ISSUE ???????");
                    strcpy(backupip1, storage_servers[i].backup_ip1);
                    backup1_nm_port = storage_servers[i].backup_port1;
                    strcpy(backupip2, storage_servers[i].backup_ip2);
                    backup2_nm_port = storage_servers[i].backup_port2;
                    break;
                }
            }
            printf("Backup 1 IP: %s, Port: %d\n", backupip1, backup1_nm_port);
            printf("Backup 2 IP: %s, Port: %d\n", backupip2, backup2_nm_port);
            char data[4096];
            int bytes_received = recv(sock, data, sizeof(data) - 1, 0);
            if (bytes_received > 0) {
                data[bytes_received] = '\0';
                printf("Data received from storage server: %s\n", data);
            } else {
                perror("Failed to receive data from storage server");
            }
            printf("Invalid message received from storage server\n");
            for (int i = 0; i < server_count; i++) {
                if (storage_servers[i].is_active) {
                    printf("Comparing: %s %s %d %d\n", storage_servers[i].ip, backupip1, storage_servers[i].port_client, backup1_nm_port);
                    printf("Comparing: %s %s %d %d\n", storage_servers[i].ip, backupip2, storage_servers[i].port_client, backup2_nm_port);
                    if ((strcmp(storage_servers[i].ip, backupip1) == 0 && storage_servers[i].port_client == backup1_nm_port) ||
                        (strcmp(storage_servers[i].ip, backupip2) == 0 && storage_servers[i].port_client == backup2_nm_port)) {
                        printf("Active server matches with backup: IP: %s, Port: %d\n", storage_servers[i].ip, storage_servers[i].port_client);
                        int backup_sock;
                        struct sockaddr_in backup_addr;
                        sleep(1);
                        if ((backup_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                            perror("Socket creation error");
                            continue;
                        }

                        backup_addr.sin_family = AF_INET;
                        backup_addr.sin_port = htons(storage_servers[i].port_nm);

                        if (inet_pton(AF_INET, storage_servers[i].ip, &backup_addr.sin_addr) <= 0) {
                            perror("Invalid address/ Address not supported");
                            close(backup_sock);
                            continue;
                        }

                        if (connect(backup_sock, (struct sockaddr *)&backup_addr, sizeof(backup_addr)) < 0) {
                            perror("Connection to backup storage server failed");
                            close(backup_sock);
                            continue;
                        }

                        
                        char backup_command[5000];
                        snprintf(backup_command, sizeof(backup_command), "Backup %s", file_path);
                        send(backup_sock, backup_command, strlen(backup_command), 0);
                        sleep(1);
                        send(backup_sock, data, strlen(data), 0);
                        close(backup_sock);
                    }
                }
            }
        }
        close(sock);
    }
}

// Function to initialize the Naming Server
int start_naming_server(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Configure address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections on all interfaces
    address.sin_port = htons(port);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Naming Server initialized on port %d\n", port);
    return server_fd;
}

// Function to accept storage server connections
void start_accepting_storage_servers(int storage_server_socket_fd) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    printf("Naming Server is now accepting storage servers...\n");

    while (1) {
        int new_socket = accept(storage_server_socket_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        int *new_sock = malloc(sizeof(int));
        *new_sock = new_socket;

        if (pthread_create(&thread_id, NULL, handle_storage_server, (void *)new_sock) < 0) {
            perror("Could not create thread");
            free(new_sock);
        }

        pthread_detach(thread_id);
    }
}

// Thread function to handle individual client connections
void *handle_client(void *arg) {
    ClientConnection *client_conn = (ClientConnection *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // Acknowledge connection
    printf("Client connected from IP: %s, Port: %d\n",
           inet_ntoa(client_conn->client_addr.sin_addr),
           ntohs(client_conn->client_addr.sin_port));

    // Interact with client
    while ((bytes_read = read(client_conn->socket_fd, buffer, sizeof(buffer))) > 0) {
        buffer[bytes_read] = '\0'; // Null-terminate the buffer
        printf("Received message from client: %s\n", buffer);
        process_client_command(client_conn, buffer);
    }

    // Close the connection when done
    close(client_conn->socket_fd);
    free(client_conn);
    printf("Client disconnected.\n");

    return NULL;
}

// Function to initialize and start accepting client connections
void start_accepting_clients(int client_socket_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t client_thread;

    printf("Naming Server is now accepting clients...\n");

    // Infinite loop to accept multiple clients
    while (1) {
        ClientConnection *client_conn = (ClientConnection *)malloc(sizeof(ClientConnection));
        if (!client_conn) {
            perror("Failed to allocate memory for client connection");
            exit(EXIT_FAILURE);
        }

        // Accept a new client connection
        client_conn->socket_fd = accept(client_socket_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_conn->socket_fd < 0) {
            perror("Failed to accept client connection");
            free(client_conn);
            continue;
        }

        // Store the client address
        client_conn->client_addr = client_addr;

        // Create a new thread to handle the client
        if (pthread_create(&client_thread, NULL, handle_client, (void *)client_conn) != 0) {
            perror("Failed to create thread for client");
            free(client_conn);
            continue;
        }

        // Detach the thread so it can clean up after itself
        pthread_detach(client_thread);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <client_port> <storage_server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int client_port = atoi(argv[1]);
    int storage_server_port = atoi(argv[2]);

    // Initialize the Naming Server sockets
    int client_socket_fd = start_naming_server(client_port);
    int storage_server_socket_fd = start_naming_server(storage_server_port);

    // Initialize the trie for file information
    file_trie = create_trie();

    // Start accepting clients and storage servers
    pthread_t client_thread, storage_server_thread;

    if (pthread_create(&client_thread, NULL, (void *)start_accepting_clients, (void *)(intptr_t)client_socket_fd) != 0) {
        perror("Failed to create thread for accepting clients");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&storage_server_thread, NULL, (void *)start_accepting_storage_servers, (void *)(intptr_t)storage_server_socket_fd) != 0) {
        perror("Failed to create thread for accepting storage servers");
        exit(EXIT_FAILURE);
    }

    // Wait for threads to finish (they won't in this infinite loop scenario)
    pthread_join(client_thread, NULL);
    pthread_join(storage_server_thread, NULL);

    // Close server sockets after use
    close(client_socket_fd);
    close(storage_server_socket_fd);

    return 0;
}