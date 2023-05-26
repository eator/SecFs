#include "../defs.h"
#include "../fcntl.h"
#include "../file.h"
#include "../fs.h"

int ustat(const char *n, struct stat *st) {
  int fd;
  int r;

  fd = ffopen(n, O_RDONLY);
  if (fd < 0)
    return -1;
  r = ffstat(fd, st);
  ffclose(fd);
  return r;
}

char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ)
    return p;

  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

int ls(char *path) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = ffopen(path, 0)) < 0) {
    printf("ls: cannot open %s\n", path);
    return -1;
  }

  if (ffstat(fd, &st) < 0) {
    printf("ls: cannot stat %s\n", path);
    ffclose(fd);
    return -1;
  }

  switch (st.type) {
  case T_FILE:
    printf("%s %d %d %lld\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while (ffread(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (ustat(buf, &st) < 0) {
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d  %d  %lld\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }

  ffclose(fd);
  return 0;
}
