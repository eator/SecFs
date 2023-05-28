#ifndef FS_H
#define FS_H

#include "defs.h"
// On-disk file system format.

#define ROOTINO 1                        // root i-number
#define FSMAGIC 0x10203040               // SecFs Magic No
#define NDIRECT 11                       // direct block data no
#define NINDIRECT (BSIZE / sizeof(uint)) // once indirect block data no
#define NININDIRECT (NINDIRECT*NINDIRECT)// doubly indirect block data no
#define MAXFILE (NDIRECT + NINDIRECT + NININDIRECT) // all data block no

// Disk layout:
// [ boot block | super block | log | inode blocks | bit map | data blocks ]
//
// mkfs computes the super block and builds an initial file system.
// The super block describes the disk layout:
struct superblock {
  uint magic;      // must be FSMAGIC
  uint size;       // size of file system image (blocks)
  uint ndata;      // num of data blocks
  uint ninodes;    // num of inodes
  uint nlog;       // num of log blocks
  uint logstart;   // block num of first log block
  uint inodestart; // block num of first inode block
  uint bmapstart;  // block num of first free map block
};

// On-disk inode structure
struct dinode {
  short type;              // file type (dir or file or free)
  short nlink;             // numbers of links to the disk inode
  short major;             // major device no
  short minor;             // minor device no
  uint size;               // file size(bytes)
  uint addrs[NDIRECT + 1 + 1]; // data block addresses
};

// Inode num per block.
#define IPB (BSIZE / sizeof(struct dinode))
// Bitmap bit num per block
#define BPB (BSIZE * 8)

// Block no containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)
// Block no of free map containing bit for data block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

// Directory entry structure(for file and directory both)
struct dirent {
  ushort inum;       // referred inode no
  char name[DIRSIZ]; // name of file or dir
};

#endif
