#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../defs.h"
#include "../fs.h"

int fsfd;                            // fd for the file system
int nblks = FSSIZE;                  // total block num for the file system
int nlog = NLOG;                     // num of log blocks
int ninode = NINODEBLK / IPB + 1;    // num of inode blocks
int nbmp = FSSIZE / (BSIZE * 8) + 1; // num of bit-map blocks
int nmeta; // num of meta blocks (boot, sb, nlog, inode, bitmap)
int ndata; // num of data blocks

uint freeinode = 1;
uint freeblock;
struct superblock sb;
char zeroes[BSIZE];

void wblk(uint, void *);
uint iialloc(ushort type);
void iappend(uint inum, void *xp, int n);
void winode(uint inum, struct dinode *ip);
void rinode(uint inum, struct dinode *ip);
void bballoc(int used);

// convert to intel byte order
ushort xshort(ushort x) {
  ushort y;
  uchar *a = (uchar *)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint xint(uint x) {
  uint y;
  uchar *a = (uchar *)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: mkfs fs.img \n");
    exit(1);
  }

  uint rootino, inum, off;
  struct dirent de;
  struct dinode din;
  char buf[BSIZE];

  // check alignence
  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  // open a clean image file
  fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fsfd < 0) {
    printf("mkfs: can't open image file");
    exit(1);
  }

  // compute meta block and data block numbers
  nmeta = 1 + 1 + nlog + ninode + nbmp;
  ndata = nblks - nmeta;

  // set attributes of superblock
  sb.magic = FSMAGIC;
  sb.size = xint(FSSIZE);
  sb.ndata = xint(ndata);
  sb.ninodes = xint(ninode);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  sb.inodestart = xint(2 + nlog);
  sb.bmapstart = xint(2 + nlog + ninode);

  printf("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks "
         "%u) data blocks %d total %d\n",
         nmeta, nlog, ninode, nbmp, ndata, FSSIZE);

  freeblock = nmeta; // the first free block that we can allocate

  // initialize all blocks to 0
  for (int i = 0; i < FSSIZE; i++)
    wblk(i, zeroes);

  // write the super block
  memset(buf, 0, BSIZE);
  memcpy(buf, &sb, sizeof(sb));
  wblk(1, buf);

  // allocate inode for root dir
  rootino = iialloc(T_DIR);
  assert(rootino == ROOTINO);

  // add . and .. for root
  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  // allocate init dir
  /*
  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "bin");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "dev");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "etc");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "home");
  iappend(rootino, &de, sizeof(de));
  */

  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off / BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);

  bballoc(freeblock);

  return 0;
}

// write a block
void wblk(uint bno, void *buf) {
  if (lseek(fsfd, bno * BSIZE, 0) != bno * BSIZE) {
    printf("mkfs: lseek");
    exit(1);
  }

  if (write(fsfd, buf, BSIZE) != BSIZE) {
    printf("mkfs: write");
    exit(1);
  }
}

// read a block
void rblk(uint bno, void *buf) {
  if (lseek(fsfd, bno * BSIZE, 0) != bno * BSIZE) {
    printf("mkfs: lseek");
    exit(1);
  }

  if (read(fsfd, buf, BSIZE) != BSIZE) {
    printf("mkfs: read");
    exit(1);
  }
}

// write a inode
void winode(uint inum, struct dinode *ip) {
  uint bn;
  char buf[BSIZE];
  struct dinode *dip;

  // does this step redundant?
  bn = IBLOCK(inum, sb);
  rblk(bn, buf);

  dip = ((struct dinode *)buf) + (inum % IPB);
  *dip = *ip;
  wblk(bn, buf);
}

// read a inode
void rinode(uint inum, struct dinode *ip) {
  uint bn;
  char buf[BSIZE];
  struct dinode *dip;

  // does this step redundant?
  bn = IBLOCK(inum, sb);
  rblk(bn, buf);

  dip = ((struct dinode *)buf) + (inum % IPB);
  *ip = *dip;
}

// allocate a inode
uint iialloc(ushort type) {
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);

  return inum;
}

void bballoc(int used) {
  uchar buf[BSIZE];
  int i;

  printf("bballoc: first %d blocks have been allocated\n", used);
  assert(used < BSIZE * 8);
  bzero(buf, BSIZE);
  for (i = 0; i < used; i++) {
    buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
  }
  printf("bballoc: write bitmap block at block %d\n", sb.bmapstart);
  wblk(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void *xp, int n) {
  char *p = (char *)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x; // for data address

  rinode(inum, &din);
  off = xint(din.size); // convert back
  // printf("append inum %d at off %d sz %d\n", inum, off, n);
  while (n > 0) {
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);
    if (fbn < NDIRECT) {
      if (xint(din.addrs[fbn]) == 0) {
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if (xint(din.addrs[NDIRECT]) == 0) {
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      rblk(xint(din.addrs[NDIRECT]), (char *)indirect);

      if (indirect[fbn - NDIRECT] == 0) {
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wblk(xint(din.addrs[NDIRECT]), (char *)indirect);
      }
      x = xint(indirect[fbn - NDIRECT]);
    }

    n1 = min(n, (fbn + 1) * BSIZE - off);
    rblk(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wblk(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }

  din.size = xint(off);
  winode(inum, &din);
}
