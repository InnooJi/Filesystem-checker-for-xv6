#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define stat xv6_stat //this was causing conflict bc of the 2 stat defs

#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"

#define SUPERBLOCK 1

// Maximum number of directory entries that can fit in all direct + indirect blocks in a directory inode
// To reduce duplicate code
#define MAX_DIRENT_COUNT (BSIZE / sizeof(struct dirent) * (NDIRECT + NINDIRECT))


int fsfd;  // Global file descriptor

/*
 * Read a filesystem block.
 * @param bnum the block number to read
 * @param buf a buffer into which to read the block; the buffer must be at
 *      least as large as one block
 * @return the block number that was read, or -1 on error
 */
int rblock(uint bnum, void *buf) {
    if (lseek(fsfd, bnum * BSIZE, 0) != bnum * BSIZE) {
        return -1;
    }
    if (read(fsfd, buf, BSIZE) != BSIZE) {
        return -1;
    }
    return bnum;
}

/*
* Check if a block is marked allocated in the bitmap
* Returns 1 if allocated, 0 if free, -1 on error
*/
int is_block_allocated(struct superblock *sb, uint blockno){
    if(blockno >= sb->size){
        return -1;
    }
    // Block 0 is never allocated in the bitmap
    if(blockno == 0) return 0;

    //Calculate which bitmap block contains this block's bit
    uint bitmap_block = (blockno / (BSIZE * 8)) + sb -> bmapstart;
    uint bit_offset = blockno % (BSIZE * 8);

    char bitmap[BSIZE];
    if (rblock(bitmap_block, bitmap) < 0){
        return -1;
    }

    return (bitmap[bit_offset/8] >> (bit_offset%8)) & 1;
}



/*
 * Check if a block address is valid
 */
int is_valid_block(struct superblock *sb, uint blockno) {
    // Block 0 is invalid (boot sector)
    // Must be within filesystem size
    // Must be after bitmap start
    // Cannot be superblock (block 1)
    return blockno != 0 && 
           blockno < sb->size && 
           blockno >= sb->bmapstart &&
           blockno != 1;
}

/*
 * Read an inode from disk
 */
int read_inode(struct superblock *sb, uint inum, struct dinode *dip) {
    char buf[BSIZE];
    uint inode_block_num = (inum / IPB) + sb->inodestart;
    
    if (rblock(inode_block_num, buf) < 0) {
        printf("ERROR: failed to read inode block\n");
        return -1;
    }
    
    *dip = *((struct dinode *)buf + (inum % IPB));
    return 0;
}

/*
 * Check all blocks referenced by an inode, also verifies blocks are marked allocated in bitmap
 */
int check_inode_blocks(struct superblock *sb, struct dinode *dip) {
    // Check direct blocks
    for (int i = 0; i < NDIRECT; i++) {
        if (dip->addrs[i] != 0 ) {
            if(!is_valid_block(sb, dip->addrs[i])){
                printf("ERROR: bad address in inode\n");
                return -1;
            } 
            //New bitmap check
            int allocated = is_block_allocated(sb, dip->addrs[i]);
            if(allocated < 0){
                printf("ERROR: failed to read bitmap\n");
                return -1;
            }
            if(!allocated){
                printf("ERROR: address used by inode but marked free in bitmap\n");
                return -1;
            }
        }
    }

    // Check indirect block
    if (dip->addrs[NDIRECT] != 0) {
        if (!is_valid_block(sb, dip->addrs[NDIRECT])) {
            printf("ERROR: bad address in inode\n");
            return -1;
        }

        //Check indirect blocks allocation
        int allocated = is_block_allocated(sb, dip->addrs[NDIRECT]);
        if (allocated < 0){
            printf("ERROR: failed to read bitmap\n");
            return -1;
        }
        if(!allocated){
            printf("ERROR: address used by inode but marked free in bitmap\n");
            return -1;
        }
        
        char indirect_block[BSIZE];
        if (rblock(dip->addrs[NDIRECT], indirect_block) < 0) {
            printf("ERROR: failed to read indirect block\n");
            return -1;
        }

        uint *addrs = (uint *)indirect_block;
        for (int i = 0; i < NINDIRECT; i++) {
                if(addrs[i] != 0){
                    if(!is_valid_block(sb, addrs[i])){
                    printf("ERROR: bad address in inode\n");
                    return -1;}
                //Check data blocks allocation
                allocated = is_block_allocated(sb, addrs[i]);
                if(allocated < 0){
                    printf("ERROR: failed to read bitmap\n");
                    return -1;
                }
                if (!allocated) {
                    printf("ERROR: address used by inode but marked free in bitmap\n");
                    return -1;
                }
            }
        }
    }
    return 0;
}
    

