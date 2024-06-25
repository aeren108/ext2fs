#ifndef __EXT2FS_H__
#define __EXT2FS_H__

#include <stdint.h>

#define EXT2_BOOT_BLOCK_SIZE 1024
#define EXT2_SUPER_BLOCK_SIZE 1024
#define EXT2_SUPER_BLOCK_POSITION EXT2_BOOT_BLOCK_SIZE
#define EXT2_ROOT_INODE 2
#ifndef EXT2_INODE_SIZE
#define EXT2_INODE_SIZE 256
#endif
#define EXT2_NUM_DIRECT_BLOCKS 12
#define EXT2_MAX_NAME_LENGTH 255

#define EXT2_SUPER_MAGIC 0xEF53

/* Can use this to convert super block log_* fields to actual sizes */
#define EXT2_UNLOG(v) (1UL << (10UL + (v)))

/* inode mode bits for file type */
// regular file and directory
#define EXT2_I_FTYPE   0x8000
#define EXT2_I_DTYPE   0x4000

/* dir entry file types */
// regular file and directory
#define EXT2_D_FTYPE   1
#define EXT2_D_DTYPE   2

/* inode mode bits for file permissions */
// regular file and directory
#define EXT2_I_FPERM	0664
#define EXT2_I_DPERM	0775

/* inode default uid and gid */
#define EXT2_I_UID 1000
#define EXT2_I_GID 1000


#include <time.h>

#ifdef TIMESTAMP
#define UPDATE_TIME(t) (t) = TIMESTAMP
#else
#define UPDATE_TIME(t) time((time_t *)&(t))
#endif

/* Minor level after we modify to ext2s, otherwise it's usually 0 */
#define EXT2S_MINOR_LEVEL 334

enum ext2_journal_info {
    EXT2_ALLOC_INODE = 0xf8, EXT2_FREE_INODE,
    EXT2_LINK_INODE, EXT2_UNLINK_INODE,
    EXT2_ALLOC_BLOCK, EXT2_FREE_BLOCK,
    EXT2_LINK_BLOCK, EXT2_UNLINK_BLOCK,
};


struct ext2_super_block {
    uint32_t inode_count; /* Total number of inodes in the fs */
    uint32_t block_count; /* Total number of blocks in the fs */
    uint32_t reserved_block_count; /* Number of blocks reserved for root */
    uint32_t free_block_count; /* Number of free blocks */
    uint32_t free_inode_count; /* Number of free inodes */
    uint32_t first_data_block; /* The first data block number */
    uint32_t log_block_size; /* 2^(10 + this value) gives the block size */
    uint32_t log_fragment_size; /* Same for fragments (we won't use fragments) */
    uint32_t blocks_per_group; /* Number of blocks for each block group (last group can have fewer) */
    uint32_t fragments_per_group; /* Same for fragments */
    uint32_t inodes_per_group; /* Number of inodes for each block group (last group can have fewer) */
    uint32_t mount_time; /* Mounting and modification metadata, many less important fields */
    uint32_t write_time;
    uint16_t mount_count;
    uint16_t max_mount_count;
    uint16_t magic; /* Magic field, should be EXT2_SUPER_MAGIC */
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t last_check_time;
    uint32_t check_interval;
    uint32_t creator_os;
    uint32_t rev_level; /* Revision level: 0 or 1 */
    uint16_t default_uid;
    uint16_t default_gid;
    uint32_t first_inode; /* First non-reserved inode in the filesystem */
    uint16_t inode_size; /* Size of each inode */
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    /* More stuff after this, but don't worry about them! */
};

// Type for the reference counter. Set to be 32 bits.
typedef uint32_t refctr_t;

struct ext2_block_group_descriptor {
    uint32_t block_bitmap; /* Block containing the block bitmap */
    uint32_t inode_bitmap; /* Block containing the inode bitmap */
    uint32_t inode_table; /* First block of the inode table */
    uint16_t free_block_count; /* Number of free blocks in the group */
    uint16_t free_inode_count; /* Number of free inodes in the group */
    uint16_t used_dirs_count; /* Number of directories in the group */
    uint16_t pad; /* Padding to 4 byte alignment */
    uint32_t reserved[3];
};

struct ext2_inode {
    uint16_t mode; /* Contains filetype and permissions */
    uint16_t uid; /* Owning user id */
    uint32_t size; /* Least significant 32-bits of file size in rev. 1 */
    uint32_t access_time; /* Timestamps (in seconds since 1 Jan 1970) */
    uint32_t creation_time;
    uint32_t modification_time;
    uint32_t deletion_time; /* Zero for non-deleted inodes! */
    uint16_t gid; /* Owning group id */
    uint16_t link_count; /* Number of hard links */
    uint32_t block_count_512; /* Number of 512-byte blocks alloc'd to file */
    uint32_t flags; /* Special flags */
    uint32_t reserved; /* 4 reserved bytes */
    uint32_t direct_blocks[EXT2_NUM_DIRECT_BLOCKS];
    uint32_t single_indirect;
    uint32_t double_indirect;
    uint32_t triple_indirect;
    /* Some other stuff that we don't care about too much */
};

struct ext2_dir_entry {
    uint32_t inode; /* inode number of the file */
    uint16_t length; /* record length, round up to 4 bytes since records need to be aligned on 4 */
    uint8_t name_length; /* 255 is the maximum possible length */
    uint8_t file_type; /* Not used in revision 0, file type identifier in revision 1 */
    char name[]; /* Where the name starts. This is called a 'flexible array member', learn! */
};

#ifdef JOURNAL
struct ext2_journal_metadata_block {
	uint8_t data[]; /* entire contents of a single block of file system metadata */
};

struct ext2_journal_descriptor_block {
	/* number of metadata blocks to be written */
	uint32_t metadata_blocks_count;
	/* Disk block numbers of the metadata blocks */
	uint32_t metadata_blocks_array[EXT2_MAX_METADATA_BLOCKS];
};

struct ext2_journal_header_block {
	/* sequence number */
	uint64_t sequence_number;
	/* current head of the journal (block number of the last block written) */
	uint32_t current_head;
	/* oldest block in the log which has not been unpinned (tail) */
	uint32_t tail;
};
#endif

#endif
