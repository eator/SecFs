#include "sleeplock.h"

void init_sleeplock(struct sleeplock *lk, char *name) {
  pthread_mutex_unlock(&lk->mutex);
  pthread_cond_signal(&lk->cond);

  lk->name = name;
  lk->locked = 0;
}

// to be complete ?
void destroy_sleeplock(struct sleeplock *lk) {
  pthread_mutex_destroy(&lk->mutex);
  pthread_cond_destroy(&lk->cond);
}

int hold_sleeplock(struct sleeplock *lk) { return lk->locked; }

void acquire_sleeplock(struct sleeplock *lk) {
  pthread_mutex_lock(&lk->mutex);

  while (lk->locked) {
    pthread_cond_wait(&lk->cond, &lk->mutex);
  }
  lk->locked = 1;

  pthread_mutex_unlock(&lk->mutex);
}

void release_sleeplock(struct sleeplock *lk) {
  pthread_mutex_lock(&lk->mutex);

  lk->locked = 0;
  pthread_cond_signal(&lk->cond);

  pthread_mutex_unlock(&lk->mutex);
}
