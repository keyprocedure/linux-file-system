/**************************************************************
* File System - Key File I/O Operations
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../include/b_io.h"
#include "../include/fsDirectory.h"
#include "../include/fsFreespace.h"
#include "../include/mfs.h"
#include "../include/fsHelperFuncs.h"
#include "../include/fsLow.h"
#include "../include/fsFreespaceHelper.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512
#define DEFAULT_FILE_BLOCKS 20

typedef struct b_fcb {
	DirectoryEntry* fi;        // Holds the low level file system info
	char * buf;		           // Holds the open file buffer
	int buffer_offset;		   // Holds the current position in the buffer
	int buffer_len;		       // Holds how many valid bytes are in the buffer
	int current_block;         // Holds the current block number
	int num_blocks;            // Holds how many blocks the file occupies
	int block_index;		   // Holds the current block index
	int access_mode;           // Holds the file access mode
	int file_index;            // Holds the index of file in dir_array
    bool need_to_write_block;  // Flag to indicate whether the current block needs to be written before being changed
} b_fcb;
	
b_fcb fcbArray[MAXFCBS];
struct parse_path_return_data parse_path_info;
int parent_index;
int startup = 0;  // Indicates that this has not been initialized

// Method to initialize our file system
void b_init () {
	// init fcbArray to free all
	for (int i = 0; i < MAXFCBS; i++) {
		fcbArray[i].buf = NULL; // Indicates a free fcbArray
	}
		
	startup = 1;
}

// Method to get a free FCB element
b_io_fd b_getFCB () {
	for (int i = 0; i < MAXFCBS; i++) {
		if (fcbArray[i].buf == NULL) {
			return i;	
		}
	}
	return (-1);  // All in use
}
	
// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open (char * filename, int flags) {
    if (startup == 0) b_init();  // Initialize system

    // Check if the filename is longer than the maximum size set
    if (strlen(filename) > MAX_NAME_SIZE) {
        fprintf(stderr, "Filename exceeds the maximum length.\n");
        return -1; // Return an error code
    }

	b_io_fd returnFd;

    // Invalid path check
    if (parse_path(filename, &parse_path_info) != 0) {
        return -1;
    }
	int index = parse_path_info.last_element_index;

	// Check invalid cases if file/dir is not found
	if (index < 0) {
		if (flags & O_RDONLY) {
            fprintf(stderr, "Read only: file not found.\n");
			return -2;
		}
		if (!(flags & O_CREAT)) {
            fprintf(stderr, "Create flag is not set, new file cannot be created.\n");
			return -2;
		}
	} else { // Check if DE is a directory
		if (parse_path_info.parent[index].is_dir == FILE_TYPE_DIRECTORY) {
			printf("%s is a directory and can't be opened as a file.\n", 
			parse_path_info.parent[index].name);
			return -2;
		}
	}
	// Allocate memory for the buffer
	char* buf = malloc(B_CHUNK_SIZE);
	if (buf == NULL) {
        fprintf(stderr, "Buffer malloc failed\n");
		return -1;
	}

	returnFd = b_getFCB();  // Get our own file descriptor
	// Check for error - all used FCBs			
	if (returnFd == -1) {
        fprintf(stderr, "No free FCB available.\n");
		return -1;
	}

	// Allocate memory for directory entry
	fcbArray[returnFd].fi = malloc(sizeof(DirectoryEntry));

	if (fcbArray[returnFd].fi == NULL) {
        fprintf(stderr, "Malloc for fileInfo failed.\n");
		return -1;
	}

	// If index > -1, that means file was found. Then load directory entry into
	// fcbArray[returnFd].fi. Otherwise, create new file.
	if (index > -1) {
		memcpy(fcbArray[returnFd].fi, &parse_path_info.parent[index], sizeof(DirectoryEntry));
		fcbArray[returnFd].file_index = index;
        parent_index = index;
	} else {
		int new_file_index = get_available_DE_index(parse_path_info.parent);

		// Check if there's any available DE
		if (new_file_index == -1) {
			return -1; // No available DE
		}

		int start_block = allocate_freespace(DEFAULT_FILE_BLOCKS);

		// Check if there's enough free space available
		if (start_block == -1) {
			return -1; // No free space left
		}

		time_t current_time = time(NULL);

		// Initialize file metadata in the directory entry
		parse_path_info.parent[new_file_index].size = 0;
		parse_path_info.parent[new_file_index].start_block = start_block;
		parse_path_info.parent[new_file_index].is_dir = FILE_TYPE_REGULAR;
		parse_path_info.parent[new_file_index].creation_time = current_time;
		parse_path_info.parent[new_file_index].access_time = current_time;
		parse_path_info.parent[new_file_index].modification_time = current_time;
		strcpy(parse_path_info.parent[new_file_index].name, parse_path_info.last_element_name);

		write_dir(parse_path_info.parent);

		memcpy(fcbArray[returnFd].fi, &parse_path_info.parent[new_file_index], 
		sizeof(DirectoryEntry));
		fcbArray[returnFd].file_index = new_file_index;
        parent_index = new_file_index;
	}

	// Initialize fcbArray entries
	fcbArray[returnFd].buf = buf;
	fcbArray[returnFd].buffer_len = 0;
	fcbArray[returnFd].buffer_offset = 0;
	fcbArray[returnFd].block_index = 0;
	fcbArray[returnFd].current_block = fcbArray[returnFd].fi->start_block;
	fcbArray[returnFd].num_blocks = retrieve_num_of_blocks(fcbArray[returnFd].fi->size, B_CHUNK_SIZE);
	fcbArray[returnFd].access_mode = flags;
    fcbArray[returnFd].need_to_write_block = false;

	// If O_TRUNC is set, truncate the file size to 0
	if (flags & O_TRUNC) {
		fcbArray[returnFd].fi->size = 0;
	}

	return (returnFd);	// All set
	}


// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence) {
	int new_file_pointer; // Variable to hold the new file pointer after seek

	if (startup == 0) b_init();  // Initialize system
	
	// Check if the file is open
    if (fcbArray[fd].fi == NULL)  return -1;

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS)) return -1; // Invalid file descriptor

	// Calculate actual file pointer
	int curr_file_pointer=(fcbArray[fd].block_index * B_CHUNK_SIZE)+fcbArray[fd].buffer_offset;
	
    // Calculate new file pointer based on whence and offset
    switch (whence) {
		// Seek from the start of the file
        case B_SEEK_START:
            new_file_pointer = offset;
            break;
		// Seek from the current position of the file pointer
        case B_SEEK_CUR:
            new_file_pointer = curr_file_pointer + offset;
            break;
		// Seek from the end of file
        case B_SEEK_END:
            new_file_pointer = fcbArray[fd].fi->size + offset;
            break;
        default:
            return -1; // Invalid whence
    }
	// Ensure not to seek to a negative value
	if(new_file_pointer < 0) return -1;

	// Check if the buffer is dirty, if so write it to the volume
	if(fcbArray[fd].need_to_write_block){
		if (LBAwrite(fcbArray[fd].buf,1,fcbArray[fd].current_block) != 1) {
			fprintf(stderr, "LBAwrite failure while writing to the volume\n");
			return -1;
		}
		fcbArray[fd].need_to_write_block = false;
	}

	int new_block_index = (retrieve_num_of_blocks(new_file_pointer, B_CHUNK_SIZE) - 1);
	int new_buff_offset = new_file_pointer % B_CHUNK_SIZE;

	// Update the file descriptor information to match the new location of the file pointer
	// if moving from the current block
	if((new_block_index != fcbArray[fd].block_index) ){
		int temp_curr_block;
		int num_block_to_move;
		
		// If we seek to a block before the current block
		if(new_block_index < fcbArray[fd].block_index){	
			// Start from start_block, then get blocks until the new block index		
			temp_curr_block = fcbArray[fd].fi->start_block;	
			num_block_to_move = new_block_index;
		}
		// If we seek beyond the current block
		else if(new_block_index > fcbArray[fd].block_index){
			// Start from current_block, then get blocks until the new block index
			temp_curr_block = fcbArray[fd].current_block;
			num_block_to_move = new_block_index - fcbArray[fd].block_index;
		}
		// Get the new current block 
		for(int i=0; i< num_block_to_move; i++)
			temp_curr_block = get_next_block(temp_curr_block, fcbArray[fd].fi->size);

		// Change the information of the current block to match the new block
		fcbArray[fd].block_index = new_block_index;
		fcbArray[fd].current_block = temp_curr_block;
		fcbArray[fd].buffer_len = 0;
	}
	// Update the bufffer offset to the new buffer offset
	fcbArray[fd].buffer_offset = new_buff_offset;
	
	return (0); 
}

// Interface to write function
// b_io_fd: file descriptor
// buffer: data to write to file
// count: number of bytes to write
int b_write(b_io_fd fd, char *buffer, int count) {
    // Initialize system
    if (startup == 0)
        b_init();

    // Check that fd is a valid file descriptor
    if (fd < 0 || fd >= MAXFCBS) {
        return -1;
    }

    // Check if file is open
    if (fcbArray[fd].fi == NULL) {
        return -1;
    }

    // Check file write access
    if ((fcbArray[fd].access_mode & O_RDONLY)) {
        fprintf(stderr, "File does not have write access.\n");
        return -1;
    }

    // Track where in the caller buffer to read next
    int caller_buffer_offset = 0;

    // Track the number of bytes written to the volume
    int bytes_written_to_volume = 0;

    // Track the number of bytes copied from the caller's 
	// buffer to the file's buffer, or written to the volume
    int number_of_bytes_moved = 0;

    // Loop while there are bytes to write from the caller's buffer
    while (count > 0) {
        // If the file's buffer is empty and at least BLOCK_SIZE (512) 
		// bytes needs to be written, directly write to the volume
        if (fcbArray[fd].buffer_offset == 0 && count >= BLOCK_SIZE) {
            // Write one block to the volume
            // Check if LBAwrite is successful
            if (LBAwrite(buffer + caller_buffer_offset, 1,fcbArray[fd].current_block) != 1) {
                // Print the error
                fprintf(stderr, "LBAwrite failure while writing to the volume\n");
                // Exit the function
                return bytes_written_to_volume;
            }

            // An entire block was written
            number_of_bytes_moved = BLOCK_SIZE;

            // Move to the next volume block
            int next_block = get_next_block(fcbArray[fd].current_block, fcbArray[fd].fi->size);

            // Check if more blocks were allocated
            if (next_block == -1) {
                // Print the error
                fprintf(stderr, "Failed to allocate more blocks.\n");
                // Exit the function
                return bytes_written_to_volume;
            }

            // Assign the next block
            fcbArray[fd].current_block = next_block;

            // Increment the number of blocks written to
            fcbArray[fd].block_index++;
        }
        // Can't directly write a block from the buffer to the volume, 
		// write a portion of a block
        else {
            // Check if the file's buffer is empty
            if (fcbArray[fd].buffer_offset == 0) {
                // Load the current block to the buffer
                // Check if LBAread is successful
                if (LBAread(fcbArray[fd].buf,1,fcbArray[fd].current_block) != 1) {
                    // Print the error
                    fprintf(stderr, "LBAread failure while reading from the volume\n");
                    // Exit the function
                    return bytes_written_to_volume;
                }
            }

            // Calculate the number of bytes to copy to the current buffer
            number_of_bytes_moved = BLOCK_SIZE - fcbArray[fd].buffer_offset;

            // Check if the number of bytes left to copy are less than the remaining size in the buffer
            if (count < number_of_bytes_moved)
                // If so, track the smaller value
                number_of_bytes_moved = count;

            // Copy from the caller's buffer to the file's buffer
            memcpy(fcbArray[fd].buf + fcbArray[fd].buffer_offset,buffer + caller_buffer_offset, 
			number_of_bytes_moved);

            // Increment the file pointer offset
            fcbArray[fd].buffer_offset += number_of_bytes_moved;

            // Set the flag indicating that the buffer needs to be written (for seek and close)
            fcbArray[fd].need_to_write_block = true;

            // Check if the file's buffer is full
            if (fcbArray[fd].buffer_offset == BLOCK_SIZE) {
                // Write the file's buffer to the volume
                // Check if LBAwrite is successful
                if (LBAwrite(fcbArray[fd].buf,1,fcbArray[fd].current_block) != 1) {
                    // Print the error
                    fprintf(stderr, "LBAwrite failure while writing to the volume\n");
                    // Exit the function
                    return bytes_written_to_volume;
                }

                // Reset the flag, since this block was just written
                fcbArray[fd].need_to_write_block = false;

                // Set the file pointer offset to 0 to indicate an empty block
                fcbArray[fd].buffer_offset = 0;

                // Move to the next volume block
                int next_block = get_next_block(fcbArray[fd].current_block, fcbArray[fd].fi->size);

                // Check if more blocks were allocated
                if (next_block == -1) {
                    // Print the error
                    fprintf(stderr, "Failed to allocate more blocks.\n");
                    // Exit the function
                    return bytes_written_to_volume;
                }

                // Assign the next block
                fcbArray[fd].current_block = next_block;

                // Increment the number of blocks written to
                fcbArray[fd].block_index++;
            }
        }

        // Increment the caller buffer index
        caller_buffer_offset += number_of_bytes_moved;

        // Track the number of bytes written to the volume
        bytes_written_to_volume += number_of_bytes_moved;

        // Decrement the number of bytes that need to be written
        count -= number_of_bytes_moved;
    }

    // Calculate the last position written
    int last_position_written = fcbArray[fd].block_index * BLOCK_SIZE + fcbArray[fd].buffer_offset;

    // Update the file size the number of blocks used by the file
    if (last_position_written > fcbArray[fd].fi->size) {
        fcbArray[fd].fi->size = last_position_written;
        fcbArray[fd].num_blocks = retrieve_num_of_blocks(fcbArray[fd].fi->size, BLOCK_SIZE);
    }

    // Update access and modification time to the current time
    fcbArray[fd].fi->access_time = time(NULL);
    fcbArray[fd].fi->modification_time = time(NULL);

    return bytes_written_to_volume;
}

// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count) {
	int blocks_read;
	int bytes_returned;
	int part1, part2, part3;
	int number_of_blocks_to_copy;
	int remaining_bytes_in_my_buf;

	if (startup == 0) b_init(); // Initialize system

	// Check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS)) {
		return (-1); // Invalid file descriptor
	}

	// Check if file is open
	if (fcbArray[fd].fi == NULL) {
		return -1;
	}

	if (fcbArray[fd].access_mode & O_WRONLY) {
        fprintf(stderr, "File does not have read access.\n");
		return -1;
	}

	// Number of available bytes to copy from buffer
	remaining_bytes_in_my_buf = fcbArray[fd].buffer_len - fcbArray[fd].buffer_offset;

	// Limit count to file length;
	int amount_already_delivered = 
	(fcbArray[fd].block_index * B_CHUNK_SIZE) - remaining_bytes_in_my_buf;
	if ((count + amount_already_delivered) > fcbArray[fd].fi->size) {
		count = fcbArray[fd].fi->size - amount_already_delivered;

		if (count < 0) {
			printf("Error: Count: %d   Delivered: %d   Current Block: %d",
			count, amount_already_delivered, fcbArray[fd].current_block);
			return 0; // End of file
		}
	}
	// Enough bytes in buffer to copy
	if (remaining_bytes_in_my_buf >= count) {
		part1 = count;
		part2 = 0;
		part3 = 0;
	} else {
		// part1 will contain what's left in the buffer
		part1 = remaining_bytes_in_my_buf;
		// part1 is not enough, set part3 to how much more bytes needed
		part3 = count - remaining_bytes_in_my_buf;
		// Calculate how many 512 byte chunks needed to copy
		// to the caller's buffer
		number_of_blocks_to_copy = part3 / B_CHUNK_SIZE;
		// part2 contains how many bytes to give to the caller's buffer
		part2 = number_of_blocks_to_copy * B_CHUNK_SIZE;

		// Reduce part3 by the number of bytes that can be copied in chunks
		// part3 is the remainder. It has to be less than block size
		part3 = part3 - part2;
	}
	// Copy part1 bytes to caller's buffer
	if (part1 > 0) {
		memcpy(buffer, fcbArray[fd].buf + fcbArray[fd].buffer_offset, part1);
		fcbArray[fd].buffer_offset += part1;
	}
	// Blocks to copy direct to caller's buffer
	if (part2 > 0) {
		blocks_read = 0;
		for (int i = 0; i < number_of_blocks_to_copy; i++) {
			blocks_read = LBAread(buffer + part1 + (i * B_CHUNK_SIZE), 1,
						  fcbArray[fd].current_block);
			fcbArray[fd].current_block = get_next_block(fcbArray[fd].current_block, fcbArray[fd].fi->size);
		}
		fcbArray[fd].block_index += blocks_read;
		part2 = blocks_read * B_CHUNK_SIZE;
	}
	// Buffer is empty, part3 is less than 512 bytes
	if (part3 > 0) {
		// LBAread the remaining block into the my buffer
		blocks_read = LBAread(fcbArray[fd].buf, 1, fcbArray[fd].current_block);
		blocks_read = blocks_read * B_CHUNK_SIZE;

		fcbArray[fd].current_block = get_next_block(fcbArray[fd].current_block, fcbArray[fd].fi->size);
		fcbArray[fd].block_index += 1;
		fcbArray[fd].buffer_offset = 0; // Reset buffer offset
		fcbArray[fd].buffer_len = blocks_read;

		if (blocks_read < part3) {
			part3 = blocks_read;
		}
		// Memcpy part3 bytes
		if (part3 > 0) {
			memcpy(buffer + part1 + part2, fcbArray[fd].buf + fcbArray[fd].buffer_offset,
			part3);
			fcbArray[fd].buffer_offset += part3; // Adjust buffer offset
		}
	}

	fcbArray[fd].fi->access_time = time(NULL); // Set access time to current time
	bytes_returned = part1 + part2 + part3;

	return bytes_returned;
}

int b_move(char* source_file_name, char* destination_file_name) {
	struct parse_path_return_data pp_info_src_file;
	struct parse_path_return_data pp_info_dest_file;
	DirectoryEntry* destination_dir;

	// Invalid source path check
    if (parse_path(source_file_name, &pp_info_src_file) != 0) {
        fprintf(stderr, "invalid source path.\n");
        return -1;
    }

	// Invalid destination path check
    if (parse_path(destination_file_name, &pp_info_dest_file) != 0) {
        fprintf(stderr, "invalid destination path.\n");
        return -1;
    }

	int source_file_index = pp_info_src_file.last_element_index;
	if (source_file_index < 0) {
        fprintf(stderr, "Source file or directory not found.\n");
        return -1;
    }

	int destination_file_index = pp_info_dest_file.last_element_index;

	// Last element exists
	if(destination_file_index > 0){
		// Last element is a file
		if (pp_info_dest_file.parent[destination_file_index].is_dir == FILE_TYPE_REGULAR) {
            fprintf(stderr, "Cannot move to a file.\n");
			return -1;
		}
		// Last element is a directory
		// Load the last element(the destination directory)
		destination_dir = load_dir(&pp_info_dest_file.parent[destination_file_index]);

		// Check if the destination dir has a file or directory with the same name as source
		int name_exist=is_DE_exist(destination_dir,pp_info_src_file.last_element_name);
		if(name_exist == 0){ // Directory has file or directory of source name
            fprintf(stderr, "Destination file/directory with this name already exists.\n");
			return -1;
		}
	}
	// Last element doesn't exist
	else if(destination_file_index == -1){
		// Check if destination parent has a file or directory with the same name as source
		int name_exist=is_DE_exist(pp_info_dest_file.parent, pp_info_dest_file.last_element_name);
			if(name_exist == 0){ // Directory has file or directory of source name
                fprintf(stderr, "Destination file/directory with this name already exists.\n");
			return -1;
		}
		// Rename the source as last elements name
		strcpy(pp_info_src_file.last_element_name, pp_info_dest_file.last_element_name);
		destination_dir = pp_info_dest_file.parent;//the directory destination
	}

	// Get the next available DE in destination directory
	int new_destination_index = get_available_DE_index(destination_dir);
	if (new_destination_index == -1) {
		return -1; // No available DE index left
	}

	// Copy the src dir entry to the dest dir entry
	strcpy(destination_dir[new_destination_index].name, pp_info_src_file.last_element_name);
	destination_dir[new_destination_index].size = 
	pp_info_src_file.parent[source_file_index].size;
	destination_dir[new_destination_index].start_block = 
	pp_info_dest_file.parent[source_file_index].start_block;
	destination_dir[new_destination_index].is_dir = 
	pp_info_dest_file.parent[source_file_index].is_dir;
	destination_dir[new_destination_index].creation_time = 
	pp_info_src_file.parent[source_file_index].creation_time;
	destination_dir[new_destination_index].access_time = 
	pp_info_src_file.parent[source_file_index].access_time;
	destination_dir[new_destination_index].modification_time = 
	pp_info_src_file.parent[source_file_index].modification_time;

	// Update changes to disk
	write_dir(destination_dir);

	// Reset the source dir entry 
	strcpy(pp_info_src_file.parent[source_file_index].name, "");
    pp_info_src_file.parent[source_file_index].size = 0;
    time_t actual_time = time(NULL);
    pp_info_src_file.parent[0].modification_time = actual_time;
    pp_info_src_file.parent[0].access_time = actual_time;

    // Update changes to disk
    write_dir(pp_info_src_file.parent);

	return 0;

}	
// Interface to close the file	
int b_close (b_io_fd fd) {
	// Check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS)) {
		return (-1); // Invalid file descriptor
	}

    // Check if the current block needs to be written
    if (fcbArray[fd].need_to_write_block == true) {
        // Write the file's buffer to the volume
        // Check if LBAwrite is successful
        if (LBAwrite(fcbArray[fd].buf,1,fcbArray[fd].current_block) != 1)
            // Print the error
            fprintf(stderr, "LBAwrite failure while writing to the volume\n");
    }

    parse_path_info.parent[parent_index] = *(fcbArray[fd].fi);
    write_dir(parse_path_info.parent);

	// Free allocated memory
	free(fcbArray[fd].fi);
	fcbArray[fd].fi = NULL;
	free(fcbArray[fd].buf);
	fcbArray[fd].buf = NULL;

	return 0;
}
