#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define BUFFER_SIZE 1024

void handle_zip(const char *path, const char *parent, const char *last, int flag2) {
    printf("Flag not set\n");
    char command2[BUFFER_SIZE];
    char original_dir[BUFFER_SIZE];

    // Save the current working directory
    if (getcwd(original_dir, sizeof(original_dir)) == NULL) {
        perror("Failed to get current directory");
        return;
    }

    if (flag2 == 1) {
        printf("ek baar to dhang se chal jao\n");
        snprintf(command2, sizeof(command2), "zip -r %s.zip %s", path, path);
    } else {
        // Change to the parent directory
        if (chdir(parent) != 0) {
            perror("Failed to change directory");
            return;
        }


        // Construct the zip command relative to the parent directory
        snprintf(command2, sizeof(command2), "zip -r %s.zip %s", last, last);
    }

    // Execute the zip command
    int result = system(command2);
    if (result != 0) {
        perror("Failed to zip folder");
    } else {
        printf("Successfully zipped the folder.\n");
    }
  if (chdir(original_dir) != 0) {
        perror("Failed to return to the original directory");}
    // Extract the zip file to a temporary directory
    char unzip_command[BUFFER_SIZE];
    snprintf(unzip_command, sizeof(unzip_command), "%s.zip", path);
    // result = system(unzip_command);
    // if (result != 0) {
    //     perror("Failed to unzip folder");
    // } else {
    //     printf("Successfully extracted the zip file.\n");

        // Now read the contents of the extracted files (example: reading a file in temp_dir)
        FILE *file = fopen(unzip_command, "r");
        if (file != NULL) {
            char line[BUFFER_SIZE];
            while (fgets(line, sizeof(line), file)) {
                printf("File content: %s", line);
            }
            fclose(file);
        } else {
            perror("Failed to open the extracted file");
        }
    }


    


int main() {
    // Example variables (replace with actual values)
    const char *path = "test/again";  // Example folder to zip
    const char *parent = "test";
    const char *last = "again";
    int flag2 = 0;

    handle_zip(path, parent, last, flag2);

    return 0;
}
