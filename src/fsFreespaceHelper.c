/**************************************************************
* Contains helper functions for the free space
**************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "../include/fsFreespaceHelper.h"
#include "../include/fsFreespace.h"

int calculate_number_of_FAT_blocks(uint64_t number_of_blocks, uint64_t block_size) {
    // Each block can hold 256 FAT entries (512 bytes in a block / 2 bytes for an unsigned short)
    int number_of_FAT_entries_per_block = block_size / sizeof(unsigned short);

    // Block size check to avoid division by 0
    if (number_of_FAT_entries_per_block == 0) {
        fprintf(stderr, "Block size too small.\n");
        return -1;
    }

    // In order to track 19,531 blocks, 77 blocks are required for the FAT (19,531 / 256 = 77)
    int number_of_FAT_blocks = (int)number_of_blocks / number_of_FAT_entries_per_block;

    // If there is a remainder in the reserve calculation, reserve an additional block for the FAT
    if (number_of_blocks % number_of_FAT_entries_per_block != 0)
        number_of_FAT_blocks++;

    return number_of_FAT_blocks;
}

int allocation_validity_checks(int requested_block_count) {
    // Confirm the freespace structure is valid
    if (fs_vcb == NULL) {
        fprintf(stderr, "File system control block is invalid.\n");
        return -1;
    }

    // Confirm the freespace map is valid
    if (fs_freespace == NULL) {
        fprintf(stderr, "Freespace map is invalid.\n");
        return -1;
    }

    // Confirm a valid block count
    if (requested_block_count < 1) {
        fprintf(stderr, "Cannot allocate less than 1 block.\n");
        return -1;
    }

    // Confirm there is enough space in the volume for the file
    if (fs_vcb->num_of_available_freespace_blocks < requested_block_count) {
        fprintf(stderr, "Not enough freespace blocks available.\n");
        return -1;
    }

    return 0;
}

int clear_validity_checks(int start_block) {
    // Confirm the freespace structure is valid
    if (fs_vcb == NULL) {
        fprintf(stderr, "File system control block is invalid.\n");
        return -1;
    }

    // Confirm the freespace FAT is valid
    if (fs_freespace == NULL) {
        fprintf(stderr, "Freespace bitmap is invalid.\n");
        return -1;
    }

    // Confirm a valid start block, start block must point to a data block
    if (start_block <= fs_vcb->num_of_freespace_blocks) {
        fprintf(stderr, "Invalid start block.\n");
        return -1;
    }

    return 0;
}

// Retrieve the block location following the location provided
int get_next_block(int current_block, int current_size) {
    // Get the next linked block
    int next_block = fs_freespace[current_block];

    // Check if more blocks need to be allocated
    if (current_block == next_block) {
        // Allocate more blocks and check if it was successful
        if (allocate_more_blocks(current_block, current_size) != 0)
            // If not, return an error indicator
            return -1;

        // Get the next linked block since more blocks have been linked
        next_block = fs_freespace[current_block];
    }

  return next_block;
}