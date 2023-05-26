#include "../defs.h"
#include "../fcntl.h"

int touch(char *args[], int arg_cnt) {
  int i;

  if (arg_cnt < 2) {
    printf("Usage: touch files...\n");
    return -1;
  }

  for (i = 1; i < arg_cnt; i++) {
    int fd;
    if ((fd = ffopen(args[i], O_CREATE)) < 0) {
      printf("touch: failed to %s\n", args[i]);
      break;
    }

    ffclose(fd);
  }

  return 0;
}
