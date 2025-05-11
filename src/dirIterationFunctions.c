/**************************************************************
* Contains the directory iteration functions 
* fs_opendir(), fs_readdir(), and fs_closedir()
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

   
fdDir * fs_opendir(const char *pathname) {
    struct parse_path_return_data parse_path_info;
    // Invalid path
	if (parse_path((char*) pathname, &parse_path_info) != 0) 
		return NULL; 

    int index = parse_path_info.last_element_index;
    // Last element doesn't exist, so you can't open it
    // or last element is a file, so we can't open it with fs_opendir
	if (index < 0 || parse_path_info.parent[index].is_dir != FILE_TYPE_DIRECTORY){
        free_directory(parse_path_info.parent);
        return NULL;
    }

    // Load the directory to iterate
    DirectoryEntry* dir = load_dir(&parse_path_info.parent[index]);
      
    // Allocate memory for the directory descriptor 
    fdDir * fd_dir = malloc(sizeof(fdDir));
    if (fd_dir == NULL) {
        printf("Memory allocation for directory descriptor failed\n");
        return NULL;
    }

    // Allocate memory to dir info struct that will be returned from read
    struct fs_diriteminfo * dir_info = malloc(sizeof(struct fs_diriteminfo));
    if (dir_info == NULL) {
        printf("Memory allocation for directory info failed\n");
        return NULL;
    }
    
    // Initialize fd_dir
    fd_dir->d_reclen = sizeof(fdDir);
    fd_dir->dirEntryPosition = 0;
    fd_dir->directory = dir;
    fd_dir->di = dir_info;
    fd_dir->number_DE = fd_dir->directory->size / sizeof(DirectoryEntry);

    free_directory(parse_path_info.parent);

    return fd_dir;
}


struct fs_diriteminfo *fs_readdir(fdDir *dirp) {
    // Verify if dirp is valid
    if((dirp == NULL) ||(dirp->directory == NULL) || (dirp->dirEntryPosition < 0)
        || dirp->dirEntryPosition >= dirp->number_DE || (dirp->di == NULL)){
        return NULL;
    }

    // Read the next available DE
    // keep incrementing the position of DE untile reaching an available DE
    // check each time if at end of directory
    while((dirp->dirEntryPosition) < (dirp->number_DE)){
        if(strcmp(dirp->directory[dirp->dirEntryPosition].name, "") != 0)
            break;
        dirp->dirEntryPosition++;
    }
    // Struct to be returned by fs_readdir
    struct fs_diriteminfo *read_info = dirp->di;

    // Copy the name of the directory to read_info structure
    strcpy(read_info->d_name, dirp->directory[dirp->dirEntryPosition].name);

    // Copy the file type of the directory to read_info structure
    if(dirp->directory[dirp->dirEntryPosition].is_dir == FILE_TYPE_DIRECTORY)
        read_info->fileType = FT_DIRECTORY;
    else
        read_info->fileType = FT_REGFILE;
    
    // Increment the position to point to the next DE
    dirp->dirEntryPosition++;

    return read_info;   
}

int fs_closedir(fdDir *dirp) {
    if (dirp == NULL) {
        fprintf(stderr, "fs_closedir() failed, fdDir is NULL.\n");
        return 1;
    }
    if(dirp->di != NULL){
        free(dirp->di);
        dirp->di = NULL;
    }
    if(dirp->directory != NULL){
        free(dirp->directory);
        dirp->directory = NULL;
    }
    free(dirp);
    dirp = NULL;

    return 0;
}