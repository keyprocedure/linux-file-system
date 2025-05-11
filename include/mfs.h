/**************************************************************
* Interface needed by the driver to interact with th filesystem
**************************************************************/
#ifndef _MFS_H
#define _MFS_H
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "b_io.h"

#include <dirent.h>
#define FT_REGFILE	DT_REG
#define FT_DIRECTORY DT_DIR
#define FT_LINK	DT_LNK

#ifndef uint64_t
typedef u_int64_t uint64_t;
#endif
#ifndef uint32_t
typedef u_int32_t uint32_t;
#endif

#define MAX_DIR_ENTRIES 50
#define BLOCK_SIZE 512       // Size of a single block in bytes
#define MAX_NAME_SIZE 20     // The maximum sizeof the file/directory name
#define MAX_FILE_SIZE 100000 // File size limit (100,000 bytes)

// Used for b_seek behaviors
#define B_SEEK_START 0 // Seek to the beginning of the file
#define B_SEEK_CUR 1   // Seek to the current file offset
#define B_SEEK_END 2   // Seek to the end of the file

// Define file types for clearer code
typedef enum {
    FILE_TYPE_REGULAR = 0,
    FILE_TYPE_DIRECTORY = 1
} FileType;

// Structure for directory entry
typedef struct {
	char name[256];           // Directory name
	size_t size;              // File size in bytes
	int start_block;          // Start block of the file/directory in the filesystem
	FileType is_dir;          // Indicates if the entry is a directory                   
	time_t creation_time;     // Time of creation
	time_t modification_time; // Time of last modification
	time_t access_time;       // Time of last access
} DirectoryEntry;

// This is the Volume Control Block struct for the file system
typedef struct {
	char volume_name[100]; 					// volume name
	long signature; 						// unique VCB identifier
	int num_blocks; 						// number of blocks in the file system
	int size_of_blocks; 					// size of each block in the file system
	int freespace_start; 					// location of the first block of the freespace
	int first_free_block_in_freespace_map;  // stores the location of the first free block
	int num_of_available_freespace_blocks;  // number of available freespace blocks
	int num_of_freespace_blocks; 			// number of freespace blocks
	int location_of_rootdir; 				// location of root directory
	int root_blocks; 						// number of blocks root dir occupies
} VCB;

struct parse_path_return_data{
	DirectoryEntry* parent;
	int last_element_index;
	char* last_element_name;
};

// This structure is returned by fs_readdir to provide the caller with information
// about each file as it iterates through a directory
struct fs_diriteminfo {
    unsigned short d_reclen; // length of this record
    unsigned char fileType;    
    char d_name[256]; 	     // filename max filename is 255 characters */
};

// This is a private structure used only by fs_opendir, fs_readdir, and fs_closedir
// A file descriptor but for a directory - can only read from a directory.  
// This structure helps the file system keep track of
// which directory entry you are currently processing so that everytime the caller
// calls the function readdir, you give the next entry in the directory
typedef struct {
	unsigned short  d_reclen;		  // length of this record
	unsigned short	dirEntryPosition; // which directory entry position, like file pos
	DirectoryEntry * directory;		  // pointer to the loaded directory you want to iterate
	struct fs_diriteminfo * di;		  // Pointer to the structure you return from read
	int number_DE;                    // number of directory entries in the loaded directory
} fdDir;


// Strucutre that is filled in from a call to fs_stat
struct fs_stat {
	off_t     st_size;    	 // total size, in bytes
	blksize_t st_blksize; 	 // blocksize for file system I/O
	blkcnt_t  st_blocks;     // number of 512B blocks allocated
	time_t    st_accesstime; // time of last access
	time_t    st_modtime;    // time of last modification
	time_t    st_createtime; // time of last status change
};

extern VCB *fs_vcb; // Volume Control Block
extern unsigned short *fs_freespace; // Freespace map
extern DirectoryEntry* fs_dir_root; // Root directory
extern DirectoryEntry* fs_dir_curr; // Current directory

int fs_stat(const char *path, struct fs_stat *buf);

// Key directory functions
int fs_mkdir(const char *pathname, mode_t mode);
int fs_rmdir(const char *pathname);

// Directory iteration functions
fdDir * fs_opendir(const char *pathname);
struct fs_diriteminfo *fs_readdir(fdDir *dirp);
int fs_closedir(fdDir *dirp);

// Misc directory functions
char * fs_getcwd(char *pathname, size_t size);
int fs_setcwd(char *pathname);  // Linux chdir
int fs_isFile(char * filename);	// Return 1 if file, 0 otherwise
int fs_isDir(char * pathname);  // Return 1 if directory, 0 otherwise
int fs_delete(char* filename);	// Removes a file
int remove_directory_entry(const char* filename);

#endif