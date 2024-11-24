#include "file_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <pthread.h>
#include<arpa/inet.h>
#include "../common/utils.h"


#define MAX_PATHS 4026
char files_being_accessed[MAX_PATHS][BUFFER_SIZE];
int num_files_being_accessed = 0;
char files_being_read[MAX_PATHS][BUFFER_SIZE];
int num_files_being_read = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;
void add_accessible_path(const char *path) {
    if (num_files_being_accessed < MAX_PATHS) {
        strncpy(files_being_accessed[num_files_being_accessed], path, BUFFER_SIZE);
        num_files_being_accessed++;
    } else {
        fprintf(stderr, "Maximum number of paths reached!\n");
    }
}
void add_accessible_path2(const char *path) {
    if (num_files_being_read < MAX_PATHS) {
        strncpy(files_being_read[num_files_being_read], path, BUFFER_SIZE);
        num_files_being_read++;
    } else {
        fprintf(stderr, "Maximum number of paths reached!\n");
    }
}
// Function to delete a file from accessible paths
void delete_accessible_path(const char *path) {
    for (int i = 0; i < num_files_being_accessed; i++) {
        if (strcmp(files_being_accessed[i], path) == 0) {
            // Shift remaining paths to fill the gap
            for (int j = i; j < num_files_being_accessed - 1; j++) {
                strncpy(files_being_accessed[j], files_being_accessed[j + 1], BUFFER_SIZE);
            }
            num_files_being_accessed--;
            break;
        }
    }
}
void delete_accessible_path2(const char *path) {
    for (int i = 0; i < num_files_being_read; i++) {
        if (strcmp(files_being_read[i], path) == 0) {
            // Shift remaining paths to fill the gap
            for (int j = i; j < num_files_being_read - 1; j++) {
                strncpy(files_being_read[j], files_being_read[j + 1], BUFFER_SIZE);
            }
            num_files_being_read--;
            break;
        }
    }
}
void read_file(const char *file_path, int client_socket) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < num_files_being_accessed; i++) {
        if (strcmp(files_being_accessed[i], file_path) == 0) {
            const char *response = "Another client is writing to this file";
            send(client_socket, response, strlen(response), 0);
            pthread_mutex_unlock(&lock);
            return;
        }
    }
    pthread_mutex_unlock(&lock);
    pthread_mutex_lock(&lock2);
    add_accessible_path2(file_path);
    pthread_mutex_unlock(&lock2);
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
    pthread_mutex_lock(&lock2);
    delete_accessible_path2(file_path);
    pthread_mutex_unlock(&lock2);
    close(fd);
}

void write_file(const char *file_path, int client_socket,char* global_nm_ip,int global_nm_port,char* global_ss_ip,int global_ss_port) {
    pthread_mutex_lock(&lock2);
    for (int i = 0; i < num_files_being_read; i++) {
        if (strcmp(files_being_read[i], file_path) == 0) {
            const char *response = "Another client is reading this file";
            send(client_socket, response, strlen(response), 0);
            pthread_mutex_unlock(&lock2);
            return;
        }
    }
    pthread_mutex_unlock(&lock2);
    pthread_mutex_lock(&lock);
    add_accessible_path(file_path);
    pthread_mutex_unlock(&lock);


    printf("Writing to file: %s\n", file_path);

    char buffer[MAX_WRITE_SIZE];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received < 0) {
        perror("Failed to receive data from client");
        return;
    }
    buffer[bytes_received] = '\0'; // Null-terminate buffer

    char *sync_flag = strstr(file_path, " --sync");
    char new_file_path[BUFFER_SIZE];
    if (sync_flag) {
        size_t path_len = sync_flag - file_path;
        strncpy(new_file_path, file_path, path_len);
        new_file_path[path_len] = '\0';
        file_path = new_file_path;
    }

    FILE *file = fopen(file_path, "a");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    if (!sync_flag) {
        const char *response = "Data received for asynchronous writing";
        send(client_socket, response, strlen(response), 0);

        for (ssize_t i = 0; i < bytes_received; i += CHUNK_SIZE) {
            ssize_t chunk_size = (bytes_received - i) < CHUNK_SIZE ? (bytes_received - i) : CHUNK_SIZE;
            size_t bytes_written = fwrite(buffer + i, 1, chunk_size, file);
            if (bytes_written < chunk_size) {
                perror("Failed to write to file");
                fclose(file);
                return;
            }
            usleep(500000); // Wait 0.5 seconds
        }
    } else {
        

        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written < bytes_received) {
            perror("Failed to write to file");
            fclose(file);
            return;
        }
        const char *response = "synchronous writing done";
        send(client_socket, response, strlen(response), 0);
    }

    fclose(file);
    printf("Data written to file: %s\n", file_path);
    pthread_mutex_lock(&lock);
    delete_accessible_path(file_path);
    pthread_mutex_unlock(&lock);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(global_nm_port);

    if (inet_pton(AF_INET, global_nm_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to main server failed");
        close(sock);
        return;
    }

    // Send a message to the main server
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s %d %s", global_ss_ip, global_ss_port,file_path);
    
    send(sock, message, strlen(message), 0);
    sleep(3);
    send(sock, buffer, bytes_received, 0);


    // Close the socket
    close(sock);


}

