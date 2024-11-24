#include "storage_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "file_manager.c"
// #include "../common/utils.h"
#include <signal.h>

// Define the ThreadArgs structure
typedef struct {
    int socket;
} ThreadArgs;

int ac=0;

#define MAX_PATHS 4026
char global_nm_ip[INET_ADDRSTRLEN];
int global_nm_port;

// Array to store accessible file paths
char accessible_paths[MAX_PATHS][BUFFER_SIZE];
char CHARBUF[BUFFER_SIZE];
char global_dir_path[BUFFER_SIZE];
int num_paths = 0;

// Function declarations
void handle_create(int nm_sock, char *path1, char *path2);
void handle_delete(int client_sock, const char *path);
void handle_copy(int client_sock, const char *source_path, const char *dest_path);
void process_nm_request(int nm_sock);
void scan_directory(const char *homedir);
void* handle_ss_request(void* args);

// Function to get the local IP address
char* get_local_ip_address() {
    static char ip_address[INET_ADDRSTRLEN];
    int sock;
    struct sockaddr_in local_address;
    socklen_t address_length = sizeof(local_address);

    // Create a socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return NULL;
    }

    // Connect to an external IP address (doesn't actually send data)
    const char* google_dns = "8.8.8.8";  // Google's public DNS IP
    int port = 80;
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = inet_addr(google_dns);
    local_address.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&local_address, sizeof(local_address)) == -1) {
        perror("Connection failed");
        close(sock);
        return NULL;
    }

    // Get the local IP address
    if (getsockname(sock, (struct sockaddr*)&local_address, &address_length) == -1) {
        perror("getsockname failed");
        close(sock);
        return NULL;
    }

    // Convert IP to readable format
    inet_ntop(AF_INET, &local_address.sin_addr, ip_address, sizeof(ip_address));

    close(sock);
    return ip_address;
}

