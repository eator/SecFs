#include "defs.h"
#include "file.h"
#include "stdio.h"

#define MAX_ARG 5
FILE *img_file;
struct file *ofile[NOFILE]; // Open files
extern struct inode *cwd;
const char *initdirs[] = {
    "bin",
    "dev",
    "etc",
    "home",
};

int resolve_inst(char *args[], int arg_cnt);
void check_initdir();

int main() {
  // open image file
  img_file = fopen("fs.img", "r+b");
  if (img_file == NULL) {
    printf("main: can't open img_file\n");
    exit(1);
  }

  // init buffer cache
  binit();
  // init virtual disk
  virtio_disk_init();
  // init fs
  fsinit(ROOTDEV);
  // init inode table
  iinit();
  // init file table
  fileinit();
  // init cwd
  init_cwd();
  // check init dir
  check_initdir();

  printf("SecFs start successfully!\n");

  while (1) {
    char inst[40];
    char *args[MAX_ARG];
    int arg_cnt = 0;

    printf("$ ");
    fgets(inst, sizeof(inst), stdin);
    char *token = strtok(inst, " \n");
    while (token != NULL) {
      args[arg_cnt] = token;
      arg_cnt++;
      if (arg_cnt >= MAX_ARG) {
        break;
      }
      token = strtok(NULL, " \n");
    }
    args[arg_cnt] = NULL;

    if (resolve_inst(args, arg_cnt))
      printf("resolve %s failed\n", args[0]);
  }

  // close image file
  fclose(img_file);
  return 0;
}

// return value: 0: success, -1: fail
int resolve_inst(char *args[], int arg_cnt) {
  if (!strcmp("ls", args[0])) {
    if (arg_cnt < 2) {
      return ls(".");
    }

    int err = 0;
    for (int i = 1; i < arg_cnt; i++) {
      int tmp = ls(args[i]);
      if (tmp)
        err = tmp;
    }
    return err;
  } else if (!strcmp("cd", args[0])) {
    if (arg_cnt < 2) {
      return ffchdir("/");
    }

    if (ffchdir(args[1]) < 0) {
      printf("cannot cd %s\n", args[1]);
      return -1;
    }

    return 0;
  } else if (!strcmp("pwd", args[0])) {
    // TODO
    return -1;
  } else if (!strcmp("mkdir", args[0])) {
    return mkdir(args, arg_cnt);
  } else if (!strcmp("del", args[0])) {
    return rm(args, arg_cnt);
  } else if (!strcmp("touch", args[0])) {
    return touch(args, arg_cnt);
  } else if (!strcmp("cat", args[0])) {
    return cat(args, arg_cnt);
  } else if (!strcmp("import", args[0])) {
    return fimport(args, arg_cnt);
  } else if (!strcmp("testseek", args[0])) {
    return testseek(args, arg_cnt);
  } else if (!strcmp("exit", args[0])) {
    fclose(img_file);
    exit(0);
  } else if (!strcmp("", args[0])) {
    return 0;
  } else {
    return -1;
  }

  return 0;
}

void check_initdir() {
  int fd;
  struct dirent de;
  struct stat st;
  int have_init[4] = {0};

  if ((fd = ffopen(".", 0)) < 0) {
    printf("check_initdir: cannot open root dir\n");
    exit(1);
  }

  while (ffread(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    for (int i = 0; i < 4; i++) {
      if (!strcmp(de.name, initdirs[i]))
        have_init[i] = 1;
    }
  }

  for (int i = 0; i < 4; i++) {
    if (!have_init[i]) {
      if (ffmkdir(initdirs[i]) < 0) {
        printf("check_initdir: %s failed to create\n", initdirs[i]);
        exit(1);
      }
    }
  }
}
