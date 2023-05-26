#include "buf.h"
#include "spinlock.h"
#include <stdio.h>

extern FILE *img_file;

struct spinlock vdisk_lock;

void virtio_disk_init(void) { init_spinlock(&vdisk_lock, "virtio_disk"); }

void virtio_disk_rw(struct buf *b, int write) {
  acquire_spinlock(&vdisk_lock);

  // TODO
  // may be use long for bigger fs?
  uint offset = b->blkno * 1024;
  int seek = fseek(img_file, offset, SEEK_SET);
  if (seek) {
    printf("panic: fseek failed");
    exit(1);
  }

  if (write) {
    size_t wret = fwrite(b->data, sizeof(char), BSIZE, img_file);
    if (wret != BSIZE) {
      printf("panic: write disk error");
      exit(1);
    }
  } else {
    size_t rret = fread(b->data, sizeof(char), BSIZE, img_file);
    if (rret != BSIZE) {
      printf("panic: read disk error");
      exit(1);
    }
  }

  release_spinlock(&vdisk_lock);
}
