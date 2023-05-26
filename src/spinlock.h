#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "defs.h"

struct spinlock {
  uint locked; // Is the lock held?
  pthread_spinlock_t lock;

  // debug fields:
  char *name; // name of lock.
};

void init_spinlock(struct spinlock *lk, char *name);
void destroy_spinlock(struct spinlock *lk);
void acquire_spinlock(struct spinlock *lk);
void release_spinlock(struct spinlock *lk);
int hold_spinlock(struct spinlock *lk);

#endif
