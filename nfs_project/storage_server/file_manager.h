#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H
#define CHUNK_SIZE 64
#define MAX_WRITE_SIZE 4096
#define BUFFER_SIZE 4096
void read_file(const char *file_path, int client_socket);
void write_file(const char *file_path, int client_socket,char* global_nm_ip,int global_nm_port,char* global_ss_ip,int global_ss_port);
void copy_to_file(const char *file_path, int client_socket);
void paste_to_folder(const char *file_path, int client_socket);
void get_file_info(const char *file_path, int client_socket);
void stream_audio_file(const char *file_path, int client_socket);
void create_file(const char *file_path, const char *file_name, int client_socket);
void copy_file(const char *source_path, const char *dest_path, int client_socket);
void delete_file_or_directory(const char *path, int client_socket) ;

#endif // FILE_MANAGER_H