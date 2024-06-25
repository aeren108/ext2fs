#include "ext2fs_utils.h"
#include "ext2fs_print.h"

uint8_t *disk;
uint32_t block_size; 

void init_disk(char const* img_path) {
    int fd = open(img_path, O_RDWR);
    if (fd == -1) {
        perror("init_disk(): can not open file");
        exit(1);
    }

    struct stat filest;
    fstat(fd, &filest);

    if ((disk = mmap(NULL, filest.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        perror("init_disk(): map failed");
        exit(1);
    }

    block_size = EXT2_UNLOG(get_super_block()->log_block_size);
}

struct ext2_super_block* get_super_block() {
    return (struct ext2_super_block*) (disk + EXT2_SUPER_BLOCK_POSITION);
}

struct ext2_block_group_descriptor* get_group_descriptor(int n) {
    return (struct ext2_block_group_descriptor*) (disk + (block_size > 1024 ? 1 : 2)*block_size + (sizeof(struct ext2_block_group_descriptor) * n));
}

struct ext2_dir_entry* find_dir_entry_by_name(struct ext2_inode* inode, char* dirname) {
    struct ext2_dir_entry *cur_dir = (struct ext2_dir_entry*) read_inode_data(inode, 0);
    int offset = 0;

    while (cur_dir != NULL) {
        if (cur_dir->inode == 0) break;
        if (strncmp(cur_dir->name, dirname, strlen(dirname)) == 0) return cur_dir;

        offset += cur_dir->length;
        cur_dir = (struct ext2_dir_entry*) read_inode_data(inode, offset);
    }

    return NULL;
}

struct ext2_inode* get_inode_by_id(int id) {
    int group_num = id / get_super_block()->inodes_per_group;
    int local_offset = id % get_super_block()->inodes_per_group;

    return (struct ext2_inode*) (disk + get_group_descriptor(group_num)->inode_table * block_size + EXT2_INODE_SIZE * (local_offset-1));
}

struct ext2_inode* get_inode_from_path(char* path, int* inode_num) {
    if (path[0] != '/') return NULL;
    else if (path[1] == '\0') {
        *inode_num = 2;
        return get_inode_by_id(2);
    }

    char* token = strtok(path, "/");
    struct ext2_inode *parent = get_inode_by_id(2); //root directory

    while (token != NULL) {
        struct ext2_dir_entry *dir = find_dir_entry_by_name(parent, token);
        if (!dir) return NULL;

        *inode_num = dir->inode;
        parent = get_inode_by_id(dir->inode);
        token = strtok(NULL, "/");
    }

    return parent;
}

uint8_t* read_inode_data(struct ext2_inode* inode, int offset) {
    if (offset < block_size) return get_block(inode->direct_blocks[0]) + offset;
    else if (offset < 12*block_size) {
        int block_offset = offset / block_size;
        int local_offset = offset % block_size;

        if (inode->direct_blocks[block_offset] == 0) return NULL;
        return get_block(inode->direct_blocks[block_offset]) + local_offset;
    } else if (offset < block_size * (block_size / sizeof(int) + 12)){
        if (inode->single_indirect == 0) return NULL;

        uint8_t *indirect_block = get_block(inode->single_indirect);
        int block_offset = offset / block_size - 12;
        int local_offset = offset % block_size;

        int block_number = *(int*) (indirect_block + block_offset);
        if (block_number == 0) return NULL;

        return get_block(block_number) + local_offset;
    } 
    
    // TODO handle double indirects and triple indirects

    return NULL;
}

int find_block() {
    int free_block = -1;
    int cur_group = 0;
    int group_count = get_group_count();

    while (cur_group < group_count && free_block == -1) {
        struct ext2_block_group_descriptor *gd = get_group_descriptor(cur_group);
        if ((free_block = get_free_bit(get_block(gd->block_bitmap))) != -1) 
            free_block += get_super_block()->blocks_per_group * cur_group;
        
        cur_group++;
    }

    return free_block;
}

int find_inode() {
    int free_inode = -1;
    int cur_group = 0;
    int group_count = get_group_count();

    while (cur_group < group_count && free_inode == -1) {
        struct ext2_block_group_descriptor *gd = get_group_descriptor(cur_group);
        if ((free_inode = get_free_bit(get_block(gd->inode_bitmap))) != -1) 
            free_inode += get_super_block()->inodes_per_group * cur_group;
        
        cur_group++;
    }

    return free_inode;
}

int return_block(int block) {
    int group_num = block / get_super_block()->blocks_per_group;
    int local_offset = block % get_super_block()->blocks_per_group;

    memset((uint8_t*)get_block(block), 0, block_size);
    unset_bit(get_block(get_group_descriptor(group_num)->block_bitmap), local_offset);

    get_super_block()->free_block_count += 1;
    get_group_descriptor(group_num)->free_block_count += 1; 
    
    return block;
}

int return_inode(int inode) {
    int group_num = inode / get_super_block()->inodes_per_group;
    int local_offset = inode % get_super_block()->inodes_per_group;

    memset((uint8_t*) get_inode_by_id(inode+1), 0, EXT2_INODE_SIZE);
    unset_bit(get_block(get_group_descriptor(group_num)->inode_bitmap), local_offset);

    get_super_block()->free_inode_count += 1;
    get_group_descriptor(group_num)->free_inode_count += 1;

    return inode;
}

int take_block(int block) {
    int group_num = block / get_super_block()->blocks_per_group;
    int local_offset = block % get_super_block()->blocks_per_group;

    set_bit(get_block(get_group_descriptor(group_num)->block_bitmap), local_offset);

    get_super_block()->free_block_count -= 1;
    get_group_descriptor(group_num)->free_block_count -= 1;

    return block;
}

int take_inode(int inode) {
    int group_num = inode / get_super_block()->inodes_per_group;
    int local_offset = inode % get_super_block()->inodes_per_group;

    set_bit(get_block(get_group_descriptor(group_num)->inode_bitmap), local_offset);

    get_super_block()->free_inode_count -= 1;
    get_group_descriptor(group_num)->free_inode_count -= 1;

    return inode;
}

//reserve a block for an inode
int reserve_block(struct ext2_inode* inode) {
    uint8_t* block_bitmap = get_block(get_group_descriptor(find_block_group_number((uint8_t*) inode))->block_bitmap);
    int free_index = -1, free_block;

    if ((free_block = find_block()) == -1) {
        perror("reserve_block(): no available free block");
        return -1;
    }

    take_block(free_block);

    for (int i = 0; i < 12; ++i) {
        if (inode->direct_blocks[i] == 0) {
            free_index = i; 
            break;
        }
    }

    if (free_index == -1) {
        if (inode->single_indirect == 0) 
            inode->single_indirect = take_block(find_block());

        uint32_t* cur_blocknum = (uint32_t*) get_block(inode->single_indirect);
        while (*cur_blocknum != 0) cur_blocknum++;
        
        *cur_blocknum = free_block;
    } else {
        inode->direct_blocks[free_index] = free_block;
    }

    return free_block;
}

uint8_t* reserve_dir_entry(struct ext2_dir_entry* dir, struct ext2_inode* inode) {
    struct ext2_dir_entry *cur = (struct ext2_dir_entry*) read_inode_data(inode, 0);
    uint8_t* spot;
    int offset = 0;

    while (cur != NULL) {
        int est_length = round_to_4k(sizeof(struct ext2_dir_entry) + cur->name_length);
        int size_diff = cur->length - est_length;
        
        if (size_diff > dir->length) {
            spot = (uint8_t*) cur;
            spot += est_length;
            
            cur->length = est_length;
            dir->length = size_diff;

            struct ext2_dir_entry* ndirspot = (struct ext2_dir_entry*) spot;
            copy_entry_dir(ndirspot, dir);

            break;
        }

        offset += cur->length;
        cur = (struct ext2_dir_entry*) read_inode_data(inode, offset);
    }
    
    if (!cur) {
        int newblock = reserve_block(inode);
        cur = (struct ext2_dir_entry*) get_block(newblock);
        
        dir->length = block_size;
        copy_entry_dir(cur, dir);
        spot = cur;
    }

    return spot;
}

struct ext2_dir_entry* rm_dir_entry(struct ext2_inode* inode, char* dirname) {
    struct ext2_dir_entry *cur = read_inode_data(inode, 0);
    struct ext2_dir_entry *next = read_inode_data(inode, cur->length);
    int offset = 0;
    
    while (cur != NULL) {
        if (!next) return NULL;

        if (strncmp(next->name, dirname, next->name_length) == 0) {
            cur->length += next->length;
            break; 
        }

        offset += cur->length;
        cur = read_inode_data(inode, offset);
        next = read_inode_data(inode, offset+cur->length);
    }
    
    return next;
}

int dir_entry_count(struct ext2_inode *inode) {
    struct ext2_dir_entry *cur = read_inode_data(inode, 0);
    int offset = 0;
    int de_count = 0;

    while (cur != NULL) {
        de_count++;
        offset += cur->length;
        
        cur = read_inode_data(inode, offset);
    }

    return de_count;
}

//returns the first unset bit of the bitmap
int get_free_bit(uint8_t* bitmap) {
    for (int i = 0; i < block_size; ++i) {
        int byte_index = i / 8;
        int bit_index = i % 8;

        if ((bitmap[byte_index] & (1 << bit_index)) == 0) return i;
    }

    return -1;
}

void set_bit(uint8_t* bitmap, int index) {
    int byte_index = index / 8;
    int bit_index = index % 8;

    bitmap[byte_index] |= 1 << bit_index;
}

void unset_bit(uint8_t* bitmap, int index) {
    int byte_index = index / 8;
    int bit_index = index % 8;

    bitmap[byte_index] &= ~(1 << bit_index);
}

uint8_t* get_block(int n) {
    return disk + n*block_size;
}

int get_group_count() {
    struct ext2_super_block* sb = get_super_block();
    if (sb->blocks_per_group >= sb->block_count) return 1;
    return sb->block_count / sb->blocks_per_group;
}

int get_block_size() {
    return block_size;
}

void split_parent(const char* path, char** parent, char** dirname) {
    char *last_slash = strrchr(path, '/');
    
    if (!last_slash) {
        *parent = NULL;
        *dirname = strdup(path);
    } else if (last_slash == path) {
        *parent = calloc(2, sizeof(char)); 
        (*parent)[0] = '/'; (*parent)[1] = '\0';

        *dirname = strdup(last_slash + 1);
    } else {
        int parentlen = last_slash - path + 1;
        *parent = calloc(parentlen + 1, sizeof(char));
        strncpy(*parent, path, parentlen);
        *(*parent + parentlen) = '\0';

        *dirname = strdup(last_slash + 1);
    }
}

void copy_entry_dir(struct ext2_dir_entry* dest, struct ext2_dir_entry* src) {
    dest->file_type = src->file_type;
    dest->inode = src->inode;
    dest->length = src->length;
    dest->name_length = src->name_length;
    strncpy(dest->name, src->name, src->name_length);
}

int round_to_4k(int n) {
    int k = n / 4;
    int r = n % 4;
    
    if (r == 0) return n;
    return (k+1)*4;
}

int find_block_number(uint8_t *spot) {
    return (spot - disk) / block_size;
}

int find_block_group_number(uint8_t *spot) {
    return find_block_number(spot)/ get_super_block()->blocks_per_group;
}