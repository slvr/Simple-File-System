/*
Alec Parent - 260688035
COMP 310 - Prof. Rola Harmouche
Fall 2017 - Final Assignment
*/

#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include "disk_emu.h"
#define PARENT_ALEC_DISK "sfs_disk.disk"
#define NUM_BLOCKS 1024  //maximum number of data blocks on the disk.
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows. 
#define MAX_FILE_NAME_LENGTH 20
#define BLOCK_SIZE 1024
#define INODE_CNT 100
#define BLOCKS_MINUS_BITMAP (NUM_BLOCKS-BITMAP_ROW_SIZE-1)
#define WRITING_BLOCKS_START (NUM_BLOCKS-BLOCKS_MINUS_BITMAP)
#define INODE_TABLE_LENGTH ((sizeof(inode)*INODE_CNT)/BLOCK_SIZE)+1

/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)

//Define global variables
int FileNumber;
int FileCurrentPosition; //used in sfs_getnextfilename
int directoryTblBlocks;
inode iNodeTbl[INODE_CNT]; //the iNode table
directoryTbl directory[INODE_CNT]; 
fileDescriptor fileDescriptorTable[INODE_CNT];
superblock sb;

//initialize all bits to high
uint8_t freeBitMap[BITMAP_ROW_SIZE] = { [0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX };

void fileNoUpdate(){
	int i;
	for(i = 1; i < INODE_CNT; i++){ //start at 1 since 0 contains root
		if(iNodeTbl[i].used == 1){
			FileNumber++;
		}
	}
}

void mksfs(int fresh) {
	int i;
	if(fresh){ //initializing the superblock data
		for(i = 0; i < INODE_CNT; i++){ //initialize fdt
			fileDescriptorTable[i].iNodeIndex = -1;
			fileDescriptorTable[i].rwptr = 0;
		}
		
		for(i = 0; i < INODE_CNT; i++){ //initialize inode table
			iNodeTbl[i].used = 0;
			iNodeTbl[i].link_cnt = 0;
		}
		
		sb.magic = 0xACBD0005; //superblock initialization
		sb.blockSize = BLOCK_SIZE;
		sb.inodeTableLen = INODE_TABLE_LENGTH;
		sb.fsSize = NUM_BLOCKS*BLOCK_SIZE;
		sb.rootDirInode = 0;
		
		init_fresh_disk(PARENT_ALEC_DISK, BLOCK_SIZE, NUM_BLOCKS); //fresh disk initialization

		write_blocks(0, 1, &sb); //write superblock
		
		force_set_index(0); //occupy superblock in bitmap

		write_blocks(1, sb.inodeTableLen, &iNodeTbl); //write inodetable
		
		for(i = 1; i < sb.inodeTableLen+1; i++){  //occupy inodetable
			force_set_index(i);
		}
		
		for(i = 0; i < INODE_CNT; i++){ //fileDescritor table and directory
			directory[i].name = ""; //as long as it's not null
			fileDescriptorTable[i].iNodeIndex = -1; //Use -1 to signify that an inode is empty			
		}
		write_blocks(get_index(), (sizeof(directoryTbl)/BLOCK_SIZE)+1, &directory); //write directory
		
		for(i = get_index(); i < (sizeof(directoryTbl)/BLOCK_SIZE)+1; i++){ //occupy directory
			force_set_index(i);
		}
		
		fileDescriptorTable[0].iNodeIndex = 0; //initialize root
		fileDescriptorTable[0].rwptr = 0;
		
		force_set_index(NUM_BLOCKS - ((sizeof(freeBitMap)/BLOCK_SIZE)+1));  //occupy free bit block
		
		write_blocks((NUM_BLOCKS - ((sizeof(freeBitMap)/BLOCK_SIZE)+1)), ((sizeof(freeBitMap)/BLOCK_SIZE)+1), &freeBitMap); //write bitmap
	}
	else{
		for(i = 0; i < INODE_CNT; i++){ //initialize fdt
			fileDescriptorTable[i].iNodeIndex = -1;
			fileDescriptorTable[i].rwptr = 0;
		}
		
		init_disk(PARENT_ALEC_DISK, BLOCK_SIZE, NUM_BLOCKS); //fetch disk
		
		read_blocks(0, 1, &sb); //read superblock
		
		read_blocks(1, INODE_TABLE_LENGTH, &iNodeTbl); //read inode table
		
		read_blocks((NUM_BLOCKS - ((sizeof(freeBitMap)/BLOCK_SIZE)+1)), ((sizeof(freeBitMap)/BLOCK_SIZE)+1), &freeBitMap); //read bitmap
	
	}
}

int sfs_getnextfilename(char *fname){
	int i;
	FileNumber = 0;
	fileNoUpdate();
	if(FileCurrentPosition == FileNumber){ //all files returned
		FileCurrentPosition = 1; //reset
		return 0;
	}
	if(FileNumber == 0){
		return 0;
	}
	else{
		for(i = 1; i < INODE_CNT; i++){
			if(i==FileCurrentPosition){ //check if current file has been visited
				if(strlen(directory[i].name) > 0){
					strcpy(fname, directory[i].name);
					FileCurrentPosition++;
					return 1;
				}
				FileCurrentPosition++;
			}
		}
	}
	return 0;
}

int sfs_getfilesize(const char* path){
	int i;
	for(i = 0; i < INODE_CNT; i++){
		if(strcmp(path, directory[i].name) == 0){
			return iNodeTbl[directory[i].iNode].size;
		}
	}
	return -1;
}

int sfs_fopen(char *name){
	int i;
	int j;
	fileDescriptor *file;
	if(strlen(name) > MAX_FILE_NAME_LENGTH){ //Check name isn't too long
		return -1;
	}
	for(i = 0; i < INODE_CNT; i++){ //checkif file exists, return descriptor if yes
		char *this = directory[i].name;
		if(!strcmp(name, this)){
			file = &fileDescriptorTable[i];
			int fileInode = file->iNodeIndex;
			if(fileInode > 0){ //Check if file is in table
				return -1;
			}
			else{ //assign file descriptor, return index
				for(j = 1; j < INODE_CNT; j++){
					if(fileDescriptorTable[j].iNodeIndex == -1){ //find an empty file descriptor
						fileDescriptorTable[j].iNodeIndex = i;
						fileDescriptorTable[j].rwptr = sfs_getfilesize(name);
						iNodeTbl[i].used = 1;
						return j;
					}
				}
			}
		}
	}
	for(i = 1; i < INODE_CNT; i++){ //find empty inode
		if(iNodeTbl[i].used == 0){ //empty inode found
			for(j = 1; j < INODE_CNT; j++){ //Find empty file descriptor
				file = &fileDescriptorTable[j];
				int thisInode = file->iNodeIndex;
				if (thisInode == -1){ //put file in fileDescriptorTable
					file->iNodeIndex = i;
					file->rwptr = 0;
					iNodeTbl[i].used = 1; //allocate iNode
					directory[i].name = name; //store name in directory table
					directory[i].iNode = i;
					fileDescriptorTable[j] = *file;
					write_blocks(1, INODE_TABLE_LENGTH, &iNodeTbl); //write iNode and directory tables to disk
					write_blocks(1+INODE_TABLE_LENGTH, (sizeof(directoryTbl)*INODE_CNT)/BLOCK_SIZE+1, &directory);
					return j; //return file descriptor
				}
			}
		}
	}
	return -1;
}

int sfs_fclose(int fileID) {
	if((fileID > INODE_CNT-1) || (fileID < 0)){ //check if file exists and is open
		return -1;
	}
	fileDescriptor *file = &fileDescriptorTable[fileID];
	int thisInode = file->iNodeIndex;
	if(!(thisInode == -1)){
		file->iNodeIndex = -1;
		file->rwptr = 0;
		return 0;
	}
	else{
		return -1;
	}
	return -1;
	
}

int sfs_fread(int fileID, char *buf, int length) {
	fileDescriptor *file = &fileDescriptorTable[fileID];
	int positionbuf = 0;
	int bytesread = 0;
	if(file->iNodeIndex == -1){ //make sure file was opened properly and exists
		return -1;
	}
	while(positionbuf < length){ //read into buffer 1 block at a time
		int targetblock = file->rwptr/BLOCK_SIZE; //find where to read from
		int currentblockpos = file->rwptr%BLOCK_SIZE;
		char buffer[BLOCK_SIZE]; //initiate block to be read
		if(targetblock < 12){ //read into buffer
			read_blocks(iNodeTbl[file->iNodeIndex].dataPointers[targetblock], 1, &buffer);
		}
		if(targetblock >= 12){ //indirect pointer
			read_blocks(iNodeTbl[iNodeTbl[file->iNodeIndex].indirectPointer].dataPointers[targetblock-12], 1, &buffer);
		}
		while(currentblockpos <= BLOCK_SIZE-1 && positionbuf < length/* && file->rwptr < iNodeTbl[file->iNodeIndex].size*/){
			buf[positionbuf] = buffer[currentblockpos];
			currentblockpos++;
			positionbuf++;
			file->rwptr++;
			bytesread++;
		}
	}
	return bytesread;

/*	int i;
	int j;
	fileDescriptor *file = &fileDescriptorTable[fileID];
	if(file->iNodeIndex == -1){ //file was improperly opened
		return -1;
	}
	inode *iNode = &iNodeTbl[file->iNodeIndex];
	int bytesRead = 0;
	int bytesToRead = ((file->rwptr+length) > iNode->size) ? (iNode->size)-(file->rwptr): length;
	char buffer[BLOCK_SIZE];
	int startBlock = file->rwptr/BLOCK_SIZE;
	int endBlock = (file->rwptr+bytesToRead)/BLOCK_SIZE+1;
	for(i = startBlock; i < endBlock; i++){
		int start = (file->rwptr)%BLOCK_SIZE;
		int amountToReadThisBlock = BLOCK_SIZE - start;
		if(bytesToRead < amountToReadThisBlock){
			amountToReadThisBlock = bytesToRead;
		}
		int block;
		if(i < 12){
			block = iNode->dataPointers[i];
		}
		else{
			j = i-12;
			int iNodeArray[(12+BLOCK_SIZE/4)-12];
			read_blocks(iNode->indirectPointer, 1, iNodeArray);
			block = iNodeArray[j];
		}
		read_blocks(block, 1, buffer);
		memcpy((buf+bytesRead), (buffer+start), amountToReadThisBlock);
		bytesRead = bytesRead + amountToReadThisBlock;
		file->rwptr = file->rwptr + amountToReadThisBlock;
		bytesToRead -= amountToReadThisBlock;
	}
	return bytesRead;*/
}

int sfs_fwrite(int fileID, const char *buf, int length) {
/*	int writtenBytes = 0;
	int fsize = 0;
	int i;
	fileDescriptor *file = &fileDescriptorTable[fileID];
	if(file->iNodeIndex == -1){ //ensure it was properly opened
		return -1;
	}
	int positionbuf = 0;
	if(iNodeTbl[file->iNodeIndex].dataPointers[0] > 0){ //if file exists, retrieve its size
		fsize = iNodeTbl[file->iNodeIndex].size;
	}
	while(positionbuf < length){ //write blocks into memory
		int targetBlock = file->rwptr/BLOCK_SIZE; //find desired write block
		int currentBlock = file->rwptr%BLOCK_SIZE; //position in the current block
		int nonemptyBlock = 0; //flag to show if desired block is empty
		char buffer[BLOCK_SIZE]; //block to be written
		if(targetBlock < 12){ //create buffer to write to disk
			if(iNodeTbl[file->iNodeIndex].dataPointers[targetBlock] == 0){ //initialize block if it is empty
				for(i = 0; i < BLOCK_SIZE-1; i++){
					buffer[i] = '\0';
				}
			}
			else{
				read_blocks(iNodeTbl[file->iNodeIndex].dataPointers[targetBlock], 1, &buffer);
				nonemptyBlock = 1;
			}
		}
		if(targetBlock >= 12){ //need indirect pointer
			if(iNodeTbl[file->iNodeIndex].indirectPointer == 0){
				for(i = 0; i < BLOCK_SIZE-1; i++){
					buffer[i] = '\0';
				}
			}
			else{
				read_blocks(iNodeTbl[iNodeTbl[file->iNodeIndex].indirectPointer].dataPointers[targetBlock-12], 1, &buffer);
				nonemptyBlock = 1;
			}
		}
		while(currentBlock <= BLOCK_SIZE-1 && positionbuf < length){ //Take data from buf
			buffer[currentBlock] = buf[positionbuf];
			fsize++;
			file->rwptr++;
			currentBlock++;
			positionbuf++;
			writtenBytes++;
		}
		if(targetBlock < 12){ //now write our data
			if(nonemptyBlock == 1){ //appending to an existing block
				write_blocks(iNodeTbl[file->iNodeIndex].dataPointers[targetBlock], 1, &buffer);
			}
			else{ //find a free block
				for(i = abs(WRITING_BLOCKS_START); i < NUM_BLOCKS; i++){ //look after filled blocks
					if(freeBitMap[i] == 0){ //found a free block
						iNodeTbl[file->iNodeIndex].dataPointers[targetBlock] = i;
						force_set_index(i);
						write_blocks(i, 1, &buffer);
						break;
					}
				}
			}
		}
		else{
			if(nonemptyBlock == 1){
				write_blocks(iNodeTbl[iNodeTbl[file->iNodeIndex].indirectPointer].dataPointers[targetBlock-12], 1, &buffer);
			}
			else{
				for(i = 0; i < INODE_CNT; i++){ //need free inode
					if(iNodeTbl[i].used == 0){
						iNodeTbl[file->iNodeIndex].indirectPointer = i;
						iNodeTbl[i].used = 1;
						for(i = abs(WRITING_BLOCKS_START); i < NUM_BLOCKS; i++){ //look after filled blocks
							if(freeBitMap[i] == 0){
								iNodeTbl[iNodeTbl[file->iNodeIndex].indirectPointer].dataPointers[targetBlock-12] = i;
								force_set_index(i);
								write_blocks(i, 1, &buffer);
								break;
							}
							break;
						}
					}
				}
			}
		}
	}
	iNodeTbl[file->iNodeIndex].size = fsize;
	return writtenBytes;*/
	
	int i;
	int j;
	int k;
	int l;
	fileDescriptor *file = &fileDescriptorTable[fileID];
	if(file->iNodeIndex == -1){ //file was improperly opened
		return -1;
	}
	inode *iNode = &iNodeTbl[file->iNodeIndex];
	int fsize = iNode->size;
	int blocksUsed = (fsize == 0) ? 0:fsize/BLOCK_SIZE+1;
	int blocksMax = (file->rwptr+length)/BLOCK_SIZE+1;
	int blocksNeed = blocksMax - blocksUsed;

	int iNodePtrsUsed = iNode->link_cnt; //This section fetches the blocks to be assigned
	int maxIndex = iNodePtrsUsed + blocksNeed;
	int indirectUsed = (iNodePtrsUsed-12 <= 0) ? 0:iNodePtrsUsed-12;
	if(maxIndex > 12+BLOCK_SIZE/4){ //Return error if too many pointers are needed
		return -1;
	}
	int assignedBlocks[blocksNeed];
	for(i = 0; i < blocksNeed; i++){
		j = get_index();
		force_set_index(j);
		write_blocks((NUM_BLOCKS - ((sizeof(freeBitMap)/BLOCK_SIZE)+1)), ((sizeof(freeBitMap)/BLOCK_SIZE)+1), &freeBitMap); //write bitmap
		assignedBlocks[i] = j;
		if(assignedBlocks[i] < 0 || assignedBlocks[i] > NUM_BLOCKS){
			for(k = 0; k <= i; k++){ //release all blocks if assignment is impossible
				if(assignedBlocks[i] > 0){
					if(assignedBlocks[i] >= 0 && assignedBlocks[i] < NUM_BLOCKS){
						rm_index(assignedBlocks[i]);
						write_blocks((NUM_BLOCKS - ((sizeof(freeBitMap)/BLOCK_SIZE)+1)), ((sizeof(freeBitMap)/BLOCK_SIZE)+1), &freeBitMap); //write bitmap
					}
				}
			}
			return -1;
		}
	}

	k = 0; //Now that we have assigned blocks, set pointers
	for(i = iNodePtrsUsed; i < maxIndex; i++){
		if(i < 12){
			iNode->dataPointers[i] = assignedBlocks[k];
			k++;
		}
		else{
			if(iNode->indirectPointer <= 0){
				j = get_index();
				force_set_index(j);
				write_blocks((NUM_BLOCKS - ((sizeof(freeBitMap)/BLOCK_SIZE)+1)), ((sizeof(freeBitMap)/BLOCK_SIZE)+1), &freeBitMap); //write bitmap
				iNode->indirectPointer = j;
				if(iNode->indirectPointer <  0 || iNode->indirectPointer > NUM_BLOCKS){
					
					for(l = 0; l <= i; l++){ //release all blocks if assignment is impossible
						if(assignedBlocks[i] > 0){
							if(assignedBlocks[l] >= 0 && assignedBlocks[l] < NUM_BLOCKS){
								rm_index(assignedBlocks[l]);
								write_blocks((NUM_BLOCKS - ((sizeof(freeBitMap)/BLOCK_SIZE)+1)), ((sizeof(freeBitMap)/BLOCK_SIZE)+1), &freeBitMap); //write bitmap
							}
						}
					}
					return -1;
				}
			}
			int iNodeArray[(12+BLOCK_SIZE/4)-12];
			read_blocks(iNode->indirectPointer, 1, iNodeArray);
			l = 0;
			while(i < maxIndex){
				iNodeArray[indirectUsed+l] = assignedBlocks[k];
				i++;
				k++;
				l++;
			}
			write_blocks(iNode->indirectPointer, 1, iNodeArray);
		}
	}
	iNode->link_cnt = iNode->link_cnt + blocksNeed;
	write_blocks(1, sb.inodeTableLen, &iNodeTbl); //write inodetable

	int startBlock = (file->rwptr)/BLOCK_SIZE; //Now that we have our allocated blocks, we may begin writing
	int endBlock = blocksMax;
	int bytesWritten = 0;
	int bytesToWrite = length;
	char buffer[BLOCK_SIZE];
	for(i = startBlock; i < endBlock; i++){
		int start = file->rwptr%BLOCK_SIZE;
		int block;
		if(i < 12){
			block = iNode->dataPointers[i];
		}
		else{
			j = i-12;
			int iNodeArray[(12+BLOCK_SIZE/4)-12];
			read_blocks(iNode->indirectPointer, 1, iNodeArray);
			block = iNodeArray[j];
		}
		read_blocks(block, 1, (void *)buffer);
		int toWrite = BLOCK_SIZE - start;
		if(bytesToWrite < toWrite){
			toWrite = bytesToWrite;
		}
		char *anotherBuffer = buffer + start;
		const char *yetAnother = (buf + bytesWritten);
		memcpy(anotherBuffer, yetAnother, toWrite);
		bytesWritten += toWrite;
		iNode->size += toWrite;
		file->rwptr += toWrite;
		bytesToWrite -= toWrite;
		write_blocks(block, 1, (void *)anotherBuffer);
	}
	write_blocks(1, sb.inodeTableLen, &iNodeTbl);
	return bytesWritten;
}

int sfs_fseek(int fileID, int loc) {
/*	if(loc < 0){ //ensure valid location
		return -1;
	}
	fileDescriptor *file = &fileDescriptorTable[fileID];
	int fileSize = iNodeTbl[file->iNodeIndex].size;
	if(loc > fileSize){ //Don't look for something bigger than the space you're looking for it in
		return -1;
	}
	if(file->iNodeIndex > 0){
		file->rwptr = loc;
		return 0;
	}
	return -1;*/

	fileDescriptor *file = &fileDescriptorTable[fileID];
	if(file->iNodeIndex == -1){ //file was improperly opened
		return -1;
	}
	inode *iNode = &iNodeTbl[file->iNodeIndex];
	if(loc >= 0 && loc <= iNode->size){
		file->rwptr = loc;
		return loc;
	}
	return -1;
}

int sfs_remove(char *file) {
	int i;
	int fileID = 0;
//	fileNoUpdate();
	for(i = 0; i < INODE_CNT; i++){ //find directory entry, clear, copy FileID
		if(strcmp(directory[i].name, file) == 0){
			directory[i].name = "";
			fileID = directory[i].iNode;
			directory[i].iNode = -1;
			break;
		}
	}

	fileDescriptor *fileptr = &fileDescriptorTable[fileID];
	if(fileID == 0){
		return -1;
	}
	char buffer[1024];
	for(i = 0; i < BLOCK_SIZE - 1; i++){ //cast empty
		buffer[i] = '\0';
	}
	for(i = 0; i < 11; i++){ //clear pointers
		if(iNodeTbl[fileptr->iNodeIndex].dataPointers[i] > 0){
			rm_index(iNodeTbl[fileptr->iNodeIndex].dataPointers[i]); //mark as free
			write_blocks(iNodeTbl[fileptr->iNodeIndex].dataPointers[i], 1, &buffer); //write null block
			iNodeTbl[fileptr->iNodeIndex].dataPointers[i] = 0; //remove block pointer from inode
		}
	}
	if(iNodeTbl[fileptr->iNodeIndex].indirectPointer > 0){ //clear more pointers
		for(i = 0; i < 11; i++){
			if(iNodeTbl[iNodeTbl[fileptr->iNodeIndex].indirectPointer].dataPointers[i] > 0){
				rm_index(iNodeTbl[iNodeTbl[fileptr->iNodeIndex].indirectPointer].dataPointers[i]); //mark as free
				write_blocks(iNodeTbl[iNodeTbl[fileptr->iNodeIndex].indirectPointer].dataPointers[i], i, &buffer); //write null block in removed location
				iNodeTbl[iNodeTbl[fileptr->iNodeIndex].indirectPointer].dataPointers[i] = 0; //remove block pointer from inode
			}
		}
		iNodeTbl[iNodeTbl[fileptr->iNodeIndex].indirectPointer].used = 0;
	}
	iNodeTbl[fileptr->iNodeIndex].indirectPointer = 0; //free pointer
	iNodeTbl[fileptr->iNodeIndex].size = 0; //remove file size
	iNodeTbl[fileptr->iNodeIndex].used = 0; //iNode can now be re-used
	sfs_fclose(fileID);
	return 0;
	
}
