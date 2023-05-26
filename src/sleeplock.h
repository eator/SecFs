#ifndef SLEEPLOCK_H
#define SLEEPLOCK_H

#include "defs.h"

struct sleeplock {
  uint locked; // Is the lock held?
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  // debug fields:
  char *name; // name of lock.
};

void init_sleeplock(struct sleeplock *lk, char *name);
void destroy_sleeplock(struct sleeplock *lk);
void acquire_sleeplock(struct sleeplock *lk);
void release_sleeplock(struct sleeplock *lk);
int hold_sleeplock(struct sleeplock *lk);

#endif
