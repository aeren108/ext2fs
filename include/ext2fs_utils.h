#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "ext2fs.h"

void init_disk(char const * img_path);
struct ext2_super_block* get_super_block();
struct ext2_block_group_descriptor* get_group_descriptor(int n);

int get_group_count();
int get_block_size();
int find_block_number(uint8_t *spot);
int find_block_group_number(uint8_t *spot);

uint8_t* get_block(int n);
struct ext2_inode* get_inode_by_id(int id);
struct ext2_inode* get_inode_from_path(char* path, int* inode_num);
uint8_t* read_inode_data(struct ext2_inode* inode, int offset);

uint8_t* reserve_dir_entry(struct ext2_dir_entry* dir, struct ext2_inode* inode);
struct ext2_dir_entry* find_dir_entry_by_name(struct ext2_inode* inode, char* dirname);
int dir_entry_count(struct ext2_inode *inode);
struct ext2_dir_entry* rm_dir_entry(struct ext2_inode* inode, char* dirname);

int find_inode();
int find_block();
int take_inode(int inode);
int take_block(int block);
int return_inode(int inode);
int return_block(int block);

int get_free_bit(uint8_t* bitmap);
void set_bit(uint8_t* bitmap, int index);
void unset_bit(uint8_t* bitmap, int index);

int round_to_4k(int n);
void split_parent(const char* path, char** parent, char** dirname);
void copy_entry_dir(struct ext2_dir_entry* dest, struct ext2_dir_entry* src);

