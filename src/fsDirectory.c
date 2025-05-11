/**************************************************************
* Contains the functions for the directory operations
**************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#include "../include/fsDirectory.h"
#include "../include/mfs.h"
#include "../include/fsHelperFuncs.h"
#include "../include/fsLow.h"
#include "../include/fsFreespace.h"

int space_needed; // Space needed for the initial number of DE
int block_needed; // Block needed for the space needed
int space_allocated; // Space actually allocated according to the number of blocks needed
int actual_DE_num; // Actual number of DE that fit in the spaceAlloctaed
int actual_size_needed;  // Actual size needed after adjusting the number of DE
size_t size_DE = sizeof(DirectoryEntry); // Size of directory entry

// Calculate the space to allocate, number of DE that fit in that space
// Calculate number of blocks needed for that space
void init_space_block_needed(int entries) {
    space_needed = size_DE * entries;  
    block_needed = retrieve_num_of_blocks(space_needed, BLOCK_SIZE);   
    space_allocated = block_needed * BLOCK_SIZE;
    actual_DE_num = space_allocated / size_DE; 
    actual_size_needed = actual_DE_num * size_DE;
}

// Return the new directory entry
DirectoryEntry* create_directory(DirectoryEntry* parent, int number_dir_entries) {
    DirectoryEntry* fs_dir;
    bool is_root = false;
    time_t actual_time = time(NULL); // Actual time for the directory creation

    init_space_block_needed(number_dir_entries);

    // Allocate array of DirectoryEntries according to the number of blocks needed
    fs_dir = malloc(space_allocated);
    if (fs_dir == NULL) {
        fprintf(stderr, "Memory allocation for directory failed\n");
        return NULL;
    }

    // Get starting location of directory by calling freespace
    int start_dir = allocate_freespace(block_needed);  
    // Set the number of blocks occupied by root dir
    fs_vcb->root_blocks = block_needed;

    // Initialize each entry in the directory to free state
    // initialize the name to empty string to indicate that the DE is not used
    // i=0 and i=1 are for . and ..
    for (int i=2; i<actual_DE_num; i++)
        strcpy(fs_dir[i].name, "");

    // Initialize dir[0] to "."
    strcpy(fs_dir[0].name, ".");               // Copies the directory name into the entry
    fs_dir[0].size = actual_size_needed;       // Sets the size of the directory/file
    fs_dir[0].start_block = start_dir;         // Sets starting block of the directory/file on disk
    fs_dir[0].is_dir = FILE_TYPE_DIRECTORY;    // Sets whether the entry is a directory
    fs_dir[0].creation_time = actual_time;     // Set creation time to actual time
    fs_dir[0].modification_time = actual_time; // Set modification time to actual time 
    fs_dir[0].access_time = actual_time;       // Set access time to actual time

    // Check if the created directory is root or another directory
    if (parent == NULL) { // It is root    
        parent = fs_dir; // Root is its own parent
        is_root = true;
    }
    // Initialize dir[0] to ".."
    strcpy(fs_dir[1].name, ".."); // Copies the directory name into the entry
    fs_dir[1].size = parent[0].size;
    fs_dir[1].start_block = parent[0].start_block;
    fs_dir[1].is_dir = parent[0].is_dir;  
    fs_dir[1].creation_time = parent[0].creation_time;     
    fs_dir[1].modification_time = parent[0].modification_time;  
    fs_dir[1].access_time = parent[0].access_time;                    

    
    // If is root, assign the directory created to the root to keep it in memory
    if (is_root)
        fs_dir_root = fs_dir;

    // Write directory to disk
    write_dir(fs_dir);
    
    return fs_dir;
}

// Load root directory to memory
int load_root_directory() {
    load_dir_helper(fs_dir_root, fs_vcb->location_of_rootdir);
    return fs_vcb->root_blocks;
}

// Load a selected directory to memory
DirectoryEntry* load_dir(DirectoryEntry* dir){
    // Number of blocks to allocate for dir
    int dir_blocks = retrieve_num_of_blocks(dir->size, BLOCK_SIZE);

    // Malloc memory to load dir
    DirectoryEntry* loaded_dir = malloc(dir_blocks * BLOCK_SIZE);

    // Call helper function to load the directory since the freespace may not be contiguous
    load_dir_helper(loaded_dir, dir->start_block);

    return loaded_dir;
}

// Write a directory to drive
void write_dir(DirectoryEntry* dir) {
    // Call helper function to write the directory since the freespace may not be contiguous
    write_dir_helper(dir);
}

/*
 * Helper function to write a directory to the drive. 
 * Since the directory's data blocks may not be
 * contiguous, each block must be located and written individually
 */
