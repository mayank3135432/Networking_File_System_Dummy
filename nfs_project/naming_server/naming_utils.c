#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "../common/utils.h"

#define MAX_CHILDREN 256
#define CACHE_SIZE 8




typedef struct TrieNode {
    struct TrieNode *children[MAX_CHILDREN];
    FILEINFO *file_info;
} TrieNode;

typedef struct {
    TrieNode *root;
} Trie;

typedef struct {
    char path[MAX_FILE_PATH_SIZE];
    FILEINFO file_info;
} CacheEntry;


static CacheEntry cache[CACHE_SIZE];
static int cache_count = 0;


// Function to find an entry in the cache
FILEINFO *find_in_cache(const char *path) {
    printf("Finding in cache\n");
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(cache[i].path, path) == 0) {
            // Move the found entry to the front (most recently used)
            CacheEntry temp = cache[i];
            for (int j = i; j > 0; j--) {
                cache[j] = cache[j - 1];
            }
            cache[0] = temp;
            return &cache[0].file_info;
        }
    }
    return NULL;
}

// Function to add an entry to the cache
void add_to_cache(const char *path, const FILEINFO *file_info) {
    printf("Adding %s to cache\n", path);
    // If the cache is full, remove the least recently used entry
    if (cache_count == CACHE_SIZE) {
        cache_count--;
    }

    // Move all entries one position to the right
    for (int i = cache_count; i > 0; i--) {
        cache[i] = cache[i - 1];
    }

    // Add the new entry to the front (most recently used)
    strncpy(cache[0].path, path, MAX_FILE_PATH_SIZE);
    cache[0].file_info = *file_info;
    cache_count++;
}

// Function to remove an entry from the cache
void remove_from_cache(const char* path) {
    printf("Removing %s from cache\n", path);
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(cache[i].path, path) == 0) {
            // Shift all entries after the found entry to the left
            for (int j = i; j < cache_count - 1; j++) {
                cache[j] = cache[j + 1];
            }
            cache_count--;
            return;
        }
    }
    printf("%s Entry not found in cache\n", path);
}


TrieNode *create_trie_node() {
    //printf("Creating trie node\n");
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    if (node) {
        for (int i = 0; i < MAX_CHILDREN; i++) {
            node->children[i] = NULL;
        }
        node->file_info = NULL;
    }
    return node;
}

Trie *create_trie() {
    printf("Creating trie\n");
    Trie *trie = (Trie *)malloc(sizeof(Trie));
    if (trie) {
        trie->root = create_trie_node();
    }
    return trie;
}



void update_file_info(Trie *trie, const StorageServerInfo *ss_info, const char *token) {
    printf("Updating file info via tries for %s\n", token);
    TrieNode *node = trie->root;
    for (int i = 0; token[i] != '\0'; i++) {
        int index = (unsigned char)token[i];
        if (!node->children[index]) {
            node->children[index] = create_trie_node();
        }
        node = node->children[index];
    }
    if (!node->file_info) {
        node->file_info = (FILEINFO *)malloc(sizeof(FILEINFO));
    }
    node->file_info->port_client = ss_info->port_client;
    strncpy(node->file_info->ip, ss_info->ip, INET_ADDRSTRLEN);
    strncpy(node->file_info->file_path, token, MAX_FILE_PATH_SIZE);
}

void update_file_info_fromdir(Trie *trie, const FILEINFO *ss_info, const char *token) {
    printf("Updating file info fromdir via tries for %s\n", token);
    TrieNode *node = trie->root;
    for (int i = 0; token[i] != '\0'; i++) {
        int index = (unsigned char)token[i];
        if (!node->children[index]) {
            node->children[index] = create_trie_node();
        }
        node = node->children[index];
    }
    if (!node->file_info) {
        node->file_info = (FILEINFO *)malloc(sizeof(FILEINFO));
    }
    node->file_info->port_client = ss_info->port_client;
    strncpy(node->file_info->ip, ss_info->ip, INET_ADDRSTRLEN);
    strncpy(node->file_info->file_path, token, MAX_FILE_PATH_SIZE);
}

FILEINFO *find_file_info(Trie *trie, const char *file_path) {
    printf("In function find_file_info\n");

    // Check the cache first
    FILEINFO *cached_info = find_in_cache(file_path);
    if (cached_info != NULL) {
        printf("Cache hit for %s\n", file_path);
        return cached_info;
    }

    // If not found in the cache, search the trie
    TrieNode *node = trie->root;
    for (int i = 0; file_path[i] != '\0'; i++) {
        int index = (unsigned char)file_path[i];
        if (!node->children[index]) {
            return NULL; // File not found
        }
        node = node->children[index];
    }

    // Add the found entry to the cache
    if (node->file_info != NULL) {
        add_to_cache(file_path, node->file_info);
    }

    return node->file_info;
}

