#include "buf.h"
#include "sleeplock.h"
#include "spinlock.h"

// bufs cache
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

// init bufs cache
void binit(void) {
  struct buf *b;

  init_spinlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;

  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    init_sleeplock(&b->lock, "buffer");

    // get b's link
    b->next = bcache.head.next;
    b->prev = &bcache.head;

    // update original head.next and head
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

static struct buf *bget(uint dev, uint blkno) {
  struct buf *b;

  acquire_spinlock(&bcache.lock);

  // Is the block already cached?
  // search the most recently used buffer
  for (b = bcache.head.next; b != &bcache.head; b = b->next) {
    if (b->dev == dev && b->blkno == blkno) {
      b->refcnt++;
      release_spinlock(&bcache.lock);
      acquire_sleeplock(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blkno = blkno;
      b->valid = 0;
      b->refcnt = 1;
      release_spinlock(&bcache.lock);
      acquire_sleeplock(&b->lock);
      return b;
    }
  }

  // No free buf to use
  printf("panic: bget: no buffers");
  exit(1);
}

struct buf *bread(uint dev, uint blkno) {
  struct buf *b;

  b = bget(dev, blkno);
  if (!b->valid) {
    virtio_disk_rw(b, 0); // 0:read, 1: write
    b->valid = 1;
  }

  return b;
}

void bwrite(struct buf *b) {
  if (!hold_sleeplock(&b->lock)) {
    printf("panic: bwrite\n");
    exit(1);
  }

  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!hold_sleeplock(&b->lock)) {
    printf("panic: brelse\n");
    exit(1);
  }

  release_sleeplock(&b->lock);

  acquire_spinlock(&bcache.lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // update b's location
    b->next->prev = b->prev;
    b->prev->next = b->next;

    b->next = bcache.head.next;
    b->prev = &bcache.head;

    bcache.head.next->prev = b;
    bcache.head.next = b;
  }

  release_spinlock(&bcache.lock);
}

void bpin(struct buf *b) {
  acquire_spinlock(&bcache.lock);
  b->refcnt++;
  release_spinlock(&bcache.lock);
}

void bunpin(struct buf *b) {
  acquire_spinlock(&bcache.lock);
  b->refcnt--;
  release_spinlock(&bcache.lock);
}
