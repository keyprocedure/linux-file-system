/**************************************************************
* Contains the miscellaneous directory functions 
* fs_getcwd(), fs_setcwd(), fs_isFile(), fs_isDir(), and fs_delete()
**************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "../include/fsLow.h"
#include "../include/fsFreespace.h"
#include "../include/mfs.h"
#include "../include/fsHelperFuncs.h"
#include "../include/fsDirectory.h"
#include <errno.h>

// Initialize a global variable
char cwd_str[1024] = {'.'};

char* simplifyPath(char* path) {
    // Calculate an upper bound for the number of tokens
    int path_len = strlen(path);
    char** stack = malloc(path_len * sizeof(char*));
    int top = -1;
    char* result = malloc(path_len + 1);
    char* token;
    // Initialize result
    strcpy(result, "/");
    // Tokenize the path by slashes
    token = strtok(path, "/");

    while (token != NULL) {
        if (!strcmp(token, "..")) {
            if (top >= 0) { // Pop from stack if not empty
                free(stack[top]); // Free the allocated memory for the directory name
                top--;
            }
        } else if (strcmp(token, ".") && strcmp(token, "")) {
            stack[++top] = strdup(token); // Push valid directory names to the stack
        }

        token = strtok(NULL, "/");
    }

    // Construct the canonical path from the stack
    for (int i = 0; i <= top; i++) {
        strcat(result, stack[i]);
        strcat(result, "/");
        free(stack[i]); // Free the memory for each directory name
    }

    if (top == -1) { // If stack is empty, only return "/"
        strcpy(result, "/");
    }

    free(stack);
    return result;
}

// Retrieve the current working directory 
char * fs_getcwd(char *pathname, size_t size) {
    // Check if the current directory pointer is valid
    if (fs_dir_curr == NULL) {
        fprintf(stderr, "Error: Current directory pointer is null.\n");
        return NULL;
    }
    // Calculate needed buffer size
    size_t needed_size = strlen(cwd_str) + 1;  // +1 for the null terminator
    if (pathname == NULL) {
        // Allocate memory if pathname is not provided
        pathname = malloc(needed_size);
        if (pathname == NULL) {
            fprintf(stderr, "Error: Memory allocation failed for current working directory.\n");
            return NULL;
        }
    } else if (size < needed_size) {
        // Check if provided buffer is large enough
        fprintf(stderr, "Error: Buffer too small for current working directory.\n");
        return NULL;
    }
    // Copy the directory name to the pathname buffer
    strcpy(pathname, cwd_str);
    return pathname;
}

// Update the current working directory
int fs_setcwd(char *pathname) {
    if (pathname == NULL) {
        fprintf(stderr, "Error: NULL pathname provided to fs_setcwd.\n");
        return -1; // Fail if pathname is NULL
    }
    
    // Parse the path and handle failures.
    struct parse_path_return_data parse_path_info;
    if (parse_path(pathname, &parse_path_info) != 0) {
        fprintf(stderr, "Error: Path parsing failed in fs_setcwd.\n");
        return -1; // Fail if the path cannot be parsed correctly
    }

    int index = parse_path_info.last_element_index;

    // Validate that the parsed path refers to a directory
    if (index == -1 || 
       (index != -2 && parse_path_info.parent[index].is_dir != FILE_TYPE_DIRECTORY)) {
        fprintf(stderr, "Error: Path does not refer to a valid directory.\n");
        return -1; // Fail if the target is not a directory or doesn't exist
    }

    // Update the current directory based on parsed path
    if(index == -2) {
        fs_dir_curr = fs_dir_root;
    } else {
        fs_dir_curr = load_dir(&parse_path_info.parent[index]);
    }
    
    // Update the 'cwd_str' to reflect the new directory
    if (pathname[0] == '/') {
        strcpy(cwd_str, pathname);
    } else {
        if (!strcmp(cwd_str, ".")) {
            strcpy(cwd_str, "/");
        }

        strcat(cwd_str, pathname);
    }
    
    // Simplify the path to its canonical form and update 'cwd_str'
    char * simple_cwd = simplifyPath(cwd_str);
    strcpy(cwd_str, simple_cwd);
    free(simple_cwd);

    // Handle edge case for the root directory
    if(!strcmp(cwd_str, "/")) {
        strcpy(cwd_str, ".");
    }

    return 0; // Return success
}

// Returns 1 if is a file, 0 otherwise
int fs_isFile(char * filename) {
    struct parse_path_return_data parse_path_info;

    // Invalid path check
    if (parse_path(filename, &parse_path_info) != 0) {
        return 0;
    }

    int index = parse_path_info.last_element_index;

    if (index < 0) {
        free_directory(parse_path_info.parent);
        fprintf(stderr, "File or directory not found.\n");
        return -2;
    }

    if (parse_path_info.parent[index].is_dir == FILE_TYPE_REGULAR) {
        free_directory(parse_path_info.parent);
        return 1;
    }
    free_directory(parse_path_info.parent);
    return 0;
}

// Returns 1 if is directory, 0 otherwise
int fs_isDir(char * pathname) {
    struct parse_path_return_data parse_path_info;

    // Invalid path check
    if (parse_path(pathname, &parse_path_info) != 0) {
        return 0;
    }
    int index = parse_path_info.last_element_index;

    if (index < 0) {
        free_directory(parse_path_info.parent);
        fprintf(stderr, "File or directory not found.\n");
        return -2;
    }

    if (parse_path_info.parent[index].is_dir == FILE_TYPE_DIRECTORY) {
        free_directory(parse_path_info.parent);
        return 1;
    }
    free_directory(parse_path_info.parent);
    return 0;
}

// Removes a file
int fs_delete(char *filename) {
    if (filename == NULL) {
        fprintf(stderr, "Error: NULL filename provided to fs_delete.\n");
        return -1; // Fail if filename is NULL
    }

    struct parse_path_return_data parse_path_info;
    if (parse_path(filename, &parse_path_info) != 0) {
        fprintf(stderr, "Error: Path parsing failed in fs_delete.\n");
        return -1; // Fail if the path cannot be parsed correctly
    }

    int index = parse_path_info.last_element_index;
    if (index < 0) {
        free_directory(parse_path_info.parent);
        fprintf(stderr, "Error: File not found in fs_delete.\n");
        return -1; // Fail if the file doesn't exist
    }

    DirectoryEntry target_entry = parse_path_info.parent[index];
    if (target_entry.is_dir == FILE_TYPE_DIRECTORY) {
        free_directory(parse_path_info.parent);
        fprintf(stderr, "Error: Attempted to delete a directory with fs_delete.");
        fprintf(stderr, " Use fs_rmdir for directories.\n");
        return -1; // Fail if trying to delete a directory
    }

    // Free the blocks used by the file using clear_freespace
    clear_freespace(target_entry.start_block);

    // Update to the parent reset the name and size
    strcpy(parse_path_info.parent[index].name, "");
    parse_path_info.parent[index].size = 0;
    time_t actual_time = time(NULL);
    parse_path_info.parent[0].modification_time = actual_time;
    parse_path_info.parent[0].access_time = actual_time;

    // Update changes to disk
    write_dir(parse_path_info.parent);
    free_directory(parse_path_info.parent);

    return 0;
}

// Fill fs_stat buffer with data from the path provided
int fs_stat(const char *path, struct fs_stat *buf) {
    struct parse_path_return_data parse_path_info;

    // Invalid path check
    if (parse_path((char*) path, &parse_path_info) != 0) {
        return -1;
    }

    int index = parse_path_info.last_element_index;

    if (index < 0) {
        free_directory(parse_path_info.parent);
        fprintf(stderr, "File or directory not found.\n");
        return -2;
    }

    buf->st_size = parse_path_info.parent[index].size;
    buf->st_blksize = fs_vcb->size_of_blocks;
    buf->st_blocks = retrieve_num_of_blocks(parse_path_info.parent[index].size, BLOCK_SIZE);
    buf->st_accesstime = parse_path_info.parent[index].access_time;
    buf->st_modtime = parse_path_info.parent[index].modification_time;
    buf->st_createtime = parse_path_info.parent[index].creation_time;

    free_directory(parse_path_info.parent);

    return 0;
}