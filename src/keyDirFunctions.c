/**************************************************************
* Contains two key directory functions mkdir() and rmdir()
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

void remove_attached_dirs(DirectoryEntry *dir_to_remove){
    DirectoryEntry* dir = load_dir(dir_to_remove);

    int num_DE = dir[0].size / sizeof(DirectoryEntry);
    
    for (int i = 2; i < num_DE; i++) {
        if(strcmp(dir[i].name, "") != 0){
            if(dir[i].is_dir == FILE_TYPE_REGULAR){
                // Free space for file
                clear_freespace(dir[i].start_block);
            }
            else{
                // Free space for directory
                remove_attached_dirs(&dir[i]);
            }
        }
    }

    clear_freespace(dir->start_block);
    free(dir);
}

// Make a directory
int fs_mkdir(const char *pathname, mode_t mode) {
    struct parse_path_return_data parse_path_info;

    // Invalid path
	if (parse_path((char*) pathname, &parse_path_info) != 0) {
        fprintf(stderr, "\nINVALID PATH. ERROR: %d\n", -1);
		return -1; 
	} 
    // Last element is found, so you can’t make the dir cuz it already exists.
	if (parse_path_info.last_element_index != -1) {
        free_directory(parse_path_info.parent);
        fprintf(stderr, "\nFILE OR DIRECTORY EXISTS. ERROR: %d\n", -2);
        return -2; // File or directory exists
	}
    //check the size of the name of the directory to be created 
    if(strlen(parse_path_info.last_element_name) > MAX_NAME_SIZE){
        fprintf(stderr, "\nNAME SIZE TOO BIG. ERROR\n");
        return -1;
    }

    // Create the new directory
    DirectoryEntry* new_dir = create_directory(parse_path_info.parent, MAX_DIR_ENTRIES);

    int index = get_available_DE_index(parse_path_info.parent);
    // Copy the last element of the path to the next available index in the parent
    strcpy(parse_path_info.parent[index].name, parse_path_info.last_element_name);
    parse_path_info.parent[index].size = new_dir[0].size;
    parse_path_info.parent[index].start_block = new_dir[0].start_block;
    parse_path_info.parent[index].is_dir = FILE_TYPE_DIRECTORY;
    parse_path_info.parent[index].creation_time = new_dir[0].creation_time;
    parse_path_info.parent[index].modification_time = new_dir[0].modification_time;
    parse_path_info.parent[index].access_time = new_dir[0].access_time;

    // Update access and modifie time for the parent
    time_t actual_time = time(NULL);
    parse_path_info.parent[0].modification_time = actual_time;
    parse_path_info.parent[0].access_time = actual_time;

    // Rewrite the parent to the drive
	write_dir(parse_path_info.parent);
    free_directory(parse_path_info.parent);
    free_directory(new_dir);

	return 0;
}

// Remove a directory
int fs_rmdir(const char *pathname) {
    struct parse_path_return_data parse_path_info;

    // Invalid path
	if (parse_path((char*) pathname, &parse_path_info) != 0) {
		return -1; 
	} 
    // Last element doesn't exist, so you can't delete it
	if (parse_path_info.last_element_index == -1) {
        free_directory(parse_path_info.parent);
        return -3; // File or directory exists
	}
    int index = parse_path_info.last_element_index;

    // Free all directories and files attached to the directory to remove
    remove_attached_dirs(&parse_path_info.parent[index]);

    // Update the parent 
    strcpy(parse_path_info.parent[index].name, "");
    time_t actual_time = time(NULL);
    parse_path_info.parent[0].modification_time = actual_time;
    parse_path_info.parent[0].access_time = actual_time;
    
    // Rewrite the parent to the drive
	write_dir(parse_path_info.parent);
    free_directory(parse_path_info.parent);

	return 0;
}





    