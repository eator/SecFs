#ifndef FILE_H
#define FILE_H

#include "defs.h"
#include "fs.h"
#include "sleeplock.h"

#define major(dev) ((dev) >> 16 & 0xFFFF) // fetch major no
#define minor(dev) ((dev)&0xFFFF)         // fetch minor no
#define mkdev(m, n)                                                            \
  ((uint)((m) << 16 | (n))) // make uint dev no by combine major and minor

// File structure for process
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};

// In-memory inode structure
struct inode {
  uint dev;  // dev no
  uint inum; // inode no
  int ref;   // reference counter
  struct sleeplock lock;
  // protect everything below the lock(valid to addrs)
  int valid; // Is the copy of disk node valid?

  short type;              // file type (dir or file)
  short nlink;             // numbers of links to the disk inode
  short major;             // major device no
  short minor;             // minor device no
  uint size;               // file size(bytes)
  uint addrs[NDIRECT + 1 + 1]; // data block addresses
};

struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};

#endif
