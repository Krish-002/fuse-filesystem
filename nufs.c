// based on cs3650 starter code

#include <assert.h>
#include <bsd/string.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "inode.h"
#include "storage.h"
#include "directory.h"


// implementation for: man 2 access
// Checks if a file exists.
int nufs_access(const char *path, int mask) {
  int access_result = tree_lookup(path); // look up the file in the tree

  // Only the root directory and our simulated file are accessible for now...
  if (access_result >= 0) {
    inode_t* node = get_inode(access_result); // get inode if file exists
    node->atime = time(NULL);
    access_result = 0;
  } else { // ...others do not exist
    access_result = -ENOENT;
  }

  printf("access(%s, %04o) -> %d\n", path, mask, access_result);
  return access_result;
}

// Gets an object's attributes (type, permissions, size, etc).
// Implementation for: man 2 stat
// This is a crucial function.
int nufs_getattr(const char *path, struct stat *st) {
  int attr_result = 0;

  // Return some metadata for the root directory...
  if (strcmp(path, "/") == 0) {
    st->st_mode = 040755; // directory
    st->st_size = 0;
    st->st_uid = getuid(); // get user id
    st->st_nlink = 1; // number of links
  } else {
    attr_result = storage_stat(path, st); // get stats for non-root paths
    st->st_uid = getuid(); // get user id
  }
  printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n", path, attr_result, st->st_mode,
         st->st_size);
  return attr_result;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
  struct stat st;
  int dir_read_result;

  dir_read_result = nufs_getattr(path, &st); // get attributes of the directory
  assert(dir_read_result == 0);

  filler(buf, ".", &st, 0); // add current directory to the buffer

  slist_t* path_slist = storage_list(path); // list paths in the directory
  if (!path_slist)
  {
    printf("readdir(%s) -> %d\n", path, dir_read_result);
    return 0;
  }
  slist_t* pt_slist = path_slist;

  int path_len = strlen(path);
  while (pt_slist)
  { // loop through each path in the directory
    char* temp_path = (char*)malloc(path_len + 50);
    strncpy(temp_path, path, path_len);
    // handle directory slash
    if (path[path_len - 1]  == '/')
      temp_path[path_len] = '\0';
    else
    {
      temp_path[path_len] = '/';
      temp_path[path_len + 1] = '\0';
    }
    strncat(temp_path, pt_slist->data, DIR_NAME_LENGTH); // concatenate the directory name
    nufs_getattr(temp_path, &st); // get attributes for each item
    filler(buf, pt_slist->data, &st, 0); // add each item to the buffer

    free(temp_path);
    pt_slist = pt_slist->next; // go to next item
  }

  printf("readdir(%s) -> %d\n", path, dir_read_result);
  slist_free(path_slist);
  return dir_read_result;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function.
int nufs_mknod(const char *path, mode_t mode, dev_t rdev) {
  int mknod_result = storage_mknod(path, mode); // create a new node
  printf("mknod(%s, %04o) -> %d\n", path, mode, mknod_result);
  return mknod_result;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int nufs_mkdir(const char *path, mode_t mode) {
  int mkdir_result = nufs_mknod(path, mode | 040000, 0); // create a directory
  printf("mkdir(%s) -> %d\n", path, mkdir_result);
  return mkdir_result;
}

int nufs_unlink(const char *path) {
  int unlink_result = storage_unlink(path); // unlink a file
  printf("unlink(%s) -> %d\n", path, unlink_result);
  return unlink_result;
}

int nufs_link(const char *from, const char *to) {
  int link_result = storage_link(from, to); // create a hard link
  printf("link(%s => %s) -> %d\n", from, to, link_result);
  return link_result;
}

int nufs_rmdir(const char *path) {
  int rmdir_result = -1; // rmdir not implemented yet
  printf("rmdir(%s) -> %d\n", path, rmdir_result);
  return rmdir_result;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char *from, const char *to) {
  int rename_result = storage_rename(from, to); // rename or move a file
  storage_ctime(to); // update change time
  printf("rename(%s => %s) -> %d\n", from, to, rename_result);
  return rename_result;
}

int nufs_chmod(const char *path, mode_t mode) {
  int chmod_result = storage_chmod(path, mode); // change file mode
  storage_ctime(path); // update time
  printf("chmod(%s, %04o) -> %d\n", path, mode, chmod_result);
  return chmod_result;
}

int nufs_truncate(const char *path, off_t size) {
  int truncate_result = storage_truncate(path, size); // truncate a file to a specific size
  storage_ctime(path); // update change time
  printf("truncate(%s, %ld bytes) -> %d\n", path, size, truncate_result);
  return truncate_result;
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
int nufs_open(const char *path, struct fuse_file_info *fi) {
  int open_result = 0; // check access
  printf("open(%s) -> %d\n", path, open_result);
  return open_result;
}

// Actually read data
int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
  int read_result = storage_read(path, buf, size, offset); // read data from a file
  printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, read_result);
  return read_result;
}

// Actually write data
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  int write_result = storage_write(path, buf, size, offset); // write data to file
  printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, write_result);
  return write_result;
}

// Update the timestamps on a file or directory.
int nufs_utimens(const char *path, const struct timespec ts[2]) {
  int utimens_result = storage_set_time(path, ts); // set time property for file
  storage_ctime(path); // update change time
  printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n", path, ts[0].tv_sec,
         ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, utimens_result);
  return utimens_result;
}

// Extended operations
int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data) {
  int ioctl_result = -1; // ioctl not implemented
  printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, ioctl_result);
  return ioctl_result;
}

void nufs_init_ops(struct fuse_operations *ops) {
  memset(ops, 0, sizeof(struct fuse_operations));
  ops->access = nufs_access;
  ops->getattr = nufs_getattr;
  ops->readdir = nufs_readdir;
  ops->mknod = nufs_mknod;
  // ops->create   = nufs_create; // alt to mknod
  ops->mkdir = nufs_mkdir;
  ops->link = nufs_link;
  ops->unlink = nufs_unlink;
  ops->rmdir = nufs_rmdir;
  ops->rename = nufs_rename;
  ops->chmod = nufs_chmod;
  ops->truncate = nufs_truncate;
  ops->open = nufs_open;
  ops->read = nufs_read;
  ops->write = nufs_write;
  ops->utimens = nufs_utimens;
  ops->ioctl = nufs_ioctl;
};

struct fuse_operations nufs_ops;

int main(int argc, char *argv[]) {
  assert(argc > 2 && argc < 6);
  printf("TODO: mount %s as data file\n", argv[argc - 1]);
  storage_init(argv[--argc]);
  nufs_init_ops(&nufs_ops);
  return fuse_main(argc, argv, &nufs_ops, NULL);
}
