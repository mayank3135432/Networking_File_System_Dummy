#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../common/utils.h"

void connect_to_server(const char *ip, int port, int *sock) {
    struct sockaddr_in serv_addr;

    if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    printf("PLEASE CHECK STUFF\n");
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(*sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    //const char *initial_message = "CLIENT\n";
    //send(*sock, initial_message, strlen(initial_message), 0);
}

void read_file(int sock, const char *file_path) {
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "READ %s", file_path);
    printf("command sent to ns is %s\n",request);
    send(sock, request, strlen(request), 0);

    FILEINFO  file_info;
     int bytes_received;
     read(sock, &file_info, sizeof(file_info));
    // while ((bytes_received = recv(sock, response, sizeof(response), 0)) > 0) {
    //     response[bytes_received] = '\0';
    //     //printf("%s", response);
    // }
    if(file_info.port_client == 0){
       fprintf(stderr, "Error: File not found\n");
        //exit(FILE_NOT_FOUND);
        return;
    }
    printf("File Info:\npath:%s\nip:%s\nport:%d\n", file_info.file_path,file_info.ip,file_info.port_client);
    
    
    int ss_sock;
    struct sockaddr_in ss_addr;

    if ((ss_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(file_info.port_client);

    if (inet_pton(AF_INET, file_info.ip, &ss_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(ss_sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0) {
        perror("Connection to storage server failed");
        exit(EXIT_FAILURE);
    }

    // Now you can interact with the storage server using ss_sock
    // For example, you can request the file content
    char file_request[BUFFER_SIZE];
    snprintf(file_request, sizeof(file_request), "READ %s", file_info.file_path);
    send(ss_sock, file_request, strlen(file_request), 0);

    char file_content[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = recv(ss_sock, file_content, sizeof(file_content), 0)) > 0) {
        file_content[bytes_read] = '\0';
        printf("%s", file_content);
    }

    close(ss_sock);
}
int connect_to_server2(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return -1;
    }

    return sock;
}

void copy_file(int sock, const char *source_path, const char *dest_path) {
    // Step 1: Send the COPY command to the naming server
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "COPY %s %s", source_path, dest_path);
    printf("COMMAND SENT TO NS: %s\n", request);
    send(sock, request, strlen(request), 0);
      char msg[1024];
    int z=recv(sock,msg,strlen(msg)-1,0);
    msg[z]='\0';
    printf("%s\n",msg);
    // Step 2: Receive the FILEINFO array from the naming server
}

void write_file(int sock, const char *file_path) {
    char request[BUFFER_SIZE];
    printf("file path is %s\n",file_path);
    snprintf(request, sizeof(request), "%s", file_path);
    send(sock, request, strlen(request), 0);

    FILEINFO file_info;
    if (read(sock, &file_info, sizeof(file_info)) <= 0) {
        fprintf(stderr, "Error retrieving file info\n");
        return;
    }
   if (file_info.port_client == 0) {
        fprintf(stderr, "Error: File not found\n");
        return;
    }
    printf("File Info:\npath:%s\nip:%s\nport:%d\n", file_info.file_path, file_info.ip, file_info.port_client);

    int ss_sock;
    struct sockaddr_in ss_addr;

    if ((ss_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(file_info.port_client);

    if (inet_pton(AF_INET, file_info.ip, &ss_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(ss_sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0) {
        perror("Connection to storage server failed");
        exit(EXIT_FAILURE);
    }

    // Now you can interact with the storage server using ss_sock
    // For example, you can send the file content
    char file_request[BUFFER_SIZE];
    if (strstr(file_path, " --sync") != NULL) {
        snprintf(file_request, sizeof(file_request), "WRITE %s --sync", file_info.file_path);
    }else{
        snprintf(file_request, sizeof(file_request), "WRITE %s", file_info.file_path);
    }

    printf("FILE REQUEST: %s\n", file_request);
    send(ss_sock, file_request, strlen(file_request), 0);
    char data[MAX_WRITE_SIZE];
    printf("Enter data to write to the file: ");
    if (fgets(data, sizeof(data), stdin) != NULL) {
        data[strcspn(data, "\n")] = '\0'; // Remove newline character
    }
    send(ss_sock, data, strlen(data), 0);
    char response[BUFFER_SIZE];
    int bytes_received = recv(ss_sock, response, sizeof(response), 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("Response from storage server: %s\n", response);
    }
    char *sync_flag = strstr(file_path, " --sync");
    if (sync_flag == NULL) {
        printf("Waiting for asynchronous write confirmation\n");
        char sync_response[BUFFER_SIZE];
        int bytes_received = recv(sock, sync_response, sizeof(sync_response), 0);
        if (bytes_received > 0) {
            sync_response[bytes_received] = '\0';
            printf("aSynchronous write response: %s\n", sync_response);
        }
    }
    close(ss_sock);
}

void get_file_info(int sock, const char *file_path) {
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "INFO %s", file_path);
    send(sock, request, strlen(request), 0);

    FILEINFO file_info;
     if (read(sock, &file_info, sizeof(file_info)) <= 0) {
        fprintf(stderr, "Error retrieving file info\n");
        exit(INFO_RETRIEVAL_FAILED);
    }
   if (file_info.port_client == 0) {
        fprintf(stderr, "Error: File not found\n");
        return;
    }
    printf("File Info:\npath:%s\nip:%s\nport:%d\n", file_info.file_path, file_info.ip, file_info.port_client);

    int ss_sock;
    struct sockaddr_in ss_addr;

    if ((ss_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(file_info.port_client);

    if (inet_pton(AF_INET, file_info.ip, &ss_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(ss_sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0) {
        perror("Connection to storage server failed");
        exit(EXIT_FAILURE);
    }

    // Now you can interact with the storage server using ss_sock
    // For example, you can request the file info
    send(ss_sock, request, strlen(request), 0);

    char response[BUFFER_SIZE];
    int bytes_received = recv(ss_sock, response, sizeof(response), 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("File Info: %s\n", response);
    }
    char *sync_flag = strstr(file_path, " --sync");
    if (sync_flag != NULL) {
        printf("Waiting for synchronous write confirmation\n");
        char sync_response[BUFFER_SIZE];
        int bytes_received = recv(sock, sync_response, sizeof(sync_response), 0);
        if (bytes_received > 0) {
            sync_response[bytes_received] = '\0';
            printf("Synchronous write response: %s\n", sync_response);
        }
    }

    close(ss_sock);
}

#include <stdio.h>
#include <stdlib.h>
#include <portaudio.h>
#include <sndfile.h>

#define FRAMES_PER_BUFFER (512)

typedef struct {
    SNDFILE *file;
    SF_INFO info;
    int sock;
} AudioData;

static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData) {
    AudioData *data = (AudioData*)userData;
    sf_count_t numRead = sf_readf_float(data->file, (float*)outputBuffer, framesPerBuffer);
    if (numRead < framesPerBuffer) {
        return paComplete;
    }
    return paContinue;
}

void stream_audio_file(int sock, const char *file_path) {
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "STREAM %s", file_path);
    send(sock, request, strlen(request), 0);
    

    FILEINFO file_info;
     if (read(sock, &file_info, sizeof(file_info)) <= 0) {
        fprintf(stderr, "Error retrieving file info\n");
        return;
    }
    if (file_info.port_client == 0) {
        printf( "File not found to receive file info from naming server\n");
        return;
    }
    printf("File Info:\npath:%s\nip:%s\nport:%d\n", file_info.file_path, file_info.ip, file_info.port_client);
    int ss_sock;
    struct sockaddr_in ss_addr;

    if ((ss_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(file_info.port_client);

    if (inet_pton(AF_INET, file_info.ip, &ss_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(ss_sock);
        return;
    }

    if (connect(ss_sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0) {
        perror("Connection to storage server failed");
        close(ss_sock);
        return;
    }

    char file_request[BUFFER_SIZE];
    snprintf(file_request, sizeof(file_request), "STREAM %s", file_info.file_path);
    send(ss_sock, file_request, strlen(file_request), 0);
    
    char buffer[BUFFER_SIZE];
    FILE *pipe = popen("mpv --no-terminal -", "w");  // Use - to read from stdin
    if (!pipe) {
        perror("Failed to open audio player");
        close(ss_sock);
        return;
    }

    // Read incoming data and pass it to mpv
    int bytes_received;
    while ((bytes_received = recv(ss_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, pipe);  // Write the received data to mpv
        fflush(pipe);  // Ensure the data is immediately sent to the player
    }

    if (bytes_received == 0) {
        printf("Streaming completed\n");
    } else {
        perror("Error receiving data");
    }

    // Clean up
    fclose(pipe);
    close(ss_sock);
}


void create_file(int sock, const char *file_path, const char *file_name) {
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "CREATE %s %s", file_path, file_name);
    send(sock, request, strlen(request), 0);

    char response[BUFFER_SIZE];
    int bytes_received = recv(sock, response, sizeof(response), 0);
    if (bytes_received <= 0) {
        fprintf(stderr, "Error creating file\n");
        
    }
   
        response[bytes_received] = '\0';
        printf("Response from NS: %s\n", response);

}
void delete_file(int sock, const char *file_path) {
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "DELETE %s", file_path);
    send(sock, request, strlen(request), 0);

    char response[BUFFER_SIZE];
    int bytes_received = recv(sock, response, sizeof(response), 0);
     if (bytes_received <= 0) {
        fprintf(stderr, "Error deleting file\n");
        return;
    }
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("Response from NS: %s\n", response);
    }
}

void list_files(int sock, const char *dir_path){
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "LIST %s", dir_path);
    send(sock, request, strlen(request), 0);
    
}

int print_usage(char *program_name) {
    printf("Usage: %s <ns_ip> <ns_port> <command> [args]\n", program_name);
    printf("Commands:\n");
    printf("  read <file_path>\n");
    printf("  write <file_path> --sync\n");
    printf("  info <file_path>\n");
    printf("  stream <file_path>\n");
    printf("  create <file_path> <file_name>\n");
    printf("  copy <source_path> <dest_path>\n");
    printf("  list <dir_path>\n");
    printf("  delete <file_path>\n");
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *ns_ip = argv[1];
    int ns_port = atoi(argv[2]);
    char command[BUFFER_SIZE];

    int sock;
    printf("Connecting to Naming Server\n");
    connect_to_server(ns_ip, ns_port, &sock);
    printf("Connected to Naming Server\n");
    while(1){
        printf("Enter command: ");
        if (fgets(command, sizeof(command), stdin) != NULL) {
            command[strcspn(command, "\n")] = '\0'; // Remove newline character
        }
        printf("command is %s\n",command);
        char command_copy[BUFFER_SIZE];
        strncpy(command_copy, command, BUFFER_SIZE);
        if (strncmp(command, "read",4) == 0) {
            char *token = strtok(command, " ");
            char *args[BUFFER_SIZE];
            int i = 0;

            while (token != NULL) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;
            printf("%d\n\n",i);
            if (i != 2) {
                print_usage(argv[0]);
                continue;
            }
            const char *file_path = args[1];
            read_file(sock, file_path);
        } 
        else if (strncmp(command, "write", 5) == 0) {
            char *token = strtok(command, " ");
            char *args[BUFFER_SIZE];
            int i = 0;

            while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
            }
            args[i] = NULL;
            printf("%d\n\n", i);
            if (i != 2 ){
                if(i == 3 && strncmp(args[2],"--sync",6)!=0) {
                print_usage(argv[0]);
                continue;
                }
            }
            const char *file_path = args[1];
            printf("command is %s\n",command);
            write_file(sock, command_copy);
        } else if (strncmp(command, "stream", 6) == 0) {
            char *token = strtok(command, " ");
            char *args[BUFFER_SIZE];
            int i = 0;

            while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
            }
            args[i] = NULL;
            printf("%d\n\n", i);
            if (i != 2) {
            print_usage(argv[0]);
            continue;
            }
            const char *file_path = args[1];
            stream_audio_file(sock, file_path);
        } else if (strncmp(command, "create",6) == 0) {
            char *token = strtok(command, " ");
            char *args[BUFFER_SIZE];
            int i = 0;

            while (token != NULL) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;
            printf("%d\n\n", i);
            if (i != 3) {
                print_usage(argv[0]);
                continue;
            }
            const char *file_path = args[1];
            const char *file_name = args[2];
            create_file(sock, file_path, file_name);
        } else if (strncmp(command, "copy" ,4) == 0) {
            char *token = strtok(command, " ");
            char *args[BUFFER_SIZE];
            int i = 0;
            printf("is it coming here , %s \n",token);
            while (token != NULL) {
            args[i++] = token;
            printf("%s , %d \n",args[i-1],i-1);
            token = strtok(NULL, " ");
            }
             args[i] = NULL;
            if (i != 3) {
                print_usage(argv[0]);
                continue;
            }
            const char *source_path = args[1];
            const char *dest_path = args[2];
            printf("%s , %s",source_path,dest_path);
            copy_file(sock, source_path, dest_path);
        }
        else if (strncmp(command, "info", 4) == 0) {
            char *token = strtok(command, " ");
            char *args[BUFFER_SIZE];
            int i = 0;

            while (token != NULL) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;
            printf("%d\n\n", i);
            if (i != 2) {
                print_usage(argv[0]);
                continue;
            }
            const char *file_path = args[1];
            get_file_info(sock, file_path);
        }
        else if (strncmp(command, "delete", 6) == 0) {
            char *token = strtok(command, " ");
            char *args[BUFFER_SIZE];
            int i = 0;

            while (token != NULL) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;
            printf("%d\n\n", i);
            if (i != 2) {
                print_usage(argv[0]);
                continue;
            }
            const char *file_path = args[1];
            delete_file(sock, file_path);
        }
        else if (strncmp(command, "list",4) == 0) {
            char *token = strtok(command, " ");
            char *args[BUFFER_SIZE];
            int i = 0;

            while (token != NULL) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;
            printf("%d\n\n", i);
            if (i != 2) {
                print_usage(argv[0]);
                continue;
            }
            const char *file_path = args[1];

            printf("Requesting file list from Naming Server\n");
            char request[BUFFER_SIZE];
            snprintf(request, sizeof(request), "list %s", file_path);
            send(sock, request, strlen(request), 0);

            char response[BUFFER_SIZE];
            printf("Files:");
            while (1) {
                int bytes_received = recv(sock, response, sizeof(response), 0);
                if (bytes_received > 0) {

                    response[bytes_received] = '\0';
                    
                    if (strstr(response, "STOP") != NULL) {
                        break;
                    }
                    printf("%s\n", response);
                } else {
                    perror("Failed to receive data");
                    break;
                }
            }
        }
         else {
            print_usage(argv[0]);
            continue;
        }
    }
    close(sock);
    return EXIT_SUCCESS;
}