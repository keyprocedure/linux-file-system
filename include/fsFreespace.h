/**************************************************************
* Contains the prototype of the functions for free space operations
**************************************************************/
#include "mfs.h"

int initialize_freespace(uint64_t numberOfBlocks, uint64_t blockSize);
int allocate_freespace(int requested_block_count);
int clear_freespace(int start_block);
int load_freespace();
int allocation_validity_checks(int requested_block_count);
int clear_validity_checks(int start_block);
int allocate_more_blocks(int current_block, int current_size);