/**************************************************************
* Contains the prototype of the functions for directory operations
**************************************************************/
#ifndef FSDIRECTORY_H
#define FSDIRECTORY_H

#include <sys/types.h> // For mode_t and other standard POSIX types
#include <time.h>      // For time management functions
#include "mfs.h"       // File system specific definitions and declarations

// Function prototypes for filesystem directory operations

// Function to create a directory
DirectoryEntry* create_directory(DirectoryEntry* parent, int number_dir_entries); 

// Function to initialize space requirements
void init_space_block_needed(int entries);

//load root directory to memory
int load_root_directory(); 
DirectoryEntry* load_dir(DirectoryEntry* dir);
void write_dir(DirectoryEntry* dir);
int write_dir_helper(DirectoryEntry* dir);
int load_dir_helper(DirectoryEntry* dir, int start_block);

#endif // FSDIRECTORY_H