#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "storage.h"
#include "blocks.h"
#include "bitmap.h"
#include "inode.h"
#include "directory.h"

static void set_parent_child(const char* path, char* parent, char* child);
// finds minimum of two integers
static int min(int y, int z) {
    return y > z ? z : y;
}

// initiliazes storage with the given path
void storage_init(const char* path)
{
    // initialize blocks with path
    blocks_init(path);

    // ckhecks and allocates inode blocks if not already allocated
    if (!bitmap_get(get_blocks_bitmap(), 1))
        for (int i = 0; i < 3; i++)
        {
            int new_block = alloc_block();
            printf("alloc inode block: %d\n", new_block);
        }

    // initializes root directory if not alreayd initialized
    if (!bitmap_get(get_blocks_bitmap(), 4))
    {
      printf("initializing root directory\n");
        directory_init();
    }
}

// gets file status
int storage_stat(const char *path, struct stat *st)
{
    // lookup inode and set stats if inode exists
    int inodeNumber = tree_lookup(path);
    if (inodeNumber > 0)
    {
        inode_t* node = get_inode(inodeNumber);
        // setting various stats
        st->st_size = node->size;
        st->st_mode = node->mode;
        st->st_nlink = node->refs;
        st->st_atime = node->atime;
        st->st_ctime = node->ctime;
        st->st_mtime = node->mtime;
        return 0;
    }
    return -ENOENT; 
}

// reads data from storgae
int storage_read(const char *path, char *buf, size_t size, off_t offset)
{
    // gets inode and reads data into buffer
    inode_t* node = get_inode(tree_lookup(path));

    // indexes for buffer and source, and the size to read
    int bufferIndex = 0;
    int sourceIndex = offset;
    int bytesToRead = size;

    // loop to read data in blocks
    while (bytesToRead > 0)
    {
        // gets the block and calculates the size to copy
        char* src = blocks_get_block(inode_get_pnum(node, sourceIndex));
        src += sourceIndex % BLOCK_SIZE;
        int copy_size = min(bytesToRead, BLOCK_SIZE - (sourceIndex % BLOCK_SIZE));
        memcpy(buf + bufferIndex, src, copy_size);
        // updating indexes and remaining size
        bufferIndex += copy_size;
        sourceIndex += copy_size;
        bytesToRead -= copy_size;
    }

    return size;
}

// wirtes data to storage
int storage_write(const char *path, const char *buf, size_t size, off_t offset)
{
    // gets inode and extends it if needed
    inode_t* node = get_inode(tree_lookup(path));
    if (node->size < size + offset)
        storage_truncate(path, size + offset);

    // indexes for buffer and destination, and the size to write
    int bufferIndex = 0;
    int destinationIndex = offset;
    int bytesToWrite = size;

    // loop to write data in blocks
    while (bytesToWrite > 0)
    {
        // gets the block and calculates the size to copy
        char* dest = blocks_get_block(inode_get_pnum(node, destinationIndex));
        dest += destinationIndex % BLOCK_SIZE;
        int copy_size = min(bytesToWrite, BLOCK_SIZE - (destinationIndex % BLOCK_SIZE));
        memcpy(dest, buf + bufferIndex, copy_size);
        // updating indexes and remaining size
        bufferIndex += copy_size;
        destinationIndex += copy_size;
        bytesToWrite -= copy_size;
    }

    return size;
}

// truncates a file to a specified size
int storage_truncate(const char *path, off_t size)
{
    // gets inode and adjusts its size
    inode_t* node = get_inode(tree_lookup(path));
    if (node->size < size)
        grow_inode(node, size);
    else
        shrink_inode(node, size);
    return 0;
}

