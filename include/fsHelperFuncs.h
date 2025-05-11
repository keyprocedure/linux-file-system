/**************************************************************
* Contains the helper functions prototype for the file system
**************************************************************/
#include "mfs.h"

int retrieve_num_of_blocks(int bytes, int block_size);
void set_bit(unsigned char* fs_freespace, int block_num);
void clear_bit(unsigned char* fs_freespace, int block_num);
int get_bit(unsigned char* fs_freespace, int block_num);
void free_memory();
int parse_path(char* path_name, struct parse_path_return_data* parse_path_info);
int get_DE_index(DirectoryEntry* dir_array, char* token);
int get_available_DE_index(DirectoryEntry* dir_array);
int is_DE_a_directory(DirectoryEntry* dir);
int is_DE_exist(DirectoryEntry *parent, char *name);
void free_directory(DirectoryEntry* dir);
