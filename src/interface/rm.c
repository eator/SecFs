#include "../defs.h"

int rm(char *args[], int arg_cnt) {
  int i;

  if (arg_cnt < 2) {
    printf("Usage: rm files...\n");
    return -1;
  }

  for (i = 1; i < arg_cnt; i++) {
    if (ffunlink(args[i]) < 0) {
      printf("rm: %s failed to delete\n", args[i]);
      break;
    }
  }

  return 0;
}
