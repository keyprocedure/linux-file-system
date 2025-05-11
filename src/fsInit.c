/**************************************************************
* Main driver for the file system
* Starts and initializes the system
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
#include "../include/fsFreespaceHelper.h"

#define C_PROMPT  "\x1b[95m"
#define C_TITLE   "\x1b[35m"
#define C_LABEL   "\x1b[32m"
#define C_VALUE   "\x1b[36m"
#define C_RESET   "\x1b[0m"

long MAGIC_NUMBER = 742891252;

VCB *fs_vcb; // Volume control block
unsigned short *fs_freespace; // Freespace array (FAT)
DirectoryEntry* fs_dir_root; // Root directory
DirectoryEntry* fs_dir_curr; // Current directory

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize) {
    fs_vcb = malloc(blockSize); // Allocate memory for the VCB
    LBAread(fs_vcb, 1, 0); // Read the first block into the VCB

    // If the file system has been previously initialized, the freespace is loaded from the
    // volume, otherwise the freespace is initialized
    initialize_freespace(numberOfBlocks, blockSize);

	/*
		Check if the VCB signature matches the magic number.
		If it matches, load the freespace into memory.
		If it doesn't match, initialize the values in VCB,
		free space, and the root directory. Also, LBAWrite()
		the VCB to block 0.
	*/
    if (fs_vcb->signature == MAGIC_NUMBER) {
        // Load root directory to memory

        // Allocate memory for root directory
        fs_dir_root = malloc(fs_vcb->root_blocks * blockSize);

        if (load_root_directory() != fs_vcb->root_blocks) {
            perror("Root directory failed to load.\n");
            return -1;
        }

        //printf(C_LABEL "Root directory loaded.\n\n" C_RESET);

        // At the beginning,current dir is root dir
        fs_dir_curr = fs_dir_root;
    } else {
        printf(C_TITLE "+ Initializing File System\n" C_RESET);
        printf("  " C_VALUE "%ld blocks × %ld bytes\n\n" C_RESET, numberOfBlocks, blockSize);

        strcpy(fs_vcb->volume_name, "Test volume name");
        fs_vcb->signature = MAGIC_NUMBER;
        fs_vcb->num_blocks = numberOfBlocks;
        fs_vcb->size_of_blocks = blockSize;

        // Initialize root directory. NULL means root doesn't have parent
        fs_dir_root = create_directory(NULL, MAX_DIR_ENTRIES);
        // Start block of the root in the volume
        fs_vcb->location_of_rootdir = fs_dir_root->start_block;
        // Number of blocks allocated for the root in the volume
        fs_vcb->root_blocks = retrieve_num_of_blocks(fs_dir_root[0].size, BLOCK_SIZE);

        //printf("root blocks %d\n", fs_vcb->root_blocks);

        // LBAWrite() the VCB to block 0
        if (LBAwrite(fs_vcb, 1, 0) != 1) {
            perror("LBAwrite the VCB to block 0 failed.\n");
            exit (EXIT_FAILURE);
        }
    }

    // At the beginning,current dir is root dir
    fs_dir_curr = fs_dir_root;

    return 0;
}
	
void exitFileSystem () {
	printf (C_PROMPT "\nSystem exiting\n" C_RESET);

	// Ensure that the Volume Control Block (VCB) is written to disk.
	if (LBAwrite(fs_vcb, 1, 0) != 1) {
		perror("LBAwrite failed when trying to write the VCB.\n");
	}

	// Ensure that the free space information is written to disk.
	if (LBAwrite(fs_freespace,fs_vcb->num_of_freespace_blocks,1) != fs_vcb->num_of_freespace_blocks) {
		perror("LBAwrite failed to write the freespace.");
	}

	free_memory();
}