/*
 * Check all inodes in filesystem
 */
int check_all_inodes(struct superblock *sb) {
    struct dinode dip;
    
    for (uint inum = 1; inum <= sb->ninodes; inum++) {
        if (read_inode(sb, inum, &dip) < 0) {
            return -1;
        }
        
        if (dip.type == 0) continue;  // Skip free inodes
        
        if (check_inode_blocks(sb, &dip) < 0) {
            return -1;
        }
    }
    return 0;
}

/*
 * Returns 1 if the given inode represents a directory; 0 otherwise.
 * Doing this because we shouldn't run functions on inodes that aren't directories
 */
int is_directory(struct dinode *dip) {
    return dip->type == T_DIR;
}

/*
 * Reads a block containing directory entries into the given dirent array.
 * Returns number of valid dirents found in the block.
 */
int read_dirent_block(uint blockno, struct dirent *entries, int max_entries) {
    char block_data[BSIZE];  // Holds one disk block's worth of data
    if (rblock(blockno, block_data) < 0) return -1; // Failed to read the block

    struct dirent *dir_entries = (struct dirent *)block_data;
    int n = BSIZE / sizeof(struct dirent);
    int count = 0;
    for (int i = 0; i < n && count < max_entries; i++) {
        if (dir_entries[i].inum != 0) { //if valid directory entry
            entries[count++] = dir_entries[i];
        }
    }
    return count; // Return number of valid entries
}

/*
 * Reads all valid directory entries for the given inode.
 * Returns the number of entries read or -1 on error.
 */
int read_all_dirents(struct superblock *sb, struct dinode *dip, struct dirent *entries, int max_entries) {
    int total = 0;

    // Direct blocks
    for (int i = 0; i < NDIRECT && total < max_entries; i++) {
        if (dip->addrs[i] == 0) continue; //skipping unused

        int n = read_dirent_block(dip->addrs[i], &entries[total], max_entries - total);
        if (n < 0) return -1;
        total += n; //for each direct we are trying to accumulate the valid entries
    }

    // Indirect block
    if (dip->addrs[NDIRECT] != 0 && total < max_entries) {
        uint indirect[NINDIRECT];
        if (rblock(dip->addrs[NDIRECT], indirect) < 0) return -1;

        for (int i = 0; i < NINDIRECT && total < max_entries; i++) {
            if (indirect[i] == 0) continue;

            int n = read_dirent_block(indirect[i], &entries[total], max_entries - total);
            if (n < 0) return -1;
            total += n;
        }
    }

    return total; // Returning num of valid dirents collected
}

/*
 * Returns 0 if the directory contains valid "." and ".." entries, else -1.
 * "." must point to its own inode number.
 * ".." must exist 
 */
int check_dot_and_dotdot(struct dirent *entries, int count, uint self_inum) {
    int found_dot = 0, found_dotdot = 0;

    for (int i = 0; i < count; i++) {
        if (strncmp(entries[i].name, ".", DIRSIZ) == 0) {
            // "." must point to self
            if (entries[i].inum != self_inum){
                return -1;
            }
            found_dot = 1;
        } else if (strncmp(entries[i].name, "..", DIRSIZ) == 0) { //checking if ".." exists
            found_dotdot = 1;
        }
    }

    if (found_dot && found_dotdot) {
        return 0;
    }
    return -1; // Missing one or both
    
}

/*
 * Iterates through all in-use inodes.
 * For each directory, ensures it contains a "." entry pointing to itself
 * and a ".." entry (but not checking parent validation here).
 * Returns 0 on successs or prints error and returns -1 on failure.
 */
int check_all_directory_formats(struct superblock *sb) {
    struct dinode dip;
    // Max number of entries we can store
    struct dirent entries[MAX_DIRENT_COUNT];

    for (uint inum = 1; inum < sb->ninodes; inum++) {
        if (read_inode(sb, inum, &dip) < 0){
            return -1; // Failed to read inode
        }
        if (dip.type == 0 || !is_directory(&dip)){
            continue; // Skipoing unused or non-directory inodes
        }

        int count = read_all_dirents(sb, &dip, entries, MAX_DIRENT_COUNT);
        if (count < 0) {
            printf("ERROR: failed to read directory entries\n");
            return -1;
        }

        // Checking "." and ".." for each
        if (check_dot_and_dotdot(entries, count, inum) < 0) {
            printf("ERROR: directory not properly formatted\n");
            return -1;
        }
    }

    return 0;
}

