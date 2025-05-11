/**************************************************************
* Contains the helper functions for the file system
**************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/fsHelperFuncs.h"
#include "../include/fsLow.h"
#include "../include/mfs.h"
#include "../include/fsDirectory.h"

// Calculate number of blocks
int retrieve_num_of_blocks(int bytes, int block_size) {
    if (block_size <= 0)
        return 0;

    return (bytes + (block_size - 1)) / block_size;
}

// Free allocated memory
void free_memory() {
    // Free volume control block
    free(fs_vcb);
	fs_vcb = NULL;

    // Free freespace
	free(fs_freespace);
	fs_freespace = NULL;

    // Free root directory
    free(fs_dir_root);
    fs_dir_root = NULL;
}

// Returns 0 if valid and -1 otherwise
int parse_path(char* path_name, struct parse_path_return_data* parse_path_info) {
    DirectoryEntry* start_parent;
    DirectoryEntry* parent;

    // Safety first
	if (path_name == NULL || parse_path_info == NULL) {
		return -1; 
	}

    if (path_name[0] == '/') { // Absolute path
        start_parent = fs_dir_root; // Start at the root
    } else {  // Relative path
        start_parent = fs_dir_curr; // Start at current directory
    }

    parent = start_parent;
    // Tokenize the path 
    char* saveptr;
    char* token;
    char* token2;
    int index_of_DE;

    char* temp_path_name = strdup(path_name);

    token = strtok_r(temp_path_name, "/", &saveptr);

    // This means that the path_name is “/” or path_name is an empty string
    if (token == NULL) { 
        if (strcmp(temp_path_name, "/") == 0) { // Root only
            parse_path_info->parent = parent;
			parse_path_info->last_element_index = -2; // There is no last element
			parse_path_info->last_element_name = NULL;
            //free(temp_path_name);
			return 0; // Valid path
        } else {
            //free(temp_path_name);
            return -1; // Invalid path
        }
    }
    while (1) {
        // Get next token in the path name
        token2 = strtok_r(NULL, "/", &saveptr);

        // Get the index token in the parent directory
        index_of_DE = get_DE_index(parent, token);
        if (index_of_DE == -1) { // Token doesn't exist
            if (token2 == NULL) { // Token is the last token in the path name
                parse_path_info->parent = parent;
                parse_path_info->last_element_index = index_of_DE;
                parse_path_info->last_element_name = token;
                //free(temp_path_name);
                return 0; // Valid path
            } else {
                //free(temp_path_name);
                return -1; // Invalid path
            }
        }
        // Token exists, and it is the last element in the path
        if (token2 == NULL) {
            parse_path_info->parent = parent;
            parse_path_info->last_element_index = index_of_DE;
            parse_path_info->last_element_name = token;
            //free(temp_path_name);
            return 0; // Valid path
        }
        // Token exists, and it is not the last element
        // Still in the middle of the path

        if (is_DE_a_directory(&parent[index_of_DE])) {
		    DirectoryEntry* temp_parent = load_dir(&parent[index_of_DE]);
            if (parent != start_parent) {
	            free(parent);
                parent = NULL;
            } 
            parent = temp_parent;
            token = token2;
	    } else { 
            // A regular file in a middle of a path
            //free(temp_path_name);
		    return -1; // Invalid path
	    }

    }
} 

// Retrieve the index of a directory based on the token
int get_DE_index(DirectoryEntry* dir_array, char* token) {
    int num_DE = dir_array[0].size / sizeof(DirectoryEntry);

    for (int i = 0; i < num_DE; i++) {
        if (strcmp(token, dir_array[i].name) == 0) 
            return i;
    }
    return -1;
}

// Retrieve the first available DE
int get_available_DE_index(DirectoryEntry* dir_array) {
    int num_DE = dir_array[0].size / sizeof(DirectoryEntry);

    for (int i = 2; i < num_DE; i++) {
        if (strcmp(dir_array[i].name, "") == 0)
            return i;
    }
    return -1;
}

// Check if it is a directory
int is_DE_a_directory(DirectoryEntry* dir){
    return (dir->is_dir == FILE_TYPE_DIRECTORY);
}

int is_DE_exist(DirectoryEntry* parent, char *name){
    int num_DE = parent[0].size / sizeof(DirectoryEntry);

	for (int i = 2; i < num_DE; i++) {
        if (strcmp(parent[i].name, name) == 0)
            return 0; // DE exist
    }
    return -1; // DE doesn't exist
}

void free_directory(DirectoryEntry* dir){
    // Don't free the directory if it is the root or the current directory
    if(dir != NULL && dir != fs_dir_root && dir != fs_dir_curr){
        free(dir);
        dir = NULL;
    }
}
