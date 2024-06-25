#include "ext2fs.h"
#include "ext2fs_print.h"
#include "ext2fs_utils.h"

int ext2_mkdir(char* path);

/*

Usage:
    ./je2fs disk.img mkdir /abs/path/to/dir

*/

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        perror("Usage: ./je2fs mkdir disk.img /abs/path/to/dir");
        return -1;
    }

    init_disk(argv[1]);

    struct ext2_super_block *sb = get_super_block();
    struct ext2_block_group_descriptor *gd = get_group_descriptor(0);

    //print_super_block(sb);
    //print_group_descriptor(gd);
    
    struct ext2_inode *inode2 = get_inode_by_id(2);
    struct ext2_inode *inode11 = get_inode_by_id(11);
    
    /*uint8_t bitmap[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t bitmap2[4] = {0, 0, 0, 0};
    set_bit(bitmap2, 0);
    for (int i = 0; i < 4; i++) {
        printf("%02X ", bitmap2[i]);
    }*/

    if (strcmp(argv[2], "mkdir") == 0)
        ext2_mkdir(argv[3]);
    else if (strcmp(argv[2], "rmdir") == 0)
        ext2_rmdir(argv[3]);
    else if (strcmp(argv[2], "rm") == 0)
        ext2_rm(argv[3]);

    return 0;
}

int ext2_mkdir(char* path) {
    char *dirstr, *parentstr;
    struct ext2_inode *parent_inode;
    int parent_ino, free_inode, free_block;

    split_parent(path, &parentstr, &dirstr);
    
    if ((parent_inode = get_inode_from_path(parentstr, &parent_ino)) == NULL) {
        perror("ext2_mkdir(): invalid directory");
        return EINVAL;
    }

    if (find_dir_entry_by_name(parent_inode, dirstr) != NULL) {
        perror("ext2_mkdir(): directory already exists");
        return EEXIST;
    }

    free_inode = find_inode() + 1;
    free_block = find_block();

    take_inode(free_inode - 1);
    take_block(free_block);

    struct ext2_dir_entry* newdir = malloc(sizeof(struct ext2_dir_entry) + strlen(dirstr));
    newdir->inode = free_inode;
    newdir->file_type = EXT2_D_DTYPE;
    newdir->name_length = strlen(dirstr);
    newdir->length = sizeof(struct ext2_dir_entry) + newdir->name_length;
    strncpy(newdir->name, dirstr, strlen(dirstr));

    uint8_t* entry_spot = reserve_dir_entry(newdir, parent_inode);
    int block_num = find_block_number(entry_spot);

    struct ext2_inode* newinode = get_inode_by_id(free_inode);
    newinode->mode = EXT2_I_DTYPE;
    newinode->uid = EXT2_I_UID;
    newinode->gid = EXT2_I_GID;
    newinode->creation_time = time(NULL);
    newinode->access_time = time(NULL);
    newinode->modification_time = time(NULL);
    newinode->deletion_time = 0;
    newinode->direct_blocks[0] = free_block;
    newinode->single_indirect = 0;
    newinode->double_indirect = 0;
    newinode->triple_indirect = 0;
    newinode->size = get_block_size();
    newinode->link_count = 2;
    newinode->block_count_512 = get_block_size() / 512;
    for (int i = 1; i < 12; ++i) newinode->direct_blocks[i] = 0;

    struct ext2_dir_entry* curdir = malloc(sizeof(struct ext2_dir_entry) + 1);
    curdir->inode = free_inode;
    curdir->file_type = EXT2_D_DTYPE;
    curdir->name_length = 1;
    curdir->length = round_to_4k(sizeof(struct ext2_dir_entry) + curdir->name_length);
    strncpy(curdir->name, ".", 1);

    struct ext2_dir_entry* parentdir = malloc(sizeof(struct ext2_dir_entry) + 2);
    parentdir->inode = parent_ino;
    parentdir->file_type = EXT2_D_DTYPE;
    parentdir->name_length = 2;
    parentdir->length = get_block_size() - curdir->length;
    strncpy(parentdir->name, "..", 2);

    struct ext2_dir_entry* start = (struct ext2_dir_entry*) read_inode_data(newinode, 0);
    copy_entry_dir(start, curdir);

    struct ext2_dir_entry* after = (struct ext2_dir_entry*) read_inode_data(newinode, curdir->length);
    copy_entry_dir(after, parentdir);

    get_group_descriptor(0)->used_dirs_count += 1;
    parent_inode->link_count += 1;

    //TODO update modification and access times

    printf("EXT2_ALLOC_INODE %d\n", free_inode);
    printf("EXT2_ALLOC_BLOCK %d\n", free_block);
    printf("EXT2_ALLOC_ENTRY \"%s\" %d\n", newdir->name, block_num);
    printf("EXT2_ALLOC_ENTRY \".\" %d\n", free_block);
    printf("EXT2_ALLOC_ENTRY \"..\" %d\n", free_block);

    free(dirstr); free(parentstr); free(newdir); free(curdir); free(parentdir);

    return 1;
}

