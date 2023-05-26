#include "spinlock.h"

void init_spinlock(struct spinlock *lk, char *name) {
  lk->name = name;
  lk->locked = 0;
  pthread_spin_init(&lk->lock, PTHREAD_PROCESS_PRIVATE);
}

// to be complete ?
void destroy_spinlock(struct spinlock *lk) { pthread_spin_destroy(&lk->lock); }

int hold_spinlock(struct spinlock *lk) { return lk->locked; }

void acquire_spinlock(struct spinlock *lk) {
  if (hold_spinlock(lk)) {
    printf("panic: acquire_spinlock");
    exit(1);
  }

  lk->locked = 1;
  pthread_spin_lock(&lk->lock);
}

void release_spinlock(struct spinlock *lk) {
  if (!hold_spinlock(lk)) {
    printf("panic: release_spinlock");
    exit(1);
  }

  lk->locked = 0;
  pthread_spin_unlock(&lk->lock);
}
