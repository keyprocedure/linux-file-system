/**************************************************************
* Contains the functions for free space map operations
**************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/fsFreespace.h"
#include "../include/mfs.h"
#include "../include/fsLow.h"
#include "../include/fsHelperFuncs.h"
#include "../include/fsFreespaceHelper.h"

#define C_TITLE   "\x1b[35m"
#define C_VALUE   "\x1b[36m"
#define C_LABEL   "\x1b[32m"
#define C_RESET   "\x1b[0m"

/*
 * Each FAT entry's value is either 0, indicating that it is free, or the number of the next
 * data block that's associated with the file/directory. 
 * The end of a sequence of blocks is detected if the FAT entry's value is equal to its own index.
 *
 * For example, a file/directory with a start_block of 1 would occupy block 1, the next associated block
 * is 3, then 4, and finally 5
 *
 * FAT:
 * [0]: 99
 * [1]: 3
 * [2]: 219
 * [3]: 4
 * [4]: 5
 * [5]: 5
 *
 * Assuming a maximum volume size of 10,000,000 bytes and a block size of 512 bytes = 19,531 blocks
 * fs_freespace can track up to 65,535 blocks (the maximum value of an unsigned short (2 bytes))
 * If the maximum volume size becomes greater or the block size becomes smaller, the
 * data type used to initialize fs_freespace may need to be changed to an unsigned int (4 bytes)
 *
 * Convert hex to decimal to read hexdump values
 *
 * Memory for the freespace is allocated each time initialize_freespace() is called
 * If the file system has previously been initialized, freespace is loaded from the volume
 * Otherwise the freespace is initialized
 */

int initialize_freespace(uint64_t numberOfBlocks, uint64_t blockSize) {
    extern long MAGIC_NUMBER;
    // Get number of FAT blocks required to track freespace
    int number_of_FAT_blocks = calculate_number_of_FAT_blocks(numberOfBlocks, blockSize);
    int number_of_FAT_entries_per_block = fs_vcb->size_of_blocks / 2;

    // Allocate memory for the freespace array
    fs_freespace = (unsigned short*)malloc(number_of_FAT_blocks * blockSize / 2 * sizeof(unsigned short));

    // Check that memory was successfully allocated for the freespace
    if (fs_freespace == NULL) {
        fprintf(stderr, "Memory allocation failed for freespace.\n");
        return -1;
    }

    printf(
      C_TITLE
      "+========================================+\n"
      "|       CUSTOM FILE SYSTEM READY"  "         |\n"
      "+========================================+\n\n"
      C_RESET);

    // If the file system is initialized, load freespace to memory
    if (fs_vcb->signature == MAGIC_NUMBER) {
        load_freespace();
        //printf(C_LABEL "Freespace loaded.\n" C_RESET);
    }
    // Else initialize the freespace
    else {
        // Set all freespace blocks to 0
        memset(fs_freespace, 0, number_of_FAT_blocks * number_of_FAT_entries_per_block * sizeof(unsigned short));

        // Reserve space for the VCB and the FAT in the freespace
        for (int i = 0; i <= number_of_FAT_blocks; i++) {
            fs_freespace[i] = 1;
        }

        fs_vcb->num_of_freespace_blocks = number_of_FAT_blocks;
        // 77 blocks for the FAT + 1 block for the VCB = 78
        fs_vcb->first_free_block_in_freespace_map = number_of_FAT_blocks + 1;
        // Total blocks in volume - 77 blocks reserved for the FAT - 1 block reserved for the VCB
        fs_vcb->num_of_available_freespace_blocks = numberOfBlocks - number_of_FAT_blocks - 1;
        // 1 is the starting block number of the FAT
        fs_vcb->freespace_start = 1;

        // Write freespace to disk
        if (LBAwrite(fs_freespace, number_of_FAT_blocks, 1) != number_of_FAT_blocks) {
            fprintf(stderr, "LBAwrite failed to execute.\n");
            free(fs_freespace);
            fs_freespace = NULL;
            return -1;
        }

        printf(C_TITLE "+ Volume Info\n" C_RESET);
        printf("  Blocks            : " C_VALUE "%ld\n" C_RESET, numberOfBlocks);
        printf("  FAT Blocks        : " C_VALUE "%d\n"  C_RESET, fs_vcb->num_of_freespace_blocks);
        printf("  Free Blocks       : " C_VALUE "%d\n"  C_RESET, fs_vcb->num_of_available_freespace_blocks);
        printf("  First Free Block  : " C_VALUE "%d\n\n" C_RESET, fs_vcb->first_free_block_in_freespace_map);
    }

    return 0;
}