void copy_to_file(const char *file_path, int client_socket) {
    printf("Writing to file: %s\n", file_path);

    char buffer[MAX_WRITE_SIZE];
    ssize_t bytes_received = read(client_socket, buffer, sizeof(buffer));
    if (bytes_received < 0) {
        perror("Failed to receive data from client");
        return;
    }
    buffer[bytes_received] = '\0'; // Null-terminate buffer

    char *sync_flag = strstr(file_path, " --sync");
    char new_file_path[BUFFER_SIZE];
    if (sync_flag) {
        size_t path_len = sync_flag - file_path;
        strncpy(new_file_path, file_path, path_len);
        new_file_path[path_len] = '\0';
        file_path = new_file_path;
    }

    FILE *file = fopen(file_path, "w");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    if (!sync_flag) {
        const char *response = "Data received for asynchronous writing";
        send(client_socket, response, strlen(response), 0);

        for (ssize_t i = 0; i < bytes_received; i += CHUNK_SIZE) {
            ssize_t chunk_size = (bytes_received - i) < CHUNK_SIZE ? (bytes_received - i) : CHUNK_SIZE;
            size_t bytes_written = fwrite(buffer + i, 1, chunk_size, file);
            if (bytes_written < chunk_size) {
                perror("Failed to write to file");
                fclose(file);
                return;
            }
            usleep(500000); // Wait 0.5 seconds
        }
    } else {
        const char *response = "Data received for synchronous writing";
        send(client_socket, response, strlen(response), 0);

        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written < bytes_received) {
            perror("Failed to write to file");
            fclose(file);
            return;
        }
    }

    fclose(file);
    printf("Data written to file: %s\n", file_path);
}

const char* extract_path(const char *path) {
    static char result[1024];  // Static so the result persists after function returns

    // Find the last occurrence of '/'
    const char *last_slash = strrchr(path, '/');
    
    if (last_slash != NULL) {
        // Find the last occurrence of '.' (extension delimiter)
        const char *dot = strrchr(path, '.');
        
        // If there's a '.' and it comes after the last '/'
        if (dot != NULL && dot > last_slash) {
            // Calculate the length of the portion we want
            int length = last_slash - path;
            
            // Copy the portion before the last slash to the result string
            strncpy(result, path, length);
            result[length] = '\0';  // Null-terminate the string
        } else {
            // If no extension, just return the portion before the last slash
            strcpy(result, path);
        }
    }

    return result;
}

void paste_to_folder(const char *file_path, int client_socket) {
  
    // printf("coming to the fucntion");
 
  char request11[MAX_WRITE_SIZE];
snprintf(request11, sizeof(request11), "touch \"%s\"", file_path); // Quote the file_path
// printf("%s this is the request\n", request11);
int ret = system(request11);
if (ret == -1) {
    perror("Error executing system command");
    return ;
}

        // return ;
    // printf("Writing to file: %s\n", file_path);
     char *buffer= (char*) malloc(10000);
size_t buffer0;
ssize_t bytes_received4 = read(client_socket, &buffer0, sizeof(buffer0));
if (bytes_received4 < 0) {
        perror("Failed to receive data from client");
        return;
    }
    // printf("%ld",buffer0);
    size_t q=buffer0;
    buffer = (char*) malloc(q);
    ssize_t bytes_received = read(client_socket, buffer, buffer0);
    if (bytes_received < 0) {
        perror("Failed to receive data from client");
        return;
    }
    buffer[bytes_received] = '\0'; // Null-terminate buffer
    // printf("%s",buffer);
     FILE *file = fopen(file_path, "wb");
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }

    // Write the received binary content to the file
    size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
    if (bytes_written < (size_t)bytes_received) {
        perror("Failed to write all data to file");
        fclose(file);
        return;
    }

    printf("Successfully wrote %zu bytes to file: %s\n", bytes_written, file_path);
    fclose(file);
    
    char request12[MAX_WRITE_SIZE];
    char directoryPath[1024];
    char original_path[1024];
    strcpy(original_path,file_path);
    // printf("%s\n",original_path);
    const char *updated_path = extract_path(file_path);
    // printf("%s %s \n",original_path,updated_path);
  snprintf(request12, sizeof(request12), "unzip %s -d %s", original_path,updated_path);
//   printf("%s\n",request12);
 
  int l=system(request12);
  char star[BUFFER_SIZE];
  snprintf(star, sizeof(star), "rm -rf %s", original_path);
  int z=system(star);
//   char response[BUFFER_SIZE];
//   strcpy(response,"completed");
//   printf("%s.response");
//   send(client_socket,response,strlen(response),0);
    return;
   
}