int write_dir_helper(DirectoryEntry* dir) {
    char buffer[BLOCK_SIZE];
    int number_of_directory_entries = actual_DE_num;
    int volume_block = dir->start_block;
    int dir_index = 0;
    int dir_offset = 0;
    int buffer_offset = 0;

    // Set each value in the buffer to 0
    memset(buffer, 0, BLOCK_SIZE);

    // Iterate through all the directory entries
    while (dir_index < number_of_directory_entries) {
        int space_in_buffer = BLOCK_SIZE - buffer_offset;
        int dir_bytes_remaining = size_DE - dir_offset;
        int bytes_to_copy = space_in_buffer >= 
        dir_bytes_remaining ? dir_bytes_remaining : space_in_buffer;

        // Copy a part of or the whole directory to the buffer
        memcpy(buffer + buffer_offset, ((char*) &dir[dir_index]) + dir_offset, bytes_to_copy);

        // Increment offset for the buffer and the current directory
        buffer_offset += bytes_to_copy;
        dir_offset += bytes_to_copy;

        // If the entire directory has been copied
        if (dir_offset == size_DE) {
            // Reset the directory entry offset
            dir_offset = 0;
            // Iterate the count of the directories
            dir_index++;
        }

        // If buffer is full
        if (buffer_offset == BLOCK_SIZE) {
            // Write a new block of data to the buffer
            if (LBAwrite(buffer, 1, volume_block) != 1) {
                fprintf(stderr, "Failed to write a directory buffer.\n");
                free(dir);
                return -1;
            }

            // If the FAT entry's value is its own index, this was the last block allocated for
            // this directory
            if (fs_freespace[volume_block] == volume_block)
                break;

            // Track the index of the next block in the FAT
            volume_block = fs_freespace[volume_block];
            buffer_offset = 0;
        }
    }

    // If there's partial data remaining in the buffer
    if (buffer_offset > 0) {
        // Clear the part of the buffer that wasn't most recently written to
        memset(buffer + buffer_offset, 0, BLOCK_SIZE - buffer_offset);

        // Write the block to the volume
        if (LBAwrite(buffer, 1, volume_block) != 1) {
            fprintf(stderr, "Failed to write the last directory buffer.\n");
            free(dir);
            return -1;
        }
    }

    return 0;
}

/*
 * Helper function to load a directory to memory. Since the directory's data blocks may not be
 * contiguous, each block must be located and loaded individually
 */
int load_dir_helper(DirectoryEntry* dir, int start_block) {
    // If class variables have not been set
    if (actual_DE_num == 0)
        // Initialize the directory information
        init_space_block_needed(MAX_DIR_ENTRIES);

    char buffer[BLOCK_SIZE];
    int number_of_directory_entries = actual_DE_num;
    int volume_block = start_block;
    int dir_index = 0;
    int dir_offset = 0;
    int buffer_offset = BLOCK_SIZE;
    bool has_more_blocks = true;

    // Iterate through all the directory entries
    while (dir_index < number_of_directory_entries) {
        // If buffer is empty and there are more blocks in the freespace for the directory
        if (buffer_offset == BLOCK_SIZE && has_more_blocks) {
            // Load a block from the volume
            if (LBAread(buffer, 1, volume_block) != 1) {
                fprintf(stderr, "Failed to read a directory buffer.\n");
                free(dir);
                return -1;
            }

            // If the FAT entry's value is its own index, this was the last block
            if (fs_freespace[volume_block] == volume_block)
                has_more_blocks = false;
            else
                // Move the volume block index to next block
                volume_block = fs_freespace[volume_block];

            // Reset buffer offset for new block
            buffer_offset = 0;
        }

        // Process each directory entry from the buffer
        int bytes_in_buffer = BLOCK_SIZE - buffer_offset;
        int dir_bytes_needed = size_DE - dir_offset;
        int bytes_to_copy = bytes_in_buffer >= 
        dir_bytes_needed ? dir_bytes_needed : bytes_in_buffer;

        // Copy the directory from the buffer to the directory entry
        memcpy(((char*) &dir[dir_index]) + dir_offset, buffer + buffer_offset, bytes_to_copy);

        // Increment the buffer and directory entry offsets
        buffer_offset += bytes_to_copy;
        dir_offset += bytes_to_copy;

        // Check if the current directory entry is completely loaded
        if (dir_offset == size_DE) {
            // Reset the directory entry offset
            dir_offset = 0;
            // If the entire directory has been copied, iterate the count of the directories
            dir_index++;
        }
    }

    return 0;
}