/*
 * Searches for ".." entry in a list of directory entries and returns the inode it points to.
 * Returns the inode number or -1 if ".." entry is not found.
 */
int get_dotdot_inum(struct dirent *entries, int count) {
    for (int i = 0; i < count; i++) {
        if (strncmp(entries[i].name, "..", DIRSIZ) == 0) {
            return entries[i].inum;  // Found "..", the supposed parent, and we should return its inum
        }
    }
    return -1;  // ".." not found
}

/*
 * Checks whether the given parent inode contains a directory entry that refers to the specified child inode.
 * Returns 1 if the parent directory contains a reference to the given child inode.
 * Returns 0 if not found or -1 on error.
 */
int is_child_referenced_in_parent(struct superblock *sb, uint parent_inum, uint child_inum) {
    struct dinode parent_dip;
    struct dirent parent_entries[MAX_DIRENT_COUNT];

    if (read_inode(sb, parent_inum, &parent_dip) < 0)
        return -1; // Could not read the inode so error

    if (!is_directory(&parent_dip))
        return -1;  // Making sure that parent must be a directory

    int count = read_all_dirents(sb, &parent_dip, parent_entries, MAX_DIRENT_COUNT);
    // Load all directory entries from the parent's data blocks

    if (count < 0){
        return -1;
    }

    // Looking through the directory entries to find one that points to the child
    for (int i = 0; i < count; i++) {
        if (parent_entries[i].inum == child_inum) {
            return 1;  // Found child in parent
        }
    }

    return 0;  // Not found
}

/*
 * Verifies that each directory's ".." entry points to the correct parent inode and that parent directory also references that child.
 * Returns 0 if all relationships are valid and -1 if there happens to be an error.
 */
int check_parent_directory_mismatch(struct superblock *sb) {
    struct dinode dip;
    struct dirent entries[MAX_DIRENT_COUNT];

    // For all inodes in the filesystem
    for (uint inum = 1; inum < sb->ninodes; inum++) {
        if (read_inode(sb, inum, &dip) < 0){
        return -1;
        }

        // Skip unused inodes or non-directory inodes
        if (dip.type == 0 || !is_directory(&dip)) {
            continue;
        }
        
        // Load all directory entries for this current directory inode of the for loop
        int count = read_all_dirents(sb, &dip, entries, MAX_DIRENT_COUNT);
        if (count < 0) {
            printf("ERROR: failed to read directory entries\n");
            return -1;
        }

        // Root inode always has itself as ".."
        if (inum == ROOTINO){
            continue;
        }

        // Getting the ".." inode number of the the directory's supposed parent we are checking
        int parent_inum = get_dotdot_inum(entries, count);
        if (parent_inum <= 0 || parent_inum >= sb->ninodes) {
            printf("ERROR: parent directory mismatch\n");
            return -1;
        }

        // Then checking if the claimed parent contains a reference to this directory
        int referenced = is_child_referenced_in_parent(sb, parent_inum, inum);
        if (referenced < 0) {
            printf("ERROR: parent directory mismatch\n");
            return -1;
        } else if (referenced == 0) {
            printf("ERROR: parent directory mismatch\n");
            return -1;
        }
    }

    return 0; //if directories all pass
}

/*
 * Allocates and builds a map of all inodes referenced in directory entries.
 * Returns a pointer to the map. This will be a NULL on error.\
 * Referenced[i] == 1 if inode i is used in a dir
 * Caller is responsible for freeing the returned array.
 */
