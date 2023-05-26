#include "file.h"
#include "spinlock.h"

// Global file table
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

// Init file table
void fileinit(void) { init_spinlock(&ftable.lock, "ftable"); }

// Allocate a file structure.
struct file *filealloc(void) {
  struct file *f;
  acquire_spinlock(&ftable.lock);

  for (f = ftable.file; f < ftable.file + NFILE; f++) {
    if (f->ref == 0) {
      f->ref = 1;
      release_spinlock(&ftable.lock);
      return f;
    }
  }

  release_spinlock(&ftable.lock);
  return 0;
}

// Increase ref count for file f.
struct file *filedup(struct file *f) {
  acquire_spinlock(&ftable.lock);

  if (f->ref < 1) {
    printf("panic: filedup");
    exit(1);
  }

  f->ref++;

  release_spinlock(&ftable.lock);
  return f;
}

// Close file f.  (Decrease ref count, close when reaches 0.)
void fileclose(struct file *f) {
  struct file ff;
  acquire_spinlock(&ftable.lock);

  if (f->ref < 1) {
    printf("panic: fileclose");
    exit(1);
  }

  (f->ref)--;
  if (f->ref > 0) {
    release_spinlock(&ftable.lock);
    return;
  }

  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release_spinlock(&ftable.lock);

  if (ff.type == FD_PIPE) { // TODO
  } else if (ff.type == FD_INODE || ff.type == FD_DEVICE) {
    begin_op();
    iput(ff.ip);
    end_op();
  } else {
    printf("panic: fileread");
    exit(1);
  }
}

// Get metadata about file f.
// addr is a address, pointing to a struct stat.
int filestat(struct file *f, void *addr) {
  struct stat st;

  if (f->type == FD_INODE || f->type == FD_DEVICE) {
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if (memcpy(addr, (char *)&st, sizeof(st)) == NULL)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f to addr
int fileread(struct file *f, void *addr, int n) {
  int r = 0;

  if (f->readable == 0)
    return -1;

  if (f->type == FD_PIPE) {          // TODO
  } else if (f->type == FD_DEVICE) { // TODO
  } else if (f->type == FD_INODE) {
    ilock(f->ip);
    if ((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  } else {
    printf("panic: fileread");
    exit(1);
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int filewrite(struct file *f, void *addr, int n) {
  int r, ret = 0;

  if (f->writable == 0)
    return -1;

  if (f->type == FD_PIPE) {          // TODO
  } else if (f->type == FD_DEVICE) { // TODO
  } else if (f->type == FD_INODE) {
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLKS - 1 - 1 - 2) / 2) * BSIZE;
    int i = 0;
    while (i < n) {
      int n1 = n - i;
      if (n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, (void *)((uint64)addr + i), f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if (r != n1) {
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    printf("panic: filewrite");
    exit(1);
  }

  return ret;
}