// Allocate entries in the FAT, linking a sequence of entries similar to a linked list
int allocate_freespace(int requested_block_count) {
    // Confirm structures and parameters are valid
    allocation_validity_checks(requested_block_count);

    // Check that there are enough free blocks available
    if (fs_vcb->num_of_available_freespace_blocks < requested_block_count) {
        fprintf(stderr, "Not enough free space remaining.\n");
        return -1;
    }

    // Check that the size requested is less than the maximum file size
    if (requested_block_count * BLOCK_SIZE > MAX_FILE_SIZE) {
        fprintf(stderr, "Unable to allocate blocks, greater than size allowed: %d.\n", MAX_FILE_SIZE);
        return -1;
    }

    // Index to iterate through the FAT entries
    int fs_index = fs_vcb->first_free_block_in_freespace_map;

    // Track previous block index for linking next block in the FAT
    int prev_entry_index = -1;

    // Track first block allocated for the current file/directory
    int start_block = -1;

    // Update the number of available blocks in the freespace
    fs_vcb->num_of_available_freespace_blocks -= requested_block_count;

    // Iterate through the FAT while requested_block_count > 0
    while (fs_index < fs_vcb->num_blocks && requested_block_count > 0) {
        // If the current block is free
        if (fs_freespace[fs_index] == 0) {
            // If this is the first block in the sequence of blocks
            if (prev_entry_index == -1) {
                // Track the first block allocated for this file/directory
                start_block = fs_index;
                // Track the current block index to link in the next iteration
                prev_entry_index = fs_index;
            }

            // Link previous block to the current block
            fs_freespace[prev_entry_index] = fs_index;
            prev_entry_index = fs_index;

            // Decrement the number of blocks remaining to be allocated for this file/directory
            requested_block_count--;
        }

        fs_index++;
    }

    // Set the value of the last FAT entry in the sequence to itself to indicate end of the sequence
    fs_freespace[fs_index - 1] = fs_index - 1;

    // Write the FAT to the volume
    if (LBAwrite(fs_freespace, fs_vcb->num_of_freespace_blocks, 1) !=
        fs_vcb->num_of_freespace_blocks) {
        fprintf(stderr, "LBAwrite failed to write the FAT correctly after allocating.\n");
        return -1;
    }

    // Find the next free block by iterating through the rest of the FAT
    while (fs_index < fs_vcb->num_blocks) {
        // If the block is free
        if (fs_freespace[fs_index] == 0) {
            // Update the freespace struct
            fs_vcb->first_free_block_in_freespace_map = fs_index;
            break;
        }

        fs_index++;
    }

    // Return the starting block of the allocated space
    return start_block;
}

// Clear the freespace FAT entries for the data beginning at start_block
int clear_freespace(int start_block) {
    // Confirm structures and parameters are valid
    clear_validity_checks(start_block);

    int current_block = start_block;

    // Reassign the first free block variable if the start block of the data being removed is
    // located at an earlier point
    if (start_block < fs_vcb->first_free_block_in_freespace_map)
        fs_vcb->first_free_block_in_freespace_map = start_block;

    // Iterate through the connected FAT entries, setting each entry to 0, indicating that it's free
    while (fs_freespace[current_block] != 0)  {
        // If the last FAT entry in this sequence is found, clear it and break out of the while loop
        if (fs_freespace[current_block] == current_block) {
            fs_freespace[current_block] = 0;
            break;
        }

        // Temporarily store the next block
        int next_block = fs_freespace[current_block];

        // Clear the current FAT entry
        fs_freespace[current_block] = 0;

        // Assign the curren_block for the next iteration
        current_block = next_block;
    }

    // Write the FAT to the volume
    if (LBAwrite(fs_freespace, fs_vcb->num_of_freespace_blocks, 1) !=
        fs_vcb->num_of_freespace_blocks) {
        fprintf(stderr, "LBAwrite failed to write the FAT correctly after clearing.\n");
        return -1;
    }

    return 0;
}

// Loads the freespace map from the volume into memory
int load_freespace() {
    // Read the FAT from the volume
    if (LBAread(fs_freespace, fs_vcb->num_of_freespace_blocks, 1) !=
            fs_vcb->num_of_freespace_blocks) {
        fprintf(stderr, "Freespace failed to load from the volume.\n");
        return -1;
    }

    return 0;
}

// Allocate additional memory and extend the current chain in the FAT
int allocate_more_blocks(int current_block, int current_size) {
    if (current_size >= MAX_FILE_SIZE) {
        fprintf(stderr, "Unable to allocate more blocks, at maximum size: %d\n", MAX_FILE_SIZE);
        return -1;
    }

    // If the current_block isn't at the end of the chain, iterate to find the end
    while (current_block != fs_freespace[current_block]) {
        current_block = fs_freespace[current_block];
    }

    // Allocate 10 blocks
    int next_start_block = allocate_freespace(5);

    // Check if additional blocks were allocated
    if (next_start_block == -1)
        // If not, return an error indicator
        return -1;

    // Link the current chain with the newly allocated chain
    fs_freespace[current_block] = next_start_block;

    return 0;
}