int *build_inode_reference_map(struct superblock *sb) {
    int *referenced = malloc(sb->ninodes * sizeof(int));
    if (!referenced) {
        perror("malloc");
        return NULL;
    }

    // Setting them all to 0
    for (uint i = 0; i < sb->ninodes; i++) {
        referenced[i] = 0;
    }

    struct dinode dip;
    struct dirent entries[MAX_DIRENT_COUNT];

    // Iterate through all inodes
    for (uint dir_inum = 1; dir_inum < sb->ninodes; dir_inum++) {
        if (read_inode(sb, dir_inum, &dip) < 0) {
            free(referenced);
            return NULL;
        }

        // Skipping unused or non-directory inodes
        if (dip.type == 0 || !is_directory(&dip))
            continue;

        // We here are reading all valid dirents from this specific directory
        int count = read_all_dirents(sb, &dip, entries, MAX_DIRENT_COUNT);
        if (count < 0) {
            printf("ERROR: failed to read directory entries\n");
            free(referenced);
            return NULL;
        }

        // For each dirent we mark the referred inode as referenced
        for (int i = 0; i < count; i++) {
            uint ref_inum = entries[i].inum;
            if (ref_inum > 0 && ref_inum < sb->ninodes) {
                referenced[ref_inum] = 1;
            }
        }
    }

    return referenced;
}

/*
* Builds a map tracking which blocks are referenced by inodes 
* returns a pointer to map or NULL on error
* Caller must free the returned array 
*/
int *build_block_reference_map(struct superblock *sb){
    int *referenced = malloc(sb->size * sizeof(int));
    if(!referenced){
        perror("malloc");
        return NULL;
    }

    // Initialize all entries to 0
    for(uint i = 0; i < sb->size; i++){
        referenced[i] = 0;
    }

    // Mark essential system blocks as referenced:
    
    // 1. Superblock (block 1)
    referenced[1] = 1;
    
    // 2. Log blocks (from logstart to logstart + nlog)
    for(uint b = sb->logstart; b < sb->logstart + sb->nlog; b++){
        if(b < sb->size) referenced[b] ++;
    }
    
    // 3. Bitmap blocks (from bmapstart to inodestart-1)
    uint bitmap_blocks = (sb->size + BSIZE*8 - 1) / (BSIZE*8);
    for(uint b = sb->bmapstart; b < sb->bmapstart + bitmap_blocks && b < sb->size; b++){
        referenced[b] ++;
    }

    // 4. Inode blocks (from inodestart to bmapstart-1)
    uint inode_blocks = (sb->ninodes + IPB - 1) / IPB;
    for(uint b = sb->inodestart; b < sb->inodestart + inode_blocks && b < sb->size; b++){
        referenced[b] ++;
    }

    // Now scan all inodes for their data block references
    struct dinode dip;
    for(uint inum = 1; inum < sb->ninodes; inum++){
        if(read_inode(sb, inum, &dip) < 0){
            free(referenced);
            return NULL;
        }

        if(dip.type == 0) continue; // Skip free inodes

        // Check direct blocks
        for(int i = 0; i < NDIRECT; i++){
            if(dip.addrs[i] != 0){
                if(dip.addrs[i] >= sb->size) {
                    free(referenced);
                    return NULL;
                }
                referenced[dip.addrs[i]] ++;
            }
        }

        // Check indirect block
        if(dip.addrs[NDIRECT] != 0){
            // Validate the indirect block number is within filesystem bounds
            if(dip.addrs[NDIRECT] >= sb->size) {
                free(referenced);
                return NULL;
            }
            referenced[dip.addrs[NDIRECT]] ++;
            
            // Read the indirect block into memory
            uint indirect[NINDIRECT];
            if(rblock(dip.addrs[NDIRECT], indirect) < 0){
                free(referenced);
                return NULL;
            }

            // Traverse all entries in the indirect block
            for(int i = 0; i < NINDIRECT; i++){
                // Skip unused blocks (zero address)
                if(indirect[i] != 0){
                    // Validate the data block number is within filesystem bounds
                    if(indirect[i] >= sb->size) {
                        free(referenced);
                        return NULL; // Error: Failed to read indirect block
                    }
                    referenced[indirect[i]] ++;
                }
            }
        }
    }
    return referenced;
}

/*
 * Verifies that each used inode is referenced by at least one directory entry which means that the type!=0
 * We are using build_inode_reference_map() helper function to determine which inodes are referred to
 * Returns 0 if all in-use inodes are found in directories or prints an error and returns -1.
 */
int check_used_inode_found_in_directory(struct superblock *sb) {
    // Getting the map of inodes that are referenced from helper
    int *referenced = build_inode_reference_map(sb);
    if (!referenced) return -1;

    struct dinode dip;

    // Going through inodes
    for (uint inum = 1; inum < sb->ninodes; inum++) {
        if (read_inode(sb, inum, &dip) < 0) {
            free(referenced);
            return -1;
        }

        // If its in use...
        if (dip.type != 0 && !referenced[inum]) { // But not marked in the map...
            printf("ERROR: inode marked used but not found in a directory\n"); // We print the corresponding error
            free(referenced);
            return -1;
        }
    }

    free(referenced);
    return 0;
}