int ext2_rmdir(char* path) {
    int parent_ino, ino, free_inode, free_block;
    char *dirstr, *parentstr, *duppath = strdup(path);
    struct ext2_inode *parent_inode;
    struct ext2_inode *inodep = get_inode_from_path(duppath, &ino);

    if (!inodep) {
        perror("ext2_rmdir(): invalid directory");
        return EINVAL;
    }

    if (dir_entry_count(inodep) > 2) {
        perror("ext2_rmdir(): directory is not empty");
        return EEXIST;
    }

    split_parent(path, &parentstr, &dirstr);
    parent_inode = get_inode_from_path(parentstr, &parent_ino);
    
    struct ext2_dir_entry *del = rm_dir_entry(parent_inode, dirstr);
    parent_inode->link_count -= 1;

    int freed_block_no = return_block(inodep->direct_blocks[0]);
    int freed_inode_no = return_inode(ino-1);
    
    memset((uint8_t*)del, 0, del->length);
    get_group_descriptor(ino / get_super_block()->inodes_per_group)->used_dirs_count -= 1;
    
    printf("EXT2_FREE_ENTRY \"%s\" %d\n", dirstr, find_block_number((uint8_t*) del));
    printf("EXT2_FREE_BLOCK %d\n", freed_block_no);
    printf("EXT2_FREE_INODE %d\n", ino);

    //TODO update modification and access times

    free(dirstr); free(parentstr); free(duppath);

    return 1;
}

int ext2_rm(const char* path) {
    int parent_ino, ino, free_inode, free_block;
    char *dirstr, *parentstr, *duppath = strdup(path);
    struct ext2_inode *parent_inode;
    struct ext2_inode *inodef = get_inode_from_path(duppath, &ino);

    if (!inodef) {
        perror("ext2_rm(): no such file");
        return EINVAL;
    }

    split_parent(path, &parentstr, &dirstr);
    parent_inode = get_inode_from_path(parentstr, &parent_ino);

    struct ext2_dir_entry *del = rm_dir_entry(parent_inode, dirstr);
    if (!del) {
        perror("ext2_rm(): no such directory entry");
        return EINVAL;
    }
    memset((uint8_t*)del, 0, del->length);
    printf("EXT2_FREE_ENTRY \"%s\" %d\n", dirstr, find_block_number((uint8_t*) del));

    for (int i = 0; i < 12; ++i) {
        if (inodef->direct_blocks[i] == 0) break;

        printf("EXT2_FREE_BLOCK %d\n", inodef->direct_blocks[i]);
        return_block(inodef->direct_blocks[i]);
        inodef->direct_blocks[i] = 0;
    }

    if (inodef->single_indirect) {
        uint32_t* cur_block = (uint32_t*) get_block(inodef->single_indirect);
        while (*cur_block) {
            printf("EXT2_FREE_BLOCK %d\n", *cur_block);
            
            return_block(*cur_block);
            *cur_block = 0;
            cur_block++;
        }
    }

    return_block(inodef->single_indirect);
    inodef->single_indirect = 0;
    printf("EXT2_FREE_INODE %d\n", ino);
    return_inode(ino-1);

    free(dirstr); free(parentstr); free(duppath);

    return 1;
}