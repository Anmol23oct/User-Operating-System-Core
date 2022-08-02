/*
 *  Copyright (C) 2022 CS416/518 Rutgers CS
 *	RU File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include<pthread.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX] = "/tmp/av730";
// void* block_mem;
// struct inode* inode_ptr;
bitmap_t inode_bitmap;
bitmap_t data_bitmap;
struct superblock* super_block;
int max_dir_entries;
int inodes_per_block;
int total_inodes_used = 0;
int total_inodes_cleared = 0;
int data_blocks_used = 0;
int data_blocks_cleared = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int readi(uint16_t ino, struct inode *inode);
int writei(uint16_t ino, struct inode *inode);

// Declare your in-memory data structures here


// utility functions

int clear_inode(int ino_num) {
	struct inode* inode_ptr = malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	readi(ino_num, inode_ptr);
	inode_ptr->ino = 0;
	inode_ptr->link = 0;
	inode_ptr->size = 0;
	inode_ptr->type = 0;
	inode_ptr->valid = 0;

	for(int i = 0; i < 16; i++){
		inode_ptr->direct_ptr[i] = -1;
	}

	for(int i = 0; i < 16; i++){
		inode_ptr->indirect_ptr[i] = -1;
	}

	writei(ino_num, inode_ptr);
	
	bio_read(super_block->i_bitmap_blk, inode_bitmap);
	unset_bitmap(inode_bitmap, ino_num);
	bio_write(super_block->i_bitmap_blk, inode_bitmap);

	total_inodes_cleared += 1;

	return 0;
}

int clear_data_block(int blk_no){
	void* block_mem = malloc(BLOCK_SIZE);
	memset(block_mem, 0, BLOCK_SIZE);

	bio_read(blk_no, block_mem);
	memset(block_mem, 0, BLOCK_SIZE);
	bio_write(blk_no, block_mem);

	bio_read(super_block->d_bitmap_blk, data_bitmap);
	unset_bitmap(data_bitmap, blk_no - super_block->d_start_blk);
	bio_write(super_block->d_bitmap_blk, data_bitmap);

	data_blocks_cleared += 1;

	return 0;
}

int init_indirect_blk(int* ind_blk) {
	for(int i = 0; i < BLOCK_SIZE/sizeof(int); i++){
		ind_blk[i] = -1;
	}
	return 0;
}

int init_inode(struct inode* new_inode, uint16_t ino, uint32_t type) {
	new_inode->ino = ino;
	new_inode->size = 0;
	new_inode->type = type;
	new_inode->valid = 1;

	for(int i = 0; i < 16; i++){
		new_inode->direct_ptr[i] = -1;
	}

	for(int i = 0; i < 16; i++){
		new_inode->indirect_ptr[i] = -1;
	}

	// stat update
	time(&(new_inode->vstat.st_atime));
	time(&(new_inode->vstat.st_mtime));
	time(&(new_inode->vstat.st_ctime));
	
	new_inode->vstat.st_ino = ino;
	new_inode->vstat.st_uid = getuid();
	new_inode->vstat.st_gid = getgid();
	if (type == 1){
		new_inode->vstat.st_mode = __S_IFDIR | 0755;
		new_inode->link = 2;
	} else {
		new_inode->vstat.st_mode = __S_IFREG | 0644;
		new_inode->link = 1;
	}
	new_inode->vstat.st_size = new_inode->size;
	new_inode->vstat.st_nlink = new_inode->link;
	return 0;

}

int init_dirent_table(struct dirent* table, uint16_t curr_ino, uint16_t par_ino) {
	for(int i = 0; i < max_dir_entries; i++){
		if (i == 0){
			strcpy(table[i].name, "..");
			table[i].ino = par_ino;
			table[i].len = 2;
			table[i].valid = 1;
		} else if (i == 1) {
			strcpy(table[i].name, ".");
			table[i].ino = curr_ino;
			table[i].len = 1;
			table[i].valid = 1;
		} else {
			strcpy(table[i].name, "");
			table[i].ino = 0;
			table[i].len = 0;
			table[i].valid = 0;
		}
	}

	return 0;
}


int is_directory_empty(struct dirent* table){
	for(int i = 2; i < max_dir_entries; i++){
		if (table[i].valid) {
			return 0;
		}
	}
	return 1;
}

void print_dirent_table(struct dirent* table){
	for(int i = 0; i < max_dir_entries; i++){
		printf("%d %s 	Valid: %d	Inode: %d\n", i+1, table[i].name, table[i].valid, table[i].ino);
	}
}

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 

	//--------Implementation-------------------

	bio_read(super_block->i_bitmap_blk,inode_bitmap);
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i=0;i<MAX_INUM; i++){
		uint8_t get_bit=get_bitmap(inode_bitmap,i);
		if (get_bit==0){
			total_inodes_used += 1;
			set_bitmap(inode_bitmap,i);
			bio_write(super_block->i_bitmap_blk,inode_bitmap);
			return i;

		}
	}

	return -1;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 

	bio_read(super_block->d_bitmap_blk, data_bitmap);
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i=0;i<MAX_DNUM; i++){
		uint8_t get_bit = get_bitmap(data_bitmap, i);
		if (get_bit == 0){
			data_blocks_used += 1;
			set_bitmap(data_bitmap, i);
			bio_write(super_block->d_bitmap_blk, data_bitmap);
			return super_block->d_start_blk+i;

		}
	}

	return -1;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number

  // Step 2: Get offset of the inode in the inode on-disk block

  // Step 3: Read the block from disk and then copy into inode structure

  int b_no;
  void *buf = malloc(BLOCK_SIZE);
  memset(buf, 0, BLOCK_SIZE);
  b_no=(int)ino/inodes_per_block;
  int block_no=super_block->i_start_blk+b_no;
  bio_read(block_no, buf);
  
  int offset;
  offset=(int)ino%inodes_per_block;
  
  memcpy(inode, buf + (offset)*sizeof(struct inode), sizeof(struct inode));
  free(buf);
  return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	
	// Step 2: Get the offset in the block where this inode resides on disk

	// Step 3: Write inode to disk
	if (ino > MAX_INUM) {
		return -1;
	}
	int b_no;
  	void *buf = malloc(BLOCK_SIZE);
	memset(buf, 0, BLOCK_SIZE);
  	b_no=(int)ino/inodes_per_block;
	int block_no=super_block->i_start_blk+b_no;
	bio_read(block_no, buf);
	
	int offset;
	offset=(int)ino%inodes_per_block;
	time(&(inode->vstat.st_ctime));
	memcpy(buf + (offset)*sizeof(struct inode), inode, sizeof(struct inode));
	bio_write(block_no, buf);
	free(buf);
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
	
	// struct inode* dir_inode;
	struct dirent* dirent_table = (struct dirent*) malloc(BLOCK_SIZE);
	memset(dirent_table, 0, BLOCK_SIZE);
	struct inode* inode_ptr = malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	int status = readi(ino, inode_ptr);
	int data_blk_no = inode_ptr->direct_ptr[0];
	bio_read(data_blk_no, dirent_table);
	char *filename = strdup(fname);
	for(int i = 0; i < max_dir_entries; i++){
		if (strcmp(filename, dirent_table[i].name) == 0){
			strcpy(dirent->name, dirent_table[i].name);
			dirent->valid = dirent_table[i].valid;
			dirent->ino = dirent_table[i].ino;
			dirent->len = dirent_table[i].len;
			free(dirent_table);
			free(inode_ptr);
			return 0;
		}
	}

	free(dirent_table);
	free(inode_ptr);
	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	int dir_blk = dir_inode.direct_ptr[0];
	struct dirent dir_ent;
	if (dir_find(dir_inode.ino, fname, name_len, &dir_ent) == 0){
		return -EEXIST;
	}

	char fname_copy[name_len+1];
	strcpy(fname_copy, fname);

	struct dirent* dirent_table = (struct dirent*) malloc(BLOCK_SIZE);
	memset(dirent_table, 0, BLOCK_SIZE);
	bio_read(dir_blk, (void*)dirent_table);
	for(int i = 0; i < max_dir_entries; i++){
		if (dirent_table[i].valid == 0){
			strcpy(dirent_table[i].name,fname_copy);
			printf("String length: %d\n", strlen(dirent_table[i].name));
			dirent_table[i].ino = f_ino;
			dirent_table[i].len = name_len;
			dirent_table[i].valid = 1;
			print_dirent_table(dirent_table);
			bio_write(dir_blk, dirent_table);

			// parent inode update
			dir_inode.size += sizeof(struct dirent);
			dir_inode.vstat.st_size = dir_inode.size;
			time(&(dir_inode.vstat.st_mtime));
			time(&(dir_inode.vstat.st_atime));
			writei(dir_inode.ino, &dir_inode);
			free(dirent_table);
			return 0;
		}
	}
	
	free(dirent_table);
	return -ENOSPC;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	int dir_blk = dir_inode.direct_ptr[0];

	struct dirent* dirent_table = (struct dirent*) malloc(BLOCK_SIZE);
	memset(dirent_table, 0, BLOCK_SIZE);
	bio_read(dir_blk, (void*)dirent_table);

	char* fname_copy = strdup(fname);

	for(int i = 0; i < max_dir_entries; i++){
		if (strcmp(dirent_table[i].name, fname_copy) == 0){
			strcpy(dirent_table[i].name, "");
			dirent_table[i].ino = 0;
			dirent_table[i].len = 0;
			dirent_table[i].valid = 0;
			bio_write(dir_blk, dirent_table);

			dir_inode.size -= sizeof(struct dirent);
			dir_inode.vstat.st_size = dir_inode.size;
			time(&(dir_inode.vstat.st_mtime));
			time(&(dir_inode.vstat.st_atime));
			writei(dir_inode.ino, &dir_inode);
			free(dirent_table);
			return 0;
		}
	}

	free(dirent_table);
	return -1;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	int num_levels = 0;
	char *path_copy = strdup(path);

	while(strcmp(basename(path_copy), "/") != 0){
        path_copy = strdup(dirname(path_copy));
		num_levels++;
	}
	if (num_levels == 0){
		readi(ino, inode);
		return 0;
	}

    char *level_names[num_levels];
    path_copy = strdup(path);
    for (int i = 0; i < num_levels; i++){
        level_names[i] = strdup(basename(path_copy));
        path_copy = strdup(dirname(path_copy));
    }

	int i = num_levels-1;
	uint16_t current_ino = ino;
	struct dirent* dir_entry = (struct dirent*) malloc(sizeof(struct dirent));
	memset(dir_entry, 0, sizeof(struct dirent));
	while (i >= 0)
	{
		if (dir_find(current_ino, level_names[i], strlen(level_names[i]), dir_entry) < 0){
			printf("Returning -1 from here\n");
			free(dir_entry);
			return -1;
		}
		current_ino = dir_entry->ino;
		i--;
	}
	readi(dir_entry->ino, inode);
	free(dir_entry);
	return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile

	// write superblock information

	// initialize inode bitmap

	// initialize data block bitmap

	// update bitmap information for root directory

	// update inode for root directory
	printf("mkfs is called\n");
	dev_init(diskfile_path);
	printf("after dev init\n");

	inodes_per_block = (int) floor((double)BLOCK_SIZE/sizeof(struct inode));
	max_dir_entries = (int) floor((double)BLOCK_SIZE/sizeof(struct dirent));
	// block_mem = malloc(BLOCK_SIZE);
	// inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	// memset(block_mem, 0, BLOCK_SIZE);
	// memset(inode_ptr, 0, sizeof(struct inode));

	super_block = (struct superblock*) malloc(BLOCK_SIZE);
	memset(super_block, 0, BLOCK_SIZE);
	super_block->d_bitmap_blk = 2;
	super_block->i_bitmap_blk = 1;
	int inode_blocks = (int)ceil((MAX_INUM*sizeof(struct inode))/(float)BLOCK_SIZE);
	super_block->d_start_blk = 3+inode_blocks;
	super_block->i_start_blk = 3;
	
	bio_write(0, super_block);

	inode_bitmap = (bitmap_t) malloc(BLOCK_SIZE);
	memset(inode_bitmap, 0, BLOCK_SIZE);
	bio_write(super_block->i_bitmap_blk, inode_bitmap);

	data_bitmap = (bitmap_t) malloc(BLOCK_SIZE);
	memset(data_bitmap, 0, BLOCK_SIZE);
	bio_write(super_block->d_bitmap_blk, data_bitmap);

	struct inode* root_inode = malloc(sizeof(struct inode));
	memset(root_inode, 0, sizeof(struct inode));
	int inode_num = get_avail_ino();
	if (inode_num < 0){
		return -1;
	}
	init_inode(root_inode, inode_num, 1);

	int data_block_num = get_avail_blkno();
	if (data_block_num < 0){
		return -1;
	}
	root_inode->direct_ptr[0] = data_block_num;

	writei(inode_num, root_inode);
	memset(root_inode, 0, sizeof(struct inode));

	struct dirent* direntry_table = (struct dirent*) malloc(BLOCK_SIZE);
	memset(direntry_table, 0, BLOCK_SIZE);

	init_dirent_table(direntry_table, inode_num, inode_num);
	bio_write(data_block_num, (void *)direntry_table);
	memset((void*) direntry_table, 0, BLOCK_SIZE);
	free(direntry_table);
	free(root_inode);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	printf("Init is called\n");
	if (dev_open(diskfile_path) < 0){
		printf("after dev open if block\n");
		int status  = rufs_mkfs();
		if (status < 0){
			printf("mkfs failed\n");
			return NULL;
		}
		dev_open(diskfile_path);
	} else {
		printf("after dev open else block\n");
		super_block = (struct superblock*) malloc(BLOCK_SIZE);
		memset(super_block, 0, BLOCK_SIZE);
		data_bitmap = (bitmap_t) malloc(BLOCK_SIZE);
		memset(data_bitmap, 0, BLOCK_SIZE);
		bio_read(super_block->d_bitmap_blk, data_bitmap);
		inode_bitmap = (bitmap_t) malloc(BLOCK_SIZE);
		memset(inode_bitmap, 0, BLOCK_SIZE);
		bio_read(super_block->i_bitmap_blk, inode_bitmap);
		inodes_per_block = (int) floor((double)BLOCK_SIZE/sizeof(struct inode));
		max_dir_entries = (int) floor((double)BLOCK_SIZE/sizeof(struct dirent));
	}
	pthread_mutex_init(&mutex, NULL);
	printf("Done with init\n");

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

	int inode_blocks_in_use = ceil((total_inodes_used - total_inodes_cleared) / (double)inodes_per_block);
	int total_inode_blocks = ceil(((double)total_inodes_used)/inodes_per_block);
	int data_blocks_in_use = data_blocks_used - data_blocks_cleared;

	printf("Total allocated data blocks: %d\n", data_blocks_used);
	printf("Total data blocks in use: %d\n", data_blocks_in_use);
	printf("Total allocated inode blocks: %d\n", total_inode_blocks);
	printf("Total inode blocks in use: %d\n", inode_blocks_in_use);

	free(super_block);
	free(inode_bitmap);
	free(data_bitmap);
	dev_close(diskfile_path);
}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode
	
	pthread_mutex_lock(&mutex);
	printf("get_attr is called %s\n", path);

	
	// stbuf->st_atime = time(NULL);
	// stbuf->st_mtime = time(NULL);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));
	
	if (get_node_by_path(strdup(path),0,inode_ptr) < 0) {
		printf("Returning ENOENT GETATTR\n");
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}

	stbuf->st_uid = getuid();
  	stbuf->st_gid = getgid();
	
	stbuf->st_mode = inode_ptr->vstat.st_mode;
	stbuf->st_nlink = inode_ptr->vstat.st_nlink;
	stbuf->st_size = inode_ptr->vstat.st_size;
	stbuf->st_atime = inode_ptr->vstat.st_atime;
	stbuf->st_mtime = inode_ptr->vstat.st_mtime;

	printf("returned from here\n");
	free(inode_ptr);
	pthread_mutex_unlock(&mutex);
	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1
	pthread_mutex_lock(&mutex);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	if (get_node_by_path(path, 0, inode_ptr) < 0){
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}
	free(inode_ptr);
	pthread_mutex_unlock(&mutex);
    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler
	pthread_mutex_lock(&mutex);
	void* block_mem = malloc(BLOCK_SIZE);
	memset(block_mem, 0, BLOCK_SIZE);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	if (get_node_by_path(path, 0, inode_ptr) < 0){
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}

	bio_read(inode_ptr->direct_ptr[0], block_mem);
	for(int i = 0; i < max_dir_entries; i++){
		struct dirent* dir_entry = (struct dirent*) (block_mem + (i * sizeof(struct dirent)));
		if (dir_entry->valid == 1){
			if (filler(buffer, dir_entry->name, NULL, 0) != 0){
				free(block_mem);
				free(inode_ptr);
				pthread_mutex_unlock(&mutex);
				return -1;
			}
		}
	}

	free(inode_ptr);
	free(block_mem);
	pthread_mutex_unlock(&mutex);
	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk

	pthread_mutex_lock(&mutex);
	struct dirent* dirent_table = (struct dirent*) malloc(BLOCK_SIZE);
	memset(dirent_table, 0, BLOCK_SIZE);

	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	printf("Make directory called\n");
	char* path_copy = strdup(path);
	char* folder_name = strdup(basename(path_copy));
	char* parent_dir = strdup(dirname(path_copy));
	printf("Dir name: %s, Folder name: %s\n", parent_dir, folder_name);

	if (get_node_by_path(parent_dir, 0, inode_ptr) != 0){
		free(dirent_table);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}
	printf("After getting inode of parent\n");
	int avail_inode = get_avail_ino();
	if (avail_inode <= 0){
		free(dirent_table);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOSPC;
	}

	int dblock = get_avail_blkno();
	if (dblock <= 0){
		free(dirent_table);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOSPC;
	}

	printf("Available inode: %d\n", avail_inode);
	int dir_add_status = dir_add(*inode_ptr, avail_inode, folder_name, strlen(folder_name));
	if (dir_add_status != 0){
		printf("Dirent add error\n");
		free(dirent_table);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return dir_add_status;
	}
	printf("Added dir entry\n");
	bio_read(inode_ptr->direct_ptr[0], dirent_table);
	print_dirent_table(dirent_table);
	memset(dirent_table, 0, BLOCK_SIZE);

	int parent_ino = inode_ptr->ino;
	memset(inode_ptr, 0, sizeof(struct inode));

	if (readi(avail_inode, inode_ptr) < 0){
		free(dirent_table);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	init_inode(inode_ptr, avail_inode, 1);
	
	inode_ptr->direct_ptr[0] = dblock;
	printf("Initialised inode\n");

	if (bio_read(dblock, dirent_table) < 0){
		printf("bio read error\n");
		free(dirent_table);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	init_dirent_table(dirent_table, avail_inode, parent_ino);

	if (bio_write(dblock, dirent_table) < 0) {
		free(dirent_table);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	printf("Dirent table written\n");

	if (writei(avail_inode, inode_ptr) < 0) {
		free(dirent_table);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	printf("Inode written\n");
	printf("mkdir process complete\n");

	free(dirent_table);
	free(inode_ptr);
	pthread_mutex_unlock(&mutex);
	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	pthread_mutex_lock(&mutex);
	void* block_mem = malloc(BLOCK_SIZE);
	memset(block_mem, 0, BLOCK_SIZE);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	char* path_copy = strdup(path);
	char* folder_name = strdup(basename(path_copy));
	char* parent_dir = strdup(dirname(path_copy));
	path_copy = strdup(path);

	if (get_node_by_path(path_copy, 0, inode_ptr) != 0){
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}
	bio_read(super_block->d_bitmap_blk, data_bitmap);

	// check whether the directory is empty
	struct dirent* dirent_table = (struct dirent*) malloc(BLOCK_SIZE);
	memset(dirent_table, 0, BLOCK_SIZE);
	int is_empty = is_directory_empty(dirent_table);
	free(dirent_table);
	if (!is_empty){
		printf("Directory not empty\n");
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOTEMPTY;
	}

	for(int i = 0; i < 16; i++){
		if (inode_ptr->direct_ptr[i] != -1){
			clear_data_block(inode_ptr->direct_ptr[i]);
		}
	}

	

	uint16_t clear_ino = inode_ptr->ino;
	clear_inode(clear_ino);
	memset(inode_ptr, 0, sizeof(struct inode));

	if (get_node_by_path(parent_dir, 0, inode_ptr) != 0){
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}

	int name_len = strlen(folder_name);
	if (dir_remove(*inode_ptr, folder_name, name_len) < 0){
		printf("Failed to remove directory entry");
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	free(block_mem);
	free(inode_ptr);
	pthread_mutex_unlock(&mutex);
	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk
	pthread_mutex_lock(&mutex);
	void* block_mem = malloc(BLOCK_SIZE);
	memset(block_mem, 0, BLOCK_SIZE);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	char* path_copy = strdup(path);
	char* folder_name = strdup(basename(path_copy));
	char* parent_dir = strdup(dirname(path_copy));
	printf("Create file called. Path: %s\n", path);

	if (get_node_by_path(parent_dir, 0, inode_ptr) != 0){
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}

	int avail_inode = get_avail_ino();
	if (avail_inode <= 0){
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOSPC;
	}
	int dir_add_status = dir_add(*inode_ptr, avail_inode, folder_name, strlen(folder_name));
	if ( dir_add_status != 0){
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return dir_add_status;
	}

	int parent_ino = inode_ptr->ino;
	memset(inode_ptr, 0, sizeof(struct inode));

	if (readi(avail_inode, inode_ptr) != 0){
		printf("CREATE: Unable to read inode\n");
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	init_inode(inode_ptr, avail_inode, 0);
	inode_ptr->vstat.st_mode = __S_IFREG | mode;

	if (writei(avail_inode, inode_ptr) != 0) {
		printf("CREATE: Unable to read inode\n");
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	printf("create process completed\n");
	free(block_mem);
	free(inode_ptr);
	pthread_mutex_unlock(&mutex);
	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1
	
	pthread_mutex_lock(&mutex);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	if (get_node_by_path(path, 0, inode_ptr) < 0){
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}

	free(inode_ptr);
	pthread_mutex_unlock(&mutex);
	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	

	pthread_mutex_lock(&mutex);
	void* block_mem = malloc(BLOCK_SIZE);
	memset(block_mem, 0, BLOCK_SIZE);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	if (get_node_by_path(path, 0, inode_ptr) < 0) {
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}
	if (offset + size > inode_ptr->size){
		printf("given range exceeds file size\n");
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return 0;
	}
	int start_block = offset / BLOCK_SIZE;
	size_t size_copy = size;
	int block_ptr = 0;
	int indir_cap = BLOCK_SIZE / sizeof(int);
	while (size_copy > 0)
	{
		if (start_block <= 15){

			block_ptr = inode_ptr->direct_ptr[start_block];
			if (block_ptr == -1){
				printf("No data for given offset\n");
				free(block_mem);
				free(inode_ptr);
				pthread_mutex_unlock(&mutex);
				return size - size_copy;
			}

		} else {

			int indirect_blk = (start_block - 16) / indir_cap;
			int* blk_ptrs = (int*) malloc(BLOCK_SIZE);
			memset(blk_ptrs, 0, BLOCK_SIZE);
			if (inode_ptr->indirect_ptr[indirect_blk] == -1){
				printf("No data for given offset\n");
				free(block_mem);
				free(inode_ptr);
				free(blk_ptrs);
				pthread_mutex_unlock(&mutex);
				return size - size_copy;
			}

			bio_read(inode_ptr->indirect_ptr[indirect_blk], (void*)blk_ptrs);
			int blk_index = (start_block - 16) % indir_cap;
			if (blk_ptrs[blk_index] == -1) {
				printf("No data for given offset\n");
				free(block_mem);
				free(inode_ptr);
				free(blk_ptrs);
				pthread_mutex_unlock(&mutex);
				return size - size_copy;
			}
			block_ptr = blk_ptrs[blk_index];
			printf("READ: reading from block %d\n", block_ptr);
			free(blk_ptrs);
		}

		int offset_in_block = offset % BLOCK_SIZE;
		bio_read(block_ptr, block_mem);
		memcpy(buffer, block_mem+offset_in_block, size_copy <= (BLOCK_SIZE - offset_in_block) ? size_copy:(BLOCK_SIZE - offset_in_block));
		memset(block_mem, 0, BLOCK_SIZE);
		size_copy = size_copy <= (BLOCK_SIZE - offset_in_block) ? 0:size_copy - (BLOCK_SIZE - offset_in_block);
		offset = 0;
		start_block++;
	}

	time(&(inode_ptr->vstat.st_atime));
	
	writei(inode_ptr->ino, inode_ptr);
	free(block_mem);
	free(inode_ptr);
	pthread_mutex_unlock(&mutex);
	return size;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	pthread_mutex_lock(&mutex);
	void* block_mem = malloc(BLOCK_SIZE);
	memset(block_mem, 0, BLOCK_SIZE);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	if (get_node_by_path(path, 0, inode_ptr) < 0) {
		printf("WRITE: get inode error\n");
		free(inode_ptr);
		free(block_mem);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}
	if (offset  > inode_ptr->size){
		printf("WRITE: given offset exceeds file size\n");
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return 0;
	}

	int start_block = offset / BLOCK_SIZE;

	size_t size_copy = size;
	int block_ptr = 0;
	int indir_cap = BLOCK_SIZE / sizeof(int);
	while (size_copy > 0)
	{
		if (start_block <= 15){
			block_ptr = inode_ptr->direct_ptr[start_block];
			if (block_ptr == -1){
				block_ptr = get_avail_blkno();
				if (block_ptr < 0){
					free(block_mem);
					free(inode_ptr);
					printf("WRITE: No more data blocks left\n");
					pthread_mutex_unlock(&mutex);
					return size-size_copy;
				}
				inode_ptr->direct_ptr[start_block] = block_ptr;
			}


		} else {
			int indirect_blk = (start_block - 16) / indir_cap;
			int* blk_ptrs = (int*) malloc(BLOCK_SIZE);
			memset(blk_ptrs, 0, BLOCK_SIZE);
			if (inode_ptr->indirect_ptr[indirect_blk] == -1){
				int new_blk = get_avail_blkno();
				if (new_blk < 0) {
					free(block_mem);
					free(inode_ptr);
					free(blk_ptrs);
					pthread_mutex_unlock(&mutex);
					return -ENOSPC;
				}
				inode_ptr->indirect_ptr[indirect_blk] = new_blk;
				bio_read(new_blk, (void*)blk_ptrs);
				init_indirect_blk(blk_ptrs);
				bio_write(new_blk, (void*)blk_ptrs);
			}
			bio_read(inode_ptr->indirect_ptr[indirect_blk], (void*)blk_ptrs);
			int blk_index = (start_block - 16) % indir_cap;
			if (blk_ptrs[blk_index] == -1) {
				int new_blk = get_avail_blkno();
				if (new_blk < 0) {
					free(block_mem);
					free(inode_ptr);
					free(blk_ptrs);
					pthread_mutex_unlock(&mutex);
					return -ENOSPC;
				}
				blk_ptrs[blk_index] = new_blk;
				bio_write(inode_ptr->indirect_ptr[indirect_blk], (void*)blk_ptrs);
			}
			block_ptr = blk_ptrs[blk_index];
			printf("Block ptr is %d\n", block_ptr);
			printf("Writing values is %d\n", buffer[0]);
			free(blk_ptrs);
		}

		int offset_in_block = offset % BLOCK_SIZE;
		bio_read(block_ptr, block_mem);
		memcpy(block_mem+offset_in_block, buffer, size_copy <= (BLOCK_SIZE - offset_in_block) ? size_copy:(BLOCK_SIZE - offset_in_block));
		bio_write(block_ptr, block_mem);
		memset(block_mem, 0, BLOCK_SIZE);
		size_copy = size_copy <= (BLOCK_SIZE - offset_in_block) ? 0:size_copy - (BLOCK_SIZE - offset_in_block);
		offset = 0;
		start_block++;
	}


	time(&(inode_ptr->vstat.st_mtime));
	inode_ptr->size += size;
	inode_ptr->vstat.st_size = inode_ptr->size;
	writei(inode_ptr->ino, inode_ptr);
	printf("WRITE: File size is now %i\n", inode_ptr->size);

	free(block_mem);
	free(inode_ptr);
	printf("WRITE: completed\n");
	pthread_mutex_unlock(&mutex);
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	pthread_mutex_lock(&mutex);
	void* block_mem = malloc(BLOCK_SIZE);
	memset(block_mem, 0, BLOCK_SIZE);
	struct inode* inode_ptr = (struct inode*) malloc(sizeof(struct inode));
	memset(inode_ptr, 0, sizeof(struct inode));

	char* path_copy = strdup(path);
	char* folder_name = strdup(basename(path_copy));
	char* parent_dir = strdup(dirname(path_copy));

	if (get_node_by_path(path, 0, inode_ptr) != 0){
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}

	for(int i = 0; i < 16; i++){
		if (inode_ptr->direct_ptr[i] != -1){
			clear_data_block(inode_ptr->direct_ptr[i]);
		}
	}

	printf("UNLINK: cleared direct pointer blocks\n");
	// indirect blocks clearing
	int* blk_ptrs = (int*) malloc(BLOCK_SIZE);
	memset(blk_ptrs, 0, BLOCK_SIZE);
	int ind_cap = BLOCK_SIZE / sizeof(int);

	for(int i = 0; i < 16; i++){
		if (inode_ptr->indirect_ptr[i] == -1){
			break;
		}

		bio_read(inode_ptr->indirect_ptr[i], blk_ptrs);
		for( int j = 0; j < ind_cap; j++){
			if (blk_ptrs[j] == -1){
				break;
			}
			clear_data_block(blk_ptrs[j]);
		}
		clear_data_block(inode_ptr->indirect_ptr[i]);
		memset(blk_ptrs, 0, BLOCK_SIZE);
	}
	free(blk_ptrs);

	uint16_t clear_ino = inode_ptr->ino;
	clear_inode(clear_ino);
	memset(inode_ptr, 0, sizeof(struct inode));

	if (get_node_by_path(parent_dir, 0, inode_ptr) != 0){
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -ENOENT;
	}

	int name_len = strlen(folder_name);
	if (dir_remove(*inode_ptr, folder_name, name_len) < 0){
		printf("Failed to remove directory entry");
		free(block_mem);
		free(inode_ptr);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	
	free(block_mem);
	free(inode_ptr);
	pthread_mutex_unlock(&mutex);
	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {

	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