// Function to scan the directory and populate accessible_paths array
void scan_directory(const char *homedir) {

    strncpy(accessible_paths[num_paths], homedir, BUFFER_SIZE);
    num_paths++;

    struct dirent *entry;
    DIR *dp = opendir(homedir);

    if (dp == NULL) {
        perror("opendir");
         return ;

    }

    while ((entry = readdir(dp))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[BUFFER_SIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", homedir, entry->d_name);

        struct stat path_stat;
        if (stat(full_path, &path_stat) != 0) {
            perror("stat failed");
            closedir(dp);
            return ;
        }

        if (S_ISDIR(path_stat.st_mode)) {
            // Recursively scan subdirectory
            strncpy(accessible_paths[num_paths], full_path, BUFFER_SIZE);
            num_paths++;
            scan_directory(full_path);
        } else if (S_ISREG(path_stat.st_mode)) {
            if (num_paths < MAX_PATHS) {
                strncpy(accessible_paths[num_paths], full_path, BUFFER_SIZE);
                num_paths++;
            } else {
                fprintf(stderr, "Maximum number of paths reached!\n");
                break;
            }
        }
    }

    closedir(dp);
}

// Start the storage server and register with Naming Server
void start_storage_server(void* params) {
    ServerParams *server_params = (ServerParams *)params;
    const char *nm_ip = server_params->nm_ip;
    int nm_port = server_params->nm_port;
    int client_port = server_params->client_port;
    int nm_port_to_recieve = server_params->nm_port_to_recieve;
    const char *homedir = server_params->homedir;
    int nm_sock;
    struct sockaddr_in serv_addr;
// Allocate memory for ss_info
    StorageServerInfo ss_info;
    memset(&ss_info, 0, sizeof(StorageServerInfo));
    printf("Received parameters:\n");
    printf("Naming Server IP: %s\n", nm_ip);
    printf("Naming Server Port: %d\n", nm_port);
    printf("Client Port: %d\n", client_port);
    printf("Naming Server Port to Receive: %d\n", nm_port_to_recieve);
    printf("Home Directory: %s\n", homedir);
    strncpy(ss_info.ip, server_params->ip, INET_ADDRSTRLEN);
    ss_info.port_nm = nm_port_to_recieve;
    ss_info.port_client = client_port;
    ss_info.ss_port = server_params->ss_port;
    // Scan the homedir directory and populate accessible_paths array
    scan_directory(homedir);
    //printf("Scanned directory\n");

    // Create comma-separated list of accessible paths
    printf("ALLOCATED paths_list[]\n");
    char paths_list[BUFFER_SIZE] = "";
    
    for (int i = 0; i < num_paths; i++) {
        strcat(paths_list, accessible_paths[i]);
        if (i < num_paths - 1) {
            strcat(paths_list, ",");
        }
    }
    strncpy(ss_info.accessible_paths, paths_list, BUFFER_SIZE);

    // Create socket to connect to the Naming Server
    if ((nm_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(nm_port);

    if (inet_pton(AF_INET, nm_ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return;
    }

    if (connect(nm_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection to Naming Server failed");
        return;
    }
    printf("numpaths: %d\n", num_paths);
    // Send storage server information to the Naming Server
    printf("%d client port\n", client_port);
    const char register_message[BUFFER_SIZE] = "REGISTER";
    send(nm_sock, register_message, strlen(register_message), 0);
    sleep(1);
    send(nm_sock, &ss_info, sizeof(ss_info), 0);
    printf("Storage Server registered with Naming Server at %s:%d\n", nm_ip, nm_port);
    char buffer[BUFFER_SIZE];
    read(nm_sock, &buffer, sizeof(buffer)); // size of charbuf
    printf("\n%s\n", buffer);
    // Process requests from the Naming Server in a loop
    while (1) {
        // process_nm_request(nm_sock);
        // Send a heartbeat message to the Naming Server every 5 seconds
        const char heartbeat_message[BUFFER_SIZE] = "HEARTBEAT";
        send(nm_sock, heartbeat_message, strlen(heartbeat_message), 0);
        sleep(5);
    }
    const char stop_message[BUFFER_SIZE] = "STOP";
    send(nm_sock, stop_message, strlen(stop_message), 0);
    close(nm_sock);
}

// Thread function to handle requests from the Naming Server
void* handle_nm_request(void* args) {
    ThreadArgs* thread_args = (ThreadArgs*)args;
    int new_socket = thread_args->socket;
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE], arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];

    int bytes_received = recv(new_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        perror("Failed to receive data from Naming Server");
        close(new_socket);
        free(thread_args);
        return NULL;
    }

    buffer[bytes_received] = '\0';
    printf("Received message from Naming Server: %s\n", buffer);
    sscanf(buffer, "%s %s %s", command, arg1, arg2);

    if (strcmp(command, "CREATE") == 0) {
        create_file(arg1, arg2, new_socket);
    } else if (strcmp(command, "DELETE") == 0) {
        delete_file_or_directory(arg1, new_socket);
    } else if (strcmp(command, "COPY") == 0) {
        handle_copy(new_socket, arg1, arg2);
        } 
    else if (strncmp(command, "Backup", 6) == 0) {
        // Handle Backup command
        // Add your backup handling code here
        char backup_data[4096];
    char backup_path[4096];
    sscanf(command, "Backup %s", backup_path);
    printf("Received Backup command with path: %s\n", command+7);
    int bytes_received = recv(new_socket, backup_data, sizeof(backup_data), 0);
    if (bytes_received <= 0) {
        perror("Failed to receive backup data from Naming Server");
        close(new_socket);
        free(thread_args);
        return NULL;
    }
    backup_data[bytes_received] = '\0';
    printf("Received backup data: %s\n", backup_data);
    // Add your backup handling code here
    char filepath[5000];
    printf("Received Backup command with path: %s\n", arg1);
    snprintf(filepath, sizeof(filepath), "%s/%s", global_dir_path, arg1);
    printf("Saving backup data to: %s\n", filepath);
    FILE *backup_file = fopen(filepath, "a");

    if (!backup_file) {
        perror("Failed to open backup file");
        close(new_socket);
        free(thread_args);
        return NULL;
    }

    fwrite(backup_data, 1, bytes_received, backup_file);
    fclose(backup_file);
    printf("Backup data saved to: %s\n", filepath);
    // Read the backup file and print its content
    } else {
        const char *response = "ERROR: Invalid command from Naming Server\n";
        send(new_socket, response, strlen(response), 0);
    }

    close(new_socket);
    free(thread_args);
    return NULL;
}

// Process requests from the Naming Server
void process_nm_request(int nm_port) {
    int nm_sock;
    struct sockaddr_in serv_addr;

    // Create socket to listen for requests from the Naming Server
    if ((nm_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(nm_port);

    if (bind(nm_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        close(nm_sock);
        return;
    }

    if (listen(nm_sock, 3) < 0) {
        perror("Listen failed");
        close(nm_sock);
        return;
    }

    printf("Listening for requests from Naming Server on port %d...\n", nm_port);

    while (1) {
        int new_socket;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        if ((new_socket = accept(nm_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Create a new thread to handle the request
        pthread_t thread_id;
        ThreadArgs* thread_args = malloc(sizeof(ThreadArgs));
        thread_args->socket = new_socket;

        if (pthread_create(&thread_id, NULL, handle_nm_request, thread_args) != 0) {
            perror("Failed to create thread for Naming Server request");
            close(new_socket);
            free(thread_args);
        }

        // Detach the thread to allow it to clean up after itself
        pthread_detach(thread_id);
    }

    close(nm_sock);
}

// Handle "CREATE" command to create a file or directory
void handle_create(int nm_sock, char *path1, char *path2) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", path1, path2);
    printf("Creating file: %s\n", full_path);
    int result;

    int file = open(full_path, O_CREAT | O_WRONLY, 0666);
    if (file >= 0) {
        close(file);
        result = 0;
    } else {
        result = -1;
    }

    if (result == 0) {
        const char *response = "CREATE: Success\n";
        send(nm_sock, response, strlen(response), 0);
    } else {
        const char *response = "CREATE: Failed\n";
        send(nm_sock, response, strlen(response), 0);
    }
}

// Handle "DELETE" command to delete a file or directory
void handle_delete(int nm_sock, const char *path) {
    int result;
    struct stat path_stat;

    if (stat(path, &path_stat) != 0) {
        result = -1; // Path does not exist
    } else if (S_ISDIR(path_stat.st_mode)) {
        result = rmdir(path); // Remove directory
    } else {
        result = unlink(path); // Remove file
    }

    if (result == 0) {
        const char *response = "DELETE: Success\n";
        send(nm_sock, response, strlen(response), 0);
    } else {
        const char *response = "DELETE: Failed\n";
        send(nm_sock, response, strlen(response), 0);
    }
}
void getFileName(const char* path, char* fileName) {
    // Find the last occurrence of the '/' character
    const char* lastSlash = strrchr(path, '/');
    
    // If a '/' is found, copy the part after it into fileName
    if (lastSlash != NULL) {
        strcpy(fileName, lastSlash + 1);  // Copy after the last slash
    } else {
        strcpy(fileName, path);  // If no '/' is found, copy the entire path (no directory part)
    }
}
void split_path(const char *path, char *parent, char *last) {
    // Find the last occurrence of '/'
    const char *last_slash = strrchr(path, '/');
    
    if (last_slash == NULL || last_slash == path) {
        // Handle case where there is no '/' or the path starts with '/'
        strcpy(parent, "");
        strcpy(last, path[0] == '/' ? path + 1 : path);
    } else {
        // Separate the parent and last folder
        size_t parent_len = last_slash - path; // Length of parent path
        strncpy(parent, path, parent_len);
        parent[parent_len] = '\0'; // Null-terminate the parent path
        strcpy(last, last_slash + 1); // Copy the last folder name
    }
}
int copy_from_folder(const char* file_path, int client_socket ) {
    // printf("Entering function with path: %s\n", file_path);
    //char* token = strtok(file_path, " ");
    //char* args[10];
    char path[100];
    char ip[100]; char parent[100], last[100];
    int port;
    char dest_path[100];
    int flag;
    sscanf(file_path,"%s %s %d %s %d ",path,ip,&port,dest_path,&flag);
    split_path(path, parent, last);
    int flag2=0;
    if (parent[0] == '\0') {
    flag2=1;
} 

    printf("here is the result : %s %s %d %s %d %s %s\n",path,ip,port,dest_path,flag,parent,last);

    char command [BUFFER_SIZE];
    if(flag)
    {
        snprintf(command, sizeof(command), "cp -r %s/* %s",path,dest_path );
        int result = system(command);
        if (result != 0) {
            perror("Failed to create directory");
            return FAIL;
        }
        return SUCCESS;
    }
    else{

        // printf("flag not set");
         char command2 [BUFFER_SIZE];
          char original_dir[BUFFER_SIZE];

    // Save the current working directory
    if (getcwd(original_dir, sizeof(original_dir)) == NULL) {
        perror("Failed to get current directory");
        return FAIL;
    }
         if(flag2==1)
   {
    //  printf("ek baar to dhang se chal jao\n");
   snprintf(command2, sizeof(command2), "zip -r %s.zip %s", path, path);}
else{  
    if (chdir(parent) != 0) {
            perror("Failed to change directory");
            return FAIL;
        }
    snprintf(command2, sizeof(command2), "zip -r %s.zip %s" ,last, last);
     
}

    int result = system(command2);
    if (result != 0) {
        perror("Failed to zip folder");
        return FAIL;
    }
    if (chdir(original_dir) != 0) {
        perror("Failed to return to the original directory");
    }
    char fileName[100];  // Buffer to store the extracted file name
    
    getFileName(path, fileName); 
   
    char file_to_send[BUFFER_SIZE];
    snprintf(file_to_send,sizeof(file_to_send),"%s.zip",path);
     printf("%s\n",file_to_send); 
    int fd = open(file_to_send, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return FAIL;
    }

    // Get the size of the file
    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        perror("Failed to get file stats");
        close(fd);
        return FAIL;
    }

    size_t file_size = file_stat.st_size;
    printf("Zip file size: %zu bytes\n", file_size);

    // Allocate memory for the file contents
    char *file_contents = (char *)malloc(file_size);
    if (!file_contents) {
        perror("Memory allocation failed");
        close(fd);
        return FAIL;
    }

    // Read the file into the array
    ssize_t bytes_read = read(fd, file_contents, file_size);
    if (bytes_read < 0) {
        perror("Failed to read file");
        free(file_contents);
        close(fd);
        return FAIL;
    }

    printf("Read %zd bytes from the zip file.\n", bytes_read);
    close(fd);
    
    int dest_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in source_addr;
    source_addr.sin_family = AF_INET;
    source_addr.sin_port = htons(port);
    inet_pton(AF_INET,ip, &source_addr.sin_addr);
    if (connect(dest_socket, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
        perror("Error connecting to source server");
        return FAIL;
    }
char file_to_send2[BUFFER_SIZE];
    snprintf(file_to_send2,sizeof(file_to_send2),"%s/%s.zip",dest_path,fileName);
char request2[5000];
snprintf(request2, sizeof(request2), "PASTE_FOLDER %s", file_to_send2);
send(dest_socket, request2, strlen(request2), 0);
sleep(2);
ssize_t bytes_sent2 = send(dest_socket,&file_size, sizeof(file_size), 0);
if (bytes_sent2 < 0) {
    perror("Failed to send file content");
    free(file_contents);
    close(dest_socket);
    return FAIL;
}
sleep(0.5);
// Send the file content in binary
ssize_t bytes_sent = send(dest_socket, file_contents, file_size, 0);
if (bytes_sent < 0) {
    perror("Failed to send file content");
    free(file_contents);
    close(dest_socket);
    return FAIL;
}

printf("Sent %zd bytes of file content.\n", bytes_sent);

// Clean up
free(file_contents);
char star[5000];
snprintf(star, sizeof(star), "rm -rf %s", file_to_send);
int z=system(star);
close(dest_socket);
    }
    // return;
printf("file successfully copied");
      char response[1024];
  strcpy(response,"completed copying");
  printf("%s",response);
  send(client_socket,response,strlen(response)+1,0);
    // }
    int i = 0;
char file_path_copy[100];
strncpy(file_path_copy, file_path, sizeof(file_path_copy));
char *token = strtok(file_path_copy, " ");
    while (token != NULL && i < 4) {
        printf("%s\n",token);
        token = strtok(NULL, " ");
    }

}

// Handle "COPY" command to copy a file or directory
void handle_copy(int nm_sock, const char *source_path, const char *dest_path) {
    FILE *src = fopen(source_path, "rb");
    if (!src) {
        const char *response = "COPY: Source file not found\n";
        send(nm_sock, response, strlen(response), 0);
        return;
    }

    FILE *dest = fopen(dest_path, "wb");
    if (!dest) {
        fclose(src);
        const char *response = "COPY: Unable to create destination file\n";
        send(nm_sock, response, strlen(response), 0);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes_read, dest);
    }

    fclose(src);
    fclose(dest);

    const char *response = "COPY: Success\n";
    send(nm_sock, response, strlen(response), 0);
}
void* handle_client_request(void* socket_ptr) {
    int client_socket = *(int*)socket_ptr;
    free(socket_ptr);  // Free the dynamically allocated memory for the socket descriptor

    char buffer[BUFFER_SIZE];
    int bytes_read;

    // Read the client request from the socket
    bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        perror("Failed to read from client socket");
        close(client_socket);
        return NULL;
    }
    
    // Null-terminate the buffer to safely work with it as a string
    buffer[bytes_read] = '\0';
    printf("Received message from client: %s\n", buffer);
    if (strncmp(buffer, "PASTE_FOLDER", 12) == 0) {
        // Call the function to read a file
    //    printf("bahot hora mera");
    //  printf("idhar aa rha hai?");
        paste_to_folder(buffer + 13, client_socket);
        close(client_socket);
        return NULL;
     }
   
    if (strncmp(buffer, "READ", 4) == 0) {
        // Call the function to read a file
        read_file(buffer + 5, client_socket);
    } else if (strncmp(buffer, "WRITE", 5) == 0) {
        // Call the function to write a file
        printf("is it coming here ?\n");
        char *global_ss_ip = get_local_ip_address();
        write_file(buffer + 6, client_socket,global_nm_ip,global_nm_port,global_ss_ip,ac);
        
        // Check if the write command has --sync at the end
        if (strstr(buffer, "--sync") == NULL) {
            // Send write command to storage server
            int nm_sock;
            struct sockaddr_in serv_addr;

            if ((nm_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation error");
            close(client_socket);
            return NULL;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(global_nm_port);

            if (inet_pton(AF_INET, global_nm_ip, &serv_addr.sin_addr) <= 0) {
            perror("Invalid address/ Address not supported");
            close(nm_sock);
            close(client_socket);
            return NULL;
            }

            if (connect(nm_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection to Storage Server failed");
            close(nm_sock);
            close(client_socket);
            return NULL;
            }

            char write_command[BUFFER_SIZE];
            snprintf(write_command, sizeof(write_command), "write %s", buffer + 6);
            sleep(2);
            send(nm_sock, write_command, strlen(write_command), 0);
            close(nm_sock);
        
        }
    } else if (strncmp(buffer, "INFO", 4) == 0) {
        // Call the function to get file info
        get_file_info(buffer + 5, client_socket);
    } else if (strncmp(buffer, "STREAM", 6) == 0) {
        // Call the function to stream an audio file
        stream_audio_file(buffer + 7, client_socket);
    }
     else if (strncmp(buffer, "DELETE",6) == 0) {
        delete_file_or_directory(buffer+7, client_socket);
    } 
    else if (strncmp(buffer, "CREATE", 6) == 0) {
      char file[1000];        // Call the function to create a file or directorychar file[1000];
        char folder [1000];
        sscanf(buffer+7,"%s %s",folder,file);
        create_file(folder, file, client_socket);
    } else if (strncmp(buffer, "COPY_FILE", 9) == 0) {
        // Call the function to copy a file
        copy_to_file(buffer + 10, client_socket);
        if (strstr(buffer, "--sync") == NULL) {
            // Send write command to storage server
            int nm_sock;
            struct sockaddr_in serv_addr;

            if ((nm_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation error");
            close(client_socket);
            return NULL;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(global_nm_port);

            if (inet_pton(AF_INET, global_nm_ip, &serv_addr.sin_addr) <= 0) {
            perror("Invalid address/ Address not supported");
            close(nm_sock);
            close(client_socket);
            return NULL;
            }

            if (connect(nm_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection to Storage Server failed");
            close(nm_sock);
            close(client_socket);
            return NULL;
            }

            char write_command[BUFFER_SIZE];
            snprintf(write_command, sizeof(write_command), "write %s", buffer + 6);
            send(nm_sock, write_command, strlen(write_command), 0);
            close(nm_sock);
        }
    } 
    else if(strncmp(buffer,"COPY_FOLDER",11)==0){
        printf("coming here");
        int ecode = copy_from_folder(buffer+12,client_socket);
        if(ecode==0){
            // Notify Naming Server that the copy operation is completed
            int nm_sock;
            struct sockaddr_in serv_addr;

            if ((nm_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("Socket creation error");
                close(client_socket);
                return NULL;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(global_nm_port);

            if (inet_pton(AF_INET, global_nm_ip, &serv_addr.sin_addr) <= 0) {
                perror("Invalid address/ Address not supported");
                close(nm_sock);
                close(client_socket);
                return NULL;
            }

            if (connect(nm_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection to Naming Server failed");
                close(nm_sock);
                close(client_socket);
                return NULL;
            }

            const char *copy_complete_message = "COPY_COMPLETED";
            send(nm_sock, copy_complete_message, strlen(copy_complete_message), 0);
            close(nm_sock);
        }
        else{
            printf("file not copied");
        }
            
            
    }
    else {
        // Send an error response for an unrecognized command
        const char *response = "ERROR: Unknown command\n";
        send(client_socket, response, strlen(response), 0);
    }

    // Close the client socket after handling the request
    close(client_socket);
    return NULL;
}

// Function to handle client connections
void client_connection(int client_port) {
    int server_fd, new_socket;
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
    address.sin_port = htons(client_port);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, MAX_PATHS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Storage Server is now accepting client connections on port %d...\n", client_port);

    while (1) {
    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (new_socket < 0) {
        perror("accept");
        continue;
    }

    pthread_t client_thread;
    int* socket_ptr = malloc(sizeof(int)); // Allocate memory for the socket descriptor
    *socket_ptr = new_socket; // Store the socket in dynamically allocated memory

    if (pthread_create(&client_thread, NULL, handle_client_request, socket_ptr) != 0) {
        perror("Failed to create thread for client request");
        close(new_socket);
        free(socket_ptr);
    }

    pthread_detach(client_thread); // Detach to clean up after completion
}

}
void handle_sigint(int sig) {
    ac = 1;
    sleep(2);
    exit(0); // Stop the program
}
// Thread function to handle requests from other storage servers     

// Process requests from other storage servers
void process_ss_request(int ss_port) {
    int ss_sock;
    struct sockaddr_in serv_addr;

    // Create socket to listen for requests from other storage servers
    if ((ss_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(ss_port);

    if (bind(ss_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        close(ss_sock);
        return;
    }

    if (listen(ss_sock, 3) < 0) {
        perror("Listen failed");
        close(ss_sock);
        return;
    }

    printf("Listening for requests from other storage servers on port %d...\n", ss_port);

    while (1) {
        int new_socket;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        if ((new_socket = accept(ss_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Create a new thread to handle the request
        pthread_t thread_id;
        ThreadArgs* thread_args = malloc(sizeof(ThreadArgs));
        thread_args->socket = new_socket;

        if (pthread_create(&thread_id, NULL, handle_ss_request, thread_args) != 0) {
            perror("Failed to create thread for other storage server request");
            close(new_socket);
            free(thread_args);
        }

        // Detach the thread to allow it to clean up after itself
        pthread_detach(thread_id);
    }

    close(ss_sock);
}
void* handle_ss_request(void* args) {
    ThreadArgs* thread_args = (ThreadArgs*)args;
    int new_socket = thread_args->socket;
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE], arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];

    int bytes_received = recv(new_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        perror("Failed to receive data from other storage server");
        close(new_socket);
        free(thread_args);
        return NULL;
    }

    buffer[bytes_received] = '\0';
    printf("Received message from other storage server: %s\n", buffer);
    sscanf(buffer, "%s %s %s", command, arg1, arg2);

    if (strcmp(command, "COPY") == 0) {
        handle_copy(new_socket, arg1, arg2);
    } else {
        const char *response = "ERROR: Invalid command from other storage server\n";
        send(new_socket, response, strlen(response), 0);
    }

    close(new_socket);
    free(thread_args);
    return NULL;
}


int main(int argc, char const *argv[]) {
    

    
    //signal(SIGINT, handle_sigint);
    if (argc < 7) {
        fprintf(stderr, "Usage: %s <NM_IP> <NM_PORT_to_connect> <CLIENT_PORT> <NM_PORT_TO_RECEIVE>  <ss_port_to_connect_other_ss> <HOMEDIR>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *nm_ip = argv[1];
    int nm_port = atoi(argv[2]);
    int client_port = atoi(argv[3]);
    int nm_port_receive = atoi(argv[4]);
    int ss_port = atoi(argv[5]);
    const char *homedir = argv[6];
    
    strncpy(global_nm_ip, nm_ip, INET_ADDRSTRLEN);
    global_nm_port = nm_port;
    // Get the local IP address
    char *ip = get_local_ip_address();
    ac=nm_port_receive;
    strcpy(global_dir_path,homedir);
    // Create threads for client connection and storage server
    pthread_t client_thread, storage_server_thread, nm_thread;

    ServerParams server_params = {nm_ip, nm_port, client_port, nm_port_receive, ip, homedir,ss_port};

    if (pthread_create(&client_thread, NULL, (void *)client_connection, (void *)(intptr_t)client_port) != 0) {
        perror("Failed to create thread for client connection");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&storage_server_thread, NULL, (void *)start_storage_server, &server_params) != 0) {
        perror("Failed to create thread for storage server");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&nm_thread, NULL, (void *)process_nm_request, (void *)(intptr_t)nm_port_receive) != 0) {
        perror("Failed to create thread for naming server request processing");
        exit(EXIT_FAILURE);
    }
    pthread_t ss_thread;

    if (pthread_create(&ss_thread, NULL, (void *)process_ss_request, (void *)(intptr_t)ss_port) != 0) {
        perror("Failed to create thread for storage server request processing");
        exit(EXIT_FAILURE);
    }

    pthread_join(ss_thread, NULL);

    pthread_join(nm_thread, NULL);
    // Wait for threads to finish (they won't in this infinite loop scenario)
    pthread_join(client_thread, NULL);
    pthread_join(storage_server_thread, NULL);

    return 0;
}