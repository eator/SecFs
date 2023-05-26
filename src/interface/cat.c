#include "../defs.h"
#include "../fcntl.h"

int cat(char *args[], int arg_cnt) {
  int i;
  char buf[512];

  // read from file
  if (arg_cnt == 2) {
    // open file
    int n, fd;
    if ((fd = ffopen(args[1], 0)) < 0) {
      printf("cat: cannot open %s\n", args[1]);
      exit(1);
    }

    // print file contents
    while ((n = ffread(fd, buf, sizeof(buf))) > 0) {
      printf("%.*s", n, buf);
    }
    printf("\n");

    ffclose(fd);
    if (n < 0) {
      printf("cat: read error\n");
      return -1;
    }
  }
  // write to file
  else if (arg_cnt == 3 || !strcmp(">", args[2])) {
    // open file
    int n, fd;
    if ((fd = ffopen(args[2], 1)) < 0) {
      printf("cat: cannot open %s\n", args[2]);
      return -1;
    }

    memcpy(buf, "hello secfs", 13);
    ffwrite(fd, buf, 13);
    ffclose(fd);
    // get input contents
    /*
    fgets(buf, sizeof(buf), stdin);
    n = ffwrite(fd, buf, n);

    ffclose(fd);
    if(n < 0){
      printf("cat: write error\n");
      return -1;
    }
    */
  } else {
    printf("Usage(read): cat file\n");
    printf("Usage(write): cat > file\n");
    return -1;
  }

  return 0;
}
