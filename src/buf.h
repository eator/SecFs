#ifndef BUF_H
#define BUF_H

#include "defs.h"
#include "sleeplock.h"

struct buf {
  int valid;
  int disk;

  uint dev;
  uint blkno;

  struct sleeplock lock; // protect the fields below it

  uint refcnt;      // Is any inode refer to the buf?
  struct buf *prev; // used for LRU reuse
  struct buf *next; // used to find if a buf is existing
  char data[BSIZE]; // the data of the buf
};

#endif
