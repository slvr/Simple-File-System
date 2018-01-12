#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

#define MAX_FILE_NAME 20
#define MAX_EXTENSION_NAME 3
#define NUM_BLOCKS 1024

typedef struct{
    uint64_t magic;
    uint64_t blockSize;
    uint64_t fsSize;
    uint64_t inodeTableLen;
    uint64_t rootDirInode;
} superblock;

typedef struct{
    unsigned int used; //A flag to determine if it's being used
//    unsigned int mode; //Reducing size of inode struct. I don't see how this is useful
    unsigned int link_cnt;
//    unsigned int uid;
//    unsigned int gid;
    unsigned int size;
    unsigned int dataPointers[12];
    unsigned int indirectPointer; // points to a data block that points to other data blocks (Single indirect)
} inode;

/*
 * inodeIndex    which inode this entry describes
 * inode  pointer towards the inode in the inode table
 *rwptr    where in the file to start   
 */
typedef struct{
    uint64_t iNodeIndex;
    inode* inode; // 
    uint64_t rwptr;
} fileDescriptor;


typedef struct{
    int iNode; // represents the inode number of the entery. 
    char *name; // represents the name of the entery. 
}directoryTbl;


void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);

#endif //_INCLUDE_SFS_API_H_
