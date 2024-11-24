
#define NAMING_SERVER_H
#define MAX_FILE_PATH_SIZE 256 
#include <arpa/inet.h>

#define MAX_CONNECTIONS 10

#define MAX_CLIENTS 10   // Define a maximum number of clients
#define BUFFER_SIZE 4096  // Define buffer size for data exchange
#define MAX_FILES 64     // Define a maximum number of files 

#define FAIL 1
#define SUCCESS 0

// Data structure for client connection
typedef struct {
    int socket_fd;
    struct sockaddr_in client_addr;
} ClientConnection;

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port_nm;
    int port_client;
    char accessible_paths[BUFFER_SIZE]; 
    int ss_port;
    int is_active;// Paths that are accessible
    char backup_ip1[INET_ADDRSTRLEN];
    int backup_port1;
    char backup_ip2[INET_ADDRSTRLEN];
    int backup_port2;
    char commands[1000][5000]; // Array of commands
    int command_count; 
} StorageServerInfo;

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port_client;
    char file_path[MAX_FILE_PATH_SIZE]; // Simplified for example
} FILEINFO;

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int nm_port;
    //char file_path[MAX_FILE_PATH_SIZE]; // Simplified for example
} NMHASH;

int start_naming_server(int port);
void start_accepting_clients(int server_socket_fd);