FILEINFO* delete_file_info(Trie *trie, const char *file_path) {
    printf("delete from trie : %s\n", file_path);
    TrieNode *node = trie->root;
    for (int i = 0; file_path[i] != '\0'; i++) {
        int index = (unsigned char)file_path[i];
        if (!node->children[index]) {
            return NULL; // File not found
        }
        node = node->children[index];
    }
    FILEINFO *file_info = node->file_info;
    if (file_info) {
        free(file_info);
        node->file_info = NULL;
    }
    return file_info;
}

FILEINFO *find_directory_info(Trie *trie, const char *dir_path) {
    printf("In function find_directory_info\n");

    // Check the cache first
    FILEINFO *cached_info = find_in_cache(dir_path);
    if (cached_info != NULL) {
        printf("Cache hit for %s\n", dir_path);
        return cached_info;
    }

    // If not found in the cache, search the trie
    TrieNode *node = trie->root;
    for (int i = 0; dir_path[i] != '\0'; i++) {
        int index = (unsigned char)dir_path[i];
        printf("Index: %c\n", index);
        if (node->children[index] == NULL) {
            //printf("Directory not found\n");
            return NULL; // Directory not found
        }
        node = node->children[index];
    }

    // Add the found entry to the cache
    if (node->file_info != NULL) {
        add_to_cache(dir_path, node->file_info);
    }
    printf("return node->file_info\n");
    return node->file_info;
}

// listing functions 
void traverse_trie(TrieNode *node, char *path, int depth, int client_socket) {
    if (node == NULL) {
        return;
    }
    

    if (node->file_info != NULL) {
        // Send the file path to the client
        char full_path[MAX_FILE_PATH_SIZE];
        strncpy(full_path, path, depth);
        full_path[depth] = '\0';
        strcat(full_path, "\n");
        printf("sending %s\n", full_path);
        send(client_socket, full_path, strlen(full_path), 0);
        //printf("sent %s\n", full_path);
    }

    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (node->children[i] != NULL) {
            path[depth] = (char)i;
            traverse_trie(node->children[i], path, depth + 1, client_socket);
        }
    }
}

void list_files(Trie* trie, const char *dir_path, int client_socket) {
    TrieNode *node = trie->root;
    for (int i = 0; dir_path[i] != '\0'; i++) {
        //if(node->file_info != NULL) printf("In function traverse_trie: %s\n", node->file_info->file_path);    
        int index = (unsigned char)dir_path[i];
        printf("Index: %c\n", index);
        if (!node->children[index]) {
            printf("Directory not found\n");
            return; // Directory not found
        }
        node = node->children[index];
    }

    char path[MAX_FILE_PATH_SIZE];
    strncpy(path, dir_path, MAX_FILE_PATH_SIZE);
    int depth = strlen(dir_path);
    traverse_trie(node, path, depth, client_socket);
    // Send "STOP" to the client to indicate the end of the list

}




// Function to deep copy a TrieNode
TrieNode *copy_trie_node(const TrieNode *node) {
    if (node == NULL) {
        return NULL;
    }

    TrieNode *new_node = create_trie_node();
    if (new_node == NULL) {
        return NULL;
    }

    if (node->file_info != NULL) {
        new_node->file_info = (FILEINFO *)malloc(sizeof(FILEINFO));
        if (new_node->file_info == NULL) {
            free(new_node);
            return NULL;
        }
        memcpy(new_node->file_info, node->file_info, sizeof(FILEINFO));
    }

    for (int i = 0; i < MAX_CHILDREN; i++) {
        new_node->children[i] = copy_trie_node(node->children[i]);
    }

    return new_node;
}

// Function to find the TrieNode corresponding to a given path
TrieNode *find_trie_node(Trie *trie, const char *path) {
    TrieNode *node = trie->root;
    for (int i = 0; path[i] != '\0'; i++) {
        int index = (unsigned char)path[i];
        if (!node->children[index]) {
            return NULL; // Path not found
        }
        node = node->children[index];
    }
    return node;
}

// Function to insert a TrieNode at a specified path
void insert_trie_node(Trie *trie, const char *path, TrieNode *new_node) {
    TrieNode *node = trie->root;
    for (int i = 0; path[i] != '\0'; i++) {
        int index = (unsigned char)path[i];
        if (!node->children[index]) {
            node->children[index] = create_trie_node();
        }
        node = node->children[index];
    }
    // Free the existing subtree if any
    if (node->file_info) {
        free(node->file_info);
    }
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (node->children[i]) {
            free(node->children[i]);
        }
    }
    // Copy the new subtree
    *node = *new_node;
}

// Function to deep copy the trie of directory arg1 to that of directory arg2
void copy_directory_trie(Trie *trie, const char *arg1, const char *arg2) {
    printf("Copying directory trie from %s to %s\n", arg1, arg2);
    TrieNode *source_node = find_trie_node(trie, arg1);
    if (source_node == NULL) {
        printf("Source directory not found\n");
        return;
    }

    TrieNode *copied_node = copy_trie_node(source_node);
    if (copied_node == NULL) {
        printf("Failed to copy source directory\n");
        return;
    }

    insert_trie_node(trie, arg2, copied_node);
}