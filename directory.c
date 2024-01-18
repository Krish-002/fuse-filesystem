#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "directory.h"
#include "bitmap.h"
#include "inode.h"
#include "slist.h"

// Initializes the root directory.
void directory_init()
{
    inode_t* root_node = get_inode(alloc_inode());
    root_node->mode = 040755; // Set permissions to root node.
}

// Looks up a directory entry by name.
int directory_lookup(inode_t *dd, const char *name)
{
    if (!strcmp(name, "")) // If the name is empty, return 0.
        return 0;
    dirent_t* dirs = blocks_get_block(dd->block); // Get the block of directories.
    for (int ii = 0; ii < 64; ii++) // Loop through directory entries.
        if (!strcmp(name, dirs[ii].name)) // If the name matches, return inode number.
            return dirs[ii].inum;
    return -ENOENT; // If not found, return an error.
}

// Looks up a file or directory in the tree structure.
int tree_lookup(const char *path)
{
    slist_t* path_slist = slist_explode(path, '/'); // Break the path into parts.
    slist_t* pt_slist = path_slist; // Temp variable for iteration.
    int inum = 0;
    while (pt_slist) // Loop through the path segments.
    {
        inum = directory_lookup(get_inode(inum), pt_slist->data); // Look up each segment.
        if (inum < 0) // If lookup fails, clean up and return error.
        {
            slist_free(path_slist);
            return -ENOENT; 
        }
        pt_slist = pt_slist->next; // Move to the next segment.
    }
    slist_free(path_slist); // Free the path list.

    return inum; // Return the inode number.
}

// Adds a new entry to a directory.
int directory_put(inode_t *dd, const char *name, int inum)
{
    dirent_t newEntry; // Create a new directory entry.
    strncpy(newEntry.name, name, DIR_NAME_LENGTH); // Set the name of the directory.
    newEntry.inum = inum; // Set the inode number.
    int existingEntriesCount = dd->size / sizeof(dirent_t); // Calculate number of existing directories.
    dirent_t* dirs = blocks_get_block(dd->block); // Get the block of directories.
    dirs[existingEntriesCount] = newEntry; // Add the new directory to the end.
    dd->size += sizeof(dirent_t); // Increase the size of the directory.
    return 0; // Return success.
}

// Deletes an entry from a directory.
int directory_delete(inode_t *dd, const char *name)
{
    int entriesCount = dd->size / sizeof(dirent_t); // Calculate number of directories.
    dirent_t* dirs = blocks_get_block(dd->block); // Get the block of directories.
    for (int entryIndex = 0; entryIndex < entriesCount; entryIndex++) // Loop through directories.
        if (!strcmp(dirs[entryIndex].name, name)) // If the name matches.
        {
            // Decrease the inode's reference count.
            inode_t* node = get_inode(dirs[entryIndex].inum);
            node->refs--;
            if (node->refs <= 0) // If no more references, free the inode.
                free_inode(dirs[entryIndex].inum);
            // Shift remaining directory entries.
            for (int shiftIndex = entryIndex; shiftIndex < entriesCount - 1; shiftIndex++)
                dirs[shiftIndex] = dirs[shiftIndex + 1];
            dd->size -= sizeof(dirent_t); // Decrease the directory size.
            return 0; // Return success.
        }
    return -ENOENT; // If not found, return an error.
}

// Lists all entries in a directory.
slist_t *directory_list(const char *path)
{
    int inum = tree_lookup(path); // Look up the inode number for the path.
    inode_t* node = get_inode(inum); // Get the inode.
    int entriesCount = node->size / sizeof(dirent_t); // Calculate number of directories.
    dirent_t* dirs = blocks_get_block(node->block); // Get the block of directories.

    slist_t* ret = NULL; // Initialize return list.
    for (int entryIndex = 0; entryIndex < entriesCount; entryIndex++) // Loop through directories.
        ret = slist_cons(dirs[entryIndex].name, ret); // Add each directory name to the list.
    return ret; // Return the list of directory names.
}

// Prints the contents of a directory.
void print_directory(inode_t *dd)
{
    int entriesCount = dd->size / sizeof(dirent_t); // Calculate number of directories.
    dirent_t* dirs = blocks_get_block(dd->block); // Get the block of directories.
    for (int entryIndex = 0; entryIndex < entriesCount; entryIndex++) // Loop through directories.
    {
        printf("Dir %d:\n", entryIndex); // Print directory index.
        printf("Name: %s\n", dirs[entryIndex].name); // Print directory name.
        printf("Inum: %d\n", dirs[entryIndex].inum); // Print inode number.
    }
}
