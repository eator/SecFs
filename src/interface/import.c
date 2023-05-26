#include "../defs.h"
#include "../fcntl.h"

int fimport(char *args[], int arg_cnt) {
  if (arg_cnt < 2) {
    printf("Usage: import files...\n");
    return -1;
  }

  int n, fd;
  char buf[512];
  FILE *outer_file = fopen(args[1], "rb");
  if (outer_file == NULL) {
    printf("import: can't open import file\n");
    return -1;
  }

  char inst[] = "touch";
  char *args_touch[] = {inst, args[1], 0};
  if (touch(args_touch, 2) < 0) {
    return -1;
  }

  if ((fd = ffopen(args[1], 1)) < 0) {
    printf("import: cannot open %s\n", args[1]);
    return -1;
  }

  while ((n = fread(buf, sizeof(char), sizeof(buf), outer_file)) > 0) {
    if (ffwrite(fd, buf, n) < 0) {
      ffclose(fd);
      printf("import: write error\n");
      return -1;
    }
  }

  ffclose(fd);
  return 0;
}
