#include "fs.h"
#include "buf.h"
#include "defs.h"
#include "file.h"
#include "sleeplock.h"
#include "spinlock.h"
#include <stdlib.h>

// get min var
#define min(a, b) ((a) < (b) ? (a) : (b))

static uint bmap(struct inode *ip, uint bn);
// there should be one superblock per disk device,
// but here we run with only one device
struct superblock sb;
struct inode *cwd;

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

// Read the super block.
static void readsb(int dev, struct superblock *sb) {
  struct buf *bp;

  bp = bread(dev, 1);
  memcpy(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Init fs
void fsinit(int dev) {
  readsb(dev, &sb);
  if (sb.magic != FSMAGIC) {
    printf("panic: invalid file system");
    exit(1);
  }

  initlog(dev, &sb);
}

void init_cwd() {
  cwd = namei("/");
  printf("root block %d\n", cwd->inum);
}

// Blocks Operation
// Zero a block.
static void bbzero(int dev, int bno) {
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Allocate a zeroed disk block.
static uint balloc(uint dev) {
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for (b = 0; b < sb.size; b += BPB) {
    bp = bread(dev, BBLOCK(b, sb));
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
      m = 1 << (bi % 8);
      if ((bp->data[bi / 8] & m) == 0) { // Is block free?
        bp->data[bi / 8] |= m;           // Mark block in use.
        log_write(bp);
        brelse(bp);
        bbzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }

  printf("panic: balloc: out of blocks");
  exit(1);
}

// Free a disk block.
static void bfree(int dev, uint b) {
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if ((bp->data[bi / 8] & m) == 0) {
    printf("panic: freeing free block");
    exit(1);
  }

  bp->data[bi / 8] &= ~m;
  log_write(bp);
  brelse(bp);
}
/* Inode Operation */
static struct inode *iget(uint dev, uint inum);

// Allocate an inode on device dev.
// Mark it as allocated by giving it type type.
// Returns an unlocked but allocated and referenced inode.
struct inode *ialloc(uint dev, short type) {
  int inum;
  struct buf *bp;
  struct dinode *dip;

  // inum 0 is reserved by convention
  for (inum = 1; inum < sb.ninodes; inum++) {
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode *)bp->data + inum % IPB;
    // find a free inode
    if (dip->type == 0) {
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      // mark it allocated on the disk
      log_write(bp);
      brelse(bp);
      return iget(dev, inum);
    }

    brelse(bp);
  }

  printf("panic: ialloc: no free inodes");
  exit(1);
}

// Init itable and inodes
void iinit() {
  int i = 0;

  init_spinlock(&itable.lock, "itable");
  for (i = 0; i < NINODE; i++) {
    init_sleeplock(&itable.inode[i].lock, "inode");
  }
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field that lives on disk.
// Caller must hold ip->lock.
void iupdate(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode *)bp->data + ip->inum % IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;

  memcpy(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// Truncate inode (discard contents).
// Caller must hold ip->lock.
void itrunc(struct inode *ip) {
  int i, j;
  struct buf *bp;
  uint *a;

  // discard content in direct blocks
  for (i = 0; i < NDIRECT; i++) {
    if (ip->addrs[i]) {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  // discard content in indirect blocks
  if (ip->addrs[NDIRECT]) {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint *)bp->data;
    for (j = 0; j < NINDIRECT; j++) {
      if (a[j])
        bfree(ip->dev, a[j]);
      else
        break;
    }

    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// Read data from inode.
// Caller must hold ip->lock.
// Returns the number of bytes successfully read.
int readi(struct inode *ip, void *dst, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off)
    return 0;

  if (off + n > ip->size)
    n = ip->size - off;

  for (tot = 0; tot < n; tot += m, off += m, dst += m) {
    bp = bread(ip->dev, bmap(ip, off / BSIZE));
    m = min(n - tot, BSIZE - off % BSIZE);
    if (memcpy(dst, bp->data + (off % BSIZE), m) == NULL) {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;
}

// Write data to inode.
// Caller must hold ip->lock.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
int writei(struct inode *ip, void *src, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off)
    return -1;
  if (off + n > MAXFILE * BSIZE)
    return -1;

  for (tot = 0; tot < n; tot += m, off += m, src += m) {
    bp = bread(ip->dev, bmap(ip, off / BSIZE));
    m = min(n - tot, BSIZE - off % BSIZE);
    if (memcpy(bp->data + (off % BSIZE), src, m) == NULL) {
      brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  if (off > ip->size)
    ip->size = off;

  // write the i-node back to disk even if the size didn't change
  // because the loop above might have called bmap() and added a new block to
  // ip->addrs[].
  iupdate(ip);

  return tot;
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void stati(struct inode *ip, struct stat *st) {
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// Find the inode with number inum on device dev
// and return the in-memory copy.
// Does not lock the inode and does not read it from disk.
static struct inode *iget(uint dev, uint inum) {
  struct inode *ip, *empty;

  acquire_spinlock(&itable.lock);

  empty = 0;
  for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
    // the inode is already in the table
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
      ip->ref++;
      release_spinlock(&itable.lock);
      return ip;
    }

    // Remember empty slot.
    if (empty == 0 && ip->ref == 0)
      empty = ip;
  }

  // No free inode to use
  if (empty == 0) {
    printf("panic: iget: no inodes");
    exit(1);
  }

  // Recycle an empty inode entry.
  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release_spinlock(&itable.lock);

  return ip;
}

// Drop a ref to an in-memory inode.
// If it was the last ref, the inode table entry can be recycled.
// If it was the last ref and the inode has no links to it, free the inode (and
// its content) on disk. All calls to iput() must be inside a transaction in
// case it has to free the inode.
void iput(struct inode *ip) {
  acquire_spinlock(&itable.lock);

  if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
    // inode has no links and no other references: truncate and free.

    // ip->ref == 1 means no other process can have ip locked,
    // so this acquiresleep() won't block (or deadlock).
    acquire_sleeplock(&ip->lock);

    release_spinlock(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    release_sleeplock(&ip->lock);

    acquire_spinlock(&itable.lock);
  }

  ip->ref--;
  release_spinlock(&itable.lock);
}

void iunlockput(struct inode *ip) {
  iunlock(ip);
  iput(ip);
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *idup(struct inode *ip) {
  acquire_spinlock(&itable.lock);
  ip->ref++;
  release_spinlock(&itable.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void ilock(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  if (ip == 0 || ip->ref < 1) {
    printf("panic: ilock: invalid inode");
    exit(1);
  }

  acquire_sleeplock(&ip->lock);

  if (ip->valid == 0) {
    // read from disk
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode *)bp->data + ip->inum % IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memcpy(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;

    // read a inode of none type, it's impossible
    if (ip->type == 0) {
      printf("panic: ilock: no type");
      exit(1);
    }
  }
}

// Unlock the given inode.
void iunlock(struct inode *ip) {
  if (ip == 0 || !hold_sleeplock(&ip->lock) || ip->ref < 1) {
    printf("panic: iunlock: invalid inode | not locked ahead");
    exit(1);
  }

  release_sleeplock(&ip->lock);
}

/* Inode content */
// The content (data) associated with each inode is stored in blocks on the
// disk. The first NDIRECT block numbers are listed in ip->addrs[]. The next
// NINDIRECT blocks are listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint bmap(struct inode *ip, uint bn) {
  uint addr, *a;
  struct buf *bp;

  // block no in direct blocks(bn start from 0)
  if (bn < NDIRECT) {
    if ((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);

    return addr;
  }

  // block no in indirect blocks
  // minus NDIRECT (count for indirect block no)
  bn -= NDIRECT;

  if (bn < NINDIRECT) {
    // Load indirect block, allocating if necessary.
    if ((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn]) == 0) {
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }

    brelse(bp);
    return addr;
  }

  printf("panic: bmap: out of range");
  exit(1);
}

/* Directories */
int namecmp(const char *s, const char *t) { return strncmp(s, t, DIRSIZ); }

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode *dirlookup(struct inode *dp, char *name, uint *poff) {
  uint off, inum;
  struct dirent de;

  if (dp->type != T_DIR) {
    printf("panic: dirlookup: not DIR");
    exit(1);
  }

  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, &de, off, sizeof(de)) != sizeof(de)) {
      printf("panic: dirlookup: read");
      exit(1);
    }

    if (de.inum == 0)
      continue;

    if (namecmp(name, de.name) == 0) {
      // entry matches path element
      if (poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int dirlink(struct inode *dp, char *name, uint inum) {
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, &de, off, sizeof(de)) != sizeof(de)) {
      printf("panic: dirlink: read");
      exit(1);
    }

    if (de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if (writei(dp, &de, off, sizeof(de)) != sizeof(de)) {
    printf("panic: dirlink: write");
    exit(1);
  }

  return 0;
}

/* Paths */

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *skipelem(char *path, char *name) {
  char *s;
  int len;

  // skip slashes
  while (*path == '/')
    path++;

  // if null path, return 0
  if (*path == 0)
    return 0;

  // fetch the name till slash
  s = path;
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;

  // truncate path name length
  if (len >= DIRSIZ)
    memcpy(name, s, DIRSIZ);
  else {
    memcpy(name, s, len);
    name[len] = 0;
  }

  // skip slashes
  while (*path == '/')
    path++;

  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode *namex(char *path, int nameiparent, char *name) {
  struct inode *ip, *next;

  if (*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(cwd);

  while ((path = skipelem(path, name)) != 0) {
    ilock(ip);

    // check if dir or not
    if (ip->type != T_DIR) {
      iunlockput(ip);
      return 0;
    }

    // stop one level early.
    if (nameiparent && *path == '\0') {
      iunlock(ip);
      return ip;
    }

    // check if not find corresponding name
    if ((next = dirlookup(ip, name, 0)) == 0) {
      iunlockput(ip);
      return 0;
    }

    iunlockput(ip);
    ip = next;
  }

  if (nameiparent) {
    iput(ip);
    return 0;
  }

  return ip;
}

struct inode *namei(char *path) {
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode *nameiparent(char *path, char *name) {
  return namex(path, 1, name);
}
