#include "../defs.h"
#include "../fcntl.h"

int testseek(char *args[], int arg_cnt) {
  int fd;
  char abc[500] = {0};
  if ((fd = ffopen("Jerry", O_RDWR)) < 0) {
    printf("testseek: failed to open %s\n", "Jerry");
    return -1;
  }

  ffseek(fd, 500, 0);
  int rd = ffread(fd, abc, 500);
  printf("read bytes num: %d\n", rd);
  for (int i = 0; i < 300; i++) {
    abc[i] = '1';
  }

  ffseek(fd, 500, 0);
  int wr = ffwrite(fd, abc, rd);
  printf("write bytes num: %d\n", wr);

  ffclose(fd);
  return 0;
}