/*
 * Verify all blocks marked in-use in bitmap are actually referenced
 * Returns 0 if valid, -1 on error with message printed
 */
int check_referenced_blocks(struct superblock *sb) {
    // Generate a map tracking which blocks are referenced by inodes/metadata
    int *referenced = build_block_reference_map(sb);
    if (!referenced) return -1;

     // Iterate through all blocks (skip block 0, reserved for boot)
    for (uint blockno = 1; blockno < sb->size; blockno++) {
        int allocated = is_block_allocated(sb, blockno);
        if (allocated < 0) {
            free(referenced);
            return -1;
        }

        // Error if block is allocated in bitmap but not referenced anywhere
        if (allocated && referenced[blockno] == 0) {
            printf("ERROR: bitmap marks block in use but it is not in use\n");
            free(referenced);
            return -1;
        }
    }

    free(referenced);
    return 0;
}

/*
 * Verify no block is referenced by more than one inode
 * Returns 0 if valid, -1 on error with message printed
 */
int check_multiply_referenced_blocks(struct superblock *sb) {
    // Generate a map tracking how many times each block is referenced
    int *referenced = build_block_reference_map(sb);
    if (!referenced) return -1;

    // Only check data blocks (after inode blocks)
    uint start_block = sb->inodestart + ((sb->ninodes + IPB - 1) / IPB);
    
    // Scan all data blocks (from start_block to sb->size - 1)
    for (uint blockno = start_block; blockno < sb->size; blockno++) {
        if (referenced[blockno] > 1) {
            printf("ERROR: address used more than once\n");
            free(referenced);
            return -1;
        }
    }
    free(referenced);
    return 0;
}

/*
 * Verifies that each inode referenced in any directory is actually marked in-use.
 * We get reference map from build_inode_reference_map() helper
 * Returns 0 if all dirent inodes are valid or prints an error and returns -1.
 */
int check_dirent_refers_to_allocated_inode(struct superblock *sb) {
    // Get map
    int *referenced = build_inode_reference_map(sb);
    if (!referenced) return -1;

    struct dinode dip;

    // Check all inodes that are referenced in directories
    for (uint inum = 1; inum < sb->ninodes; inum++) {
        // If this inode was referenced
        if (referenced[inum]) {
            if (read_inode(sb, inum, &dip) < 0) {
                free(referenced);
                return -1;
            }

            // Here we make sure it's actually in use
            if (dip.type == 0) {
                printf("ERROR: inode referred to in directory but marked free\n");
                free(referenced);
                return -1;
            }
        }
    }

    free(referenced);
    return 0;
}




int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s DISKFILE.img\n", argv[0]);
        return 1;
    }

    fsfd = open(argv[1], O_RDONLY);
    if (fsfd < 0) {
        perror(argv[1]);
        return 1;
    }

    // Read superblock
    char sbbuf[BSIZE];
    struct superblock sb;
    if (rblock(SUPERBLOCK, sbbuf) < 0) {
        printf("Failed to read superblock\n");
        close(fsfd);
        return 1;
    }
    memcpy(&sb, sbbuf, sizeof(sb));

    // Verify magic number
    if (sb.magic != FSMAGIC) {
        printf("ERROR: bad magic number in superblock\n");
        close(fsfd);
        return 1;
    }

    // Check all inodes
    if (check_all_inodes(&sb) < 0) {
        close(fsfd);
        return 1;
    }

    if (check_all_directory_formats(&sb) < 0) {
        close(fsfd);
        return 1;
    }
   
    if (check_dirent_refers_to_allocated_inode(&sb) < 0) {
        close(fsfd);
        return 1;
    }

    if (check_multiply_referenced_blocks(&sb) < 0) {
        close(fsfd);
        return 1;
    }

    if (check_referenced_blocks(&sb) < 0) {
        close(fsfd);
        return 1;
    }

    if (check_used_inode_found_in_directory(&sb) < 0) {
        close(fsfd);
        return 1;
    }

    if (check_parent_directory_mismatch(&sb) < 0) {
        close(fsfd);
        return 1;
    }

    close(fsfd);
    return 0;
}