void get_file_info(const char *file_path, int client_socket) {
    struct stat file_stat;
    if (stat(file_path, &file_stat) < 0) {
        perror("Failed to get file info");
        return;
    }

    char permissions[11];
    snprintf(permissions, sizeof(permissions), "%c%c%c%c%c%c%c%c%c%c",
             (S_ISDIR(file_stat.st_mode)) ? 'd' : '-',
             (file_stat.st_mode & S_IRUSR) ? 'r' : '-',
             (file_stat.st_mode & S_IWUSR) ? 'w' : '-',
             (file_stat.st_mode & S_IXUSR) ? 'x' : '-',
             (file_stat.st_mode & S_IRGRP) ? 'r' : '-',
             (file_stat.st_mode & S_IWGRP) ? 'w' : '-',
             (file_stat.st_mode & S_IXGRP) ? 'x' : '-',
             (file_stat.st_mode & S_IROTH) ? 'r' : '-',
             (file_stat.st_mode & S_IWOTH) ? 'w' : '-',
             (file_stat.st_mode & S_IXOTH) ? 'x' : '-');

    char time_str[20];
    strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&file_stat.st_mtime));

    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "%s %ld %ld %ld %ld %s %s",
             permissions,
             (long)file_stat.st_nlink,
             (long)file_stat.st_uid,
             (long)file_stat.st_gid,
             (long)file_stat.st_size,
             time_str,
             file_path);

    send(client_socket, response, strlen(response), 0);
}

void stream_audio_file(const char *file_path, int client_socket) {
    read_file(file_path, client_socket);
}

void create_file(const char *file_path, const char *file_name, int client_socket) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", file_path, file_name);

    
// Check if the file path has an extension
    const char *dot = strrchr(file_name, '.');
     char command[5000];
    if (dot && dot != file_name) {
        // It's a file, send a response to the client
        // Extract the directory path from the full path
        char dir_path[BUFFER_SIZE];
        strncpy(dir_path, full_path, strrchr(full_path, '/') - full_path);
        dir_path[strrchr(full_path, '/') - full_path] = '\0';

        // Check if the directory exists, if not create it
        struct stat st = {0};
        if (stat(dir_path, &st) == -1) {
           
        snprintf(command, sizeof(command), "mkdir -p %s", dir_path);
        int result = system(command);
        if (result != 0) {
            perror("Failed to create directory");
        }
        }
        int k=0;
        int fd = open(full_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd < 0) {
            perror("Failed to create file");
            k=1;
        }

        close(fd);
        if(k==0){
        const char *response = "File created successfully";
        send(client_socket, response, strlen(response), 0);
        }
        else
        {
             const char *response = "File created failyfully";
            send(client_socket, response, strlen(response), 0);
        }
    } else {
        // It's a directory, create the directory
        //char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "mkdir -p %s", full_path);
        int result = system(command);
        if (result != 0) {
            perror("Failed to create directory");
            return;
        }
        const char *response = "Directory created successfully";
        send(client_socket, response, strlen(response), 0);
    }
}

bool is_prefix(const char *prefix, const char *str) {
    size_t prefix_len = strlen(prefix);
    size_t str_len = strlen(str);

    if (prefix_len > str_len) {
        return false;
    }

    return strncmp(prefix, str, prefix_len) == 0;
}
void delete_file_or_directory(const char *path, int client_socket) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < num_files_being_accessed; i++) {
        if (is_prefix(path, files_being_accessed[i])) {
            const char *response = "Cannot delete, a client is writing to a file in this directory";
            send(client_socket, response, strlen(response), 0);
            pthread_mutex_unlock(&lock);
            return;
        }
    }
    pthread_mutex_unlock(&lock);

    pthread_mutex_lock(&lock2);
    for (int i = 0; i < num_files_being_read; i++) {
        if (is_prefix(path, files_being_read[i])) {
            const char *response = "Cannot delete, a client is reading a file in this directory";
            send(client_socket, response, strlen(response), 0);
            pthread_mutex_unlock(&lock2);
            return;
        }
    }
    pthread_mutex_unlock(&lock2);
    struct stat path_stat;
    if (stat(path, &path_stat) < 0) {
        perror("Failed to get file or directory info");
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        // It's a directory, use system command to remove it recursively
        char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "trash-put %s", path);
        int result = system(command);
        if (result != 0) {
            perror("Failed to delete directory");
            return;
        }
        const char *response = "Directory deleted successfully";
        send(client_socket, response, strlen(response), 0);
    } else {

        if (remove(path) == 0) {
        printf("File '%s' deleted successfully.\n", path);
    } else {
        perror("Error deleting the file");
    }
        const char *response = "File deleted successfully";
        send(client_socket, response, strlen(response), 0);
    }
}
void copy_file(const char *source_path, const char *dest_path, int client_socket) {
    int src_fd = open(source_path, O_RDONLY);
    if (src_fd < 0) {
        perror("Failed to open source file");
        return;
    }

    int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd < 0) {
        perror("Failed to open destination file");
        close(src_fd);
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        write(dest_fd, buffer, bytes_read);
    }

    close(src_fd);
    close(dest_fd);
}


void stream_audio_file_binary(const char *file_path, int client_socket) {
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open audio file");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(fd);
}