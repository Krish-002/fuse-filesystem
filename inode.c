#include <stdio.h>
#include <math.h>
#include <time.h>

#include "inode.h"
#include "blocks.h"
#include "bitmap.h"

// prints the details of an inode
void print_inode(inode_t* node)
{
    printf("inode.refs = %d\n", node->refs); // print refrence count
    printf("inode.mode = %d\n", node->mode); // print mode
    printf("inode.size = %d\n", node->size); // print size
    printf("inode.block = %d\n", node->block); // print starting block
    int curr_block = node->block;
    while (curr_block != block_list[curr_block]) { // loop through the block list
        curr_block = block_list[curr_block];
        printf("\tblock_list = %d\n", curr_block); // print each block in the list
    }

    // print access, modification, and change times
    printf("inode.atime = %d\n", node->atime);
    printf("inode.mtime = %d\n", node->mtime);
    printf("inode.ctime = %d\n", node->ctime);
}

// gets an inode given its number
inode_t* get_inode(int inum)
{
    inode_t* inodeArray = get_inode_bitmap() + BLOCK_BITMAP_SIZE; // get inode array
    return &inodeArray[inum]; // return the inode
}

// alloctes an inode and initialzes it
int alloc_inode()
{
    void *inodeBitmap = get_inode_bitmap(); // get inode bitmap   
    int allocatedInode = 0;
    for (int inodeIndex = 0; inodeIndex < BLOCK_COUNT; ++inodeIndex) { // find a free inode
        if (!bitmap_get(inodeBitmap, inodeIndex)) {
            bitmap_put(inodeBitmap, inodeIndex, 1); // mark the inode as used
            printf("+ alloc_inode() -> %d\n", inodeIndex);
            allocatedInode = inodeIndex;
            break;
        }
    }
    inode_t* new_node = get_inode(allocatedInode); // get the new inode
    new_node->refs = 1; // set reference count
    new_node->mode = 0; // set mode
    new_node->size = 0; // set size
    new_node->block = alloc_block(); // allocate a block
    new_node->atime = 
        new_node->ctime = 
        new_node->mtime = time(NULL); // set access, modifiction, and change times
    return allocatedInode; // return inode number
}

// frees an inode
void free_inode(int inum)
{
    printf("+ free_inode(%d)\n", inum);

    inode_t* node = get_inode(inum);
    shrink_inode(node, 0); // shrink the inode size to 0
    free_block(node->block); // free the block

    void *inodeBitmap = get_inode_bitmap();
    bitmap_put(inodeBitmap, inum, 0); // mark inode as free in bitmap
}

// grows an inode to a specfied size
int grow_inode(inode_t* node, int size)
{
    int end_block = node->block; // find the last block
    while (end_block != block_list[end_block])
        end_block = block_list[end_block];

    int requiredBlocks = (size - 1) / BLOCK_SIZE - (node->size - 1) / BLOCK_SIZE; // clacualte needed blocks
    for (int i = 0; i < requiredBlocks; i++) // allocate new blocks
    {
        int inum = alloc_block();
        block_list[end_block] = inum; // link the new block
        end_block = inum;
    }

    node->size = size; // update size
    return 0; // return success
}

// shrinks an inode to a specified size
int shrink_inode(inode_t* node, int size)
{
    int targetBlockCount = size / BLOCK_SIZE; // calculate the number of blocks after shrinking

    int i = 0;
    int currentBlock = node->block;
    while (currentBlock != block_list[currentBlock]) // loop through blocks
    {
        if (i > targetBlockCount) // if beynd new size
        {
            free_block(currentBlock); // free the block
            int nextBlock = block_list[currentBlock]; // move to next block
            currentBlock = nextBlock;
        }
        i++;
    }

    node->size = size; // set new size
    return 0; // return success
}

// gets the phyiscal block number for a file pointer number in an inode
int inode_get_pnum(inode_t* node, int fpn)
{
    int targetBlockIndex = fpn / BLOCK_COUNT; // calculate block number
    int block = node->block;
    for (int i = 0; i < targetBlockIndex; i++) // traverse to the corresponding block
        block = block_list[block];
    return block; // return the block number
}
