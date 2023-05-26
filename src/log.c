#include "buf.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[NLOG];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};

static void recover_from_log(void);
static void commit();

struct log dlog;

void initlog(int dev, struct superblock *sb) {
  if (sizeof(struct logheader) >= BSIZE) {
    printf("panic: initlog: too big logheader");
    exit(1);
  }

  init_spinlock(&dlog.lock, "dlog");
  dlog.start = sb->logstart;
  dlog.size = sb->nlog;
  dlog.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void install_trans(int recovering) {
  int tail;

  for (tail = 0; tail < dlog.lh.n; tail++) {
    struct buf *lbuf = bread(dlog.dev, dlog.start + tail + 1); // read log block
    struct buf *dbuf = bread(dlog.dev, dlog.lh.block[tail]);   // read dst
    memmove(dbuf->data, lbuf->data, BSIZE); // copy block to dst
    bwrite(dbuf);                           // write dst to disk
    if (recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void read_head(void) {
  struct buf *buf = bread(dlog.dev, dlog.start);
  struct logheader *lh = (struct logheader *)(buf->data);
  int i;
  dlog.lh.n = lh->n;
  for (i = 0; i < dlog.lh.n; i++) {
    dlog.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void write_head(void) {
  struct buf *buf = bread(dlog.dev, dlog.start);
  struct logheader *hb = (struct logheader *)(buf->data);
  int i;
  hb->n = dlog.lh.n;
  for (i = 0; i < dlog.lh.n; i++) {
    hb->block[i] = dlog.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void recover_from_log(void) {
  read_head();
  install_trans(1); // if committed, copy from log to disk
  dlog.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void begin_op(void) {
  acquire_spinlock(&dlog.lock);
  while (1) {
    // TODO
    if (dlog.committing) {
      // sleep(&dlog, &dlog.lock);
    } else if (dlog.lh.n + (dlog.outstanding + 1) * MAXOPBLKS > NLOG) {
      // this op might exhaust log space; wait for commit.
      // sleep(&dlog, &dlog.lock);
    } else {
      dlog.outstanding += 1;
      release_spinlock(&dlog.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void end_op(void) {
  int do_commit = 0;

  acquire_spinlock(&dlog.lock);
  dlog.outstanding -= 1;
  if (dlog.committing) {
    printf("panic: log: committing");
    exit(1);
  }

  if (dlog.outstanding == 0) {
    do_commit = 1;
    dlog.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    // TODO
    // wakeup(&dlog);
  }
  release_spinlock(&dlog.lock);

  if (do_commit) {
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire_spinlock(&dlog.lock);
    dlog.committing = 0;
    // TODO
    // wakeup(&dlog);
    release_spinlock(&dlog.lock);
  }
}

// Copy modified blocks from cache to log.
static void write_log(void) {
  int tail;

  for (tail = 0; tail < dlog.lh.n; tail++) {
    struct buf *to = bread(dlog.dev, dlog.start + tail + 1); // log block
    struct buf *from = bread(dlog.dev, dlog.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to); // write the log
    brelse(from);
    brelse(to);
  }
}

static void commit() {
  if (dlog.lh.n > 0) {
    write_log();      // Write modified blocks from cache to log
    write_head();     // Write header to disk -- the real commit
    install_trans(0); // Now install writes to home locations
    dlog.lh.n = 0;
    write_head(); // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void log_write(struct buf *b) {
  int i;
  acquire_spinlock(&dlog.lock);

  if (dlog.lh.n >= NLOG || dlog.lh.n >= dlog.size - 1) {
    printf("panic: too big a transaction");
    exit(1);
  }

  if (dlog.outstanding < 1) {
    printf("panic: log_write outside of trans");
    exit(1);
  }

  // log absorption
  for (i = 0; i < dlog.lh.n; i++) {
    if (dlog.lh.block[i] == b->blkno)
      break;
  }
  dlog.lh.block[i] = b->blkno;

  // Add new block to log?
  if (i == dlog.lh.n) {
    bpin(b);
    dlog.lh.n++;
  }

  release_spinlock(&dlog.lock);
}
