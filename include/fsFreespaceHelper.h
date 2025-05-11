/**************************************************************
* Contains the helper function prototypes for the free space
**************************************************************/
#include "mfs.h"

int calculate_number_of_FAT_blocks(uint64_t numberOfBlocks, uint64_t blockSize);
int allocation_validity_checks(int requested_block_count);
int clear_validity_checks(int start_block);
int get_next_block(int current_block, int current_size);