#include "../defs.h"

int mkdir(char *args[], int arg_cnt) {
  int i;

  if (arg_cnt < 2) {
    printf("Usage: mkdir files...\n");
    return -1;
  }

  for (i = 1; i < arg_cnt; i++) {
    if (ffmkdir(args[i]) < 0) {
      printf("mkdir: %s failed to create\n", args[i]);
      break;
    }
  }

  return 0;
}
