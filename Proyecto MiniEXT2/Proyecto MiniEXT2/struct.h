
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>

#define FS_SIZE_BYTES (8 * 1024 * 1024)   // 8 MiBS
#define BLOCK_SIZE       512//4096
#define NUM_BLOCKS (FS_SIZE_BYTES / BLOCK_SIZE)  // 16384 bloques
#define NUM_INODES       1024

#define INODE_SIZE       128
#define INODE_TABLE_BLK  5//3
#define FIRST_DATA_BLOCK 10
#define MAX_NAME         28
#define DIRECT_POINTERS  12
#define ROOT_INODE       0

#pragma pack(push,1)
struct superblock {
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t block_size;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t first_data_block;
    char     fs_name[8];
};

struct inode {
    uint16_t mode, uid, gid;
    uint32_t size, blocks;
    uint32_t direct[DIRECT_POINTERS];
    //uint32_t indirect;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint16_t links_count;
};

struct dir_entry {
    uint32_t inode;
    char     name[MAX_NAME];
};

#pragma pack(pop)

static void *fs_image;
static struct superblock *sb;
static struct inode      *inode_table;