// creates a new file node
int storage_mknod(const char *path, mode_t mode)
{
    // checks if file exists and returns error if it does
    if (tree_lookup(path) != -ENOENT)
        return -EEXIST;
    
    // allocates memory for child and parent paths
    char* child = (char*)malloc(DIR_NAME_LENGTH + 2);
    char* parent = (char*)malloc(strlen(path));
    set_parent_child(path, parent, child);

    // looks up parent inode and returns error if not found
    int parentInodeNumber = tree_lookup(parent);
    if (parentInodeNumber < 0)
    {
        // freeing allocated memory before returning
        free(child);
        free(parent);
        return -ENOENT;
    }
    // gets parent inode
    inode_t* parent_node = get_inode(parentInodeNumber);

    // alloctes new inode and sets its properties
    int newInodeNumber = alloc_inode();
    inode_t* node = get_inode(newInodeNumber);
    node->mode = mode;
    node->size = 0;
    node->refs = 1;

    // adds new inode to parent directory and frees memory
    directory_put(parent_node, child, newInodeNumber);
    free(child);
    free(parent);
    return 0;
}

// removes a file link
int storage_unlink(const char *path)
{
    // allocates memry for child and parent paths
    char* child = (char*)malloc(DIR_NAME_LENGTH + 2);
    char* parent = (char*)malloc(strlen(path));
    set_parent_child(path, parent, child);

    // gets parent inode and deletes directory entry
    inode_t* parent_node = get_inode(tree_lookup(parent));
    int unlinkResult = directory_delete(parent_node, child);

    // freeing allocated memory
    free(child);
    free(parent);

    return unlinkResult;
}

// creates a hard link to a file
int storage_link(const char *from, const char *to)
{
    // checks if target exists and returns error if it doesn't
    int inodeNumber = tree_lookup(to);
    if (inodeNumber < 0)
        return inodeNumber;

    // allocates memory for child and parent paths
    char* child = (char*)malloc(DIR_NAME_LENGTH + 2);
    char* parent = (char*)malloc(strlen(from));
    set_parent_child(from, parent, child);

    // gets parent inode and adds a new directory entry
    inode_t* parent_node = get_inode(tree_lookup(parent));
    directory_put(parent_node, child, inodeNumber);
    get_inode(inodeNumber)->refs++;
    
    // freeing allocated memory
    free(child);
    free(parent);

    return 0;
}

// renames a file
int storage_rename(const char *from, const char *to)
{
    // links new name to file and unlinks old name
    storage_link(to, from);
    storage_unlink(from);
    return 0;
}

// sets access and modifcation times of a file
int storage_set_time(const char *path, const struct timespec ts[2])
{
    // looks up inode and sets times if inode exists
    int inodeNumber = tree_lookup(path);
    if (inodeNumber < 0)
        return -ENOENT; 
    inode_t* node = get_inode(inodeNumber);
    node->atime = ts[0].tv_sec;
    node->mtime = ts[1].tv_sec;
    return 0;
}

// lists contnts of a directoy
slist_t *storage_list(const char *path)
{
    // returns list of directory entries
    return directory_list(path);
}

// updates the creation time of a file
int storage_ctime(const char* path)
{
    // looks up inode and sets creation time if inode exists
    int inodeNumber = tree_lookup(path);
    if (inodeNumber < 0)
        return -ENOENT; 
    get_inode(inodeNumber)->ctime = time(NULL);
    return 0;
}

// changes file mode
int storage_chmod(const char* path, mode_t mode)
{
    // looks up inode and changes mode if inode exists
    int inodeNumber = tree_lookup(path);
    if (inodeNumber < 0)
        return -ENOENT;
    get_inode(inodeNumber)->mode &= ~07777 & mode;
    return 0;
}

// sets parent and child path from given path
static void set_parent_child(const char* path, char* parent, char* child)
{
    // explodes path into list, and constructs parent and child strings
    slist_t* path_slist = slist_explode(path, '/');
    slist_t* pt_slist = path_slist;
    parent[0] = '\0';
    while (pt_slist->next)
    {
        strncat(parent, "/", 1);
        strncat(parent, pt_slist->data, DIR_NAME_LENGTH);
        pt_slist = pt_slist->next;
    }
    memcpy(child, pt_slist->data, strlen(pt_slist->data));
    child[strlen(pt_slist->data)] = '\0';
    slist_free(path_slist);
}

