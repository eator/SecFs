#include "defs.h"
#include "fcntl.h"
#include "file.h"

extern struct file *ofile[NOFILE]; // Open files
extern struct inode *cwd;

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int fdalloc(struct file *f) {
  int fd;

  for (fd = 0; fd < NOFILE; fd++) {
    if (ofile[fd] == 0) {
      ofile[fd] = f;
      return fd;
    }
  }

  return -1;
}

// copy same struct file, but with different fd
int ffdup(int fd) {
  struct file *f;

  if (fd < 0 || fd >= NOFILE || (f = ofile[fd]) == 0)
    return -1;

  if ((fd = fdalloc(f)) < 0)
    return -1;

  filedup(f);
  return fd;
}

int ffread(int fd, void *p, int n) {
  struct file *f;

  if (fd < 0 || fd >= NOFILE || (f = ofile[fd]) == 0 || n < 0)
    return -1;

  return fileread(f, p, n);
}

int ffwrite(int fd, void *p, int n) {
  struct file *f;

  if (fd < 0 || fd >= NOFILE || (f = ofile[fd]) == 0 || n < 0)
    return -1;

  return filewrite(f, p, n);
}

int ffclose(int fd) {
  struct file *f;

  if (fd < 0 || fd >= NOFILE || (f = ofile[fd]) == 0)
    return -1;

  ofile[fd] = 0;
  fileclose(f);

  return 0;
}

int ffstat(int fd, struct stat *st) {
  struct file *f;

  if (fd < 0 || fd >= NOFILE || (f = ofile[fd]) == 0)
    return -1;

  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int fflink(const char *pold, const char *pnew) {
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  strncpy(old, pold, MAXPATH);
  strncpy(new, pnew, MAXPATH);

  begin_op();
  if ((ip = namei(old)) == 0) {
    end_op();
    return -1;
  }

  ilock(ip);
  if (ip->type == T_DIR) {
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if ((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if (dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0) {
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int isdirempty(struct inode *dp) {
  int off;
  struct dirent de;

  for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
    if (readi(dp, &de, off, sizeof(de)) != sizeof(de)) {
      printf("panic: isdirempty: readi");
    }

    if (de.inum != 0)
      return 0;
  }

  return 1;
}

int ffunlink(const char *ppath) {
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  strncpy(path, ppath, MAXPATH);

  begin_op();
  if ((dp = nameiparent(path, name)) == 0) {
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if ((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if (ip->nlink < 1) {
    printf("panic: unlink: nlink < 1");
    exit(1);
  }

  if (ip->type == T_DIR && !isdirempty(ip)) {
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if (writei(dp, &de, off, sizeof(de)) != sizeof(de)) {
    printf("panic: unlink: writei");
    exit(1);
  }

  if (ip->type == T_DIR) {
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode *create(char *path, short type, short major, short minor) {
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if ((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iunlockput(dp);
    ilock(ip);
    if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if ((ip = ialloc(dp->dev, type)) == 0) {
    printf("panic: create: ialloc");
    exit(1);
  }

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if (type == T_DIR) { // Create . and .. entries.
    dp->nlink++;       // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0) {
      printf("panic: create dots");
      exit(1);
    }
  }

  if (dirlink(dp, name, ip->inum) < 0) {
    printf("panic: create: dirlink");
    exit(1);
  }

  iunlockput(dp);

  return ip;
}

int ffopen(const char *ppath, int omode) {
  char path[MAXPATH];
  int fd;
  struct file *f;
  struct inode *ip;
  int n;

  strncpy(path, ppath, MAXPATH);

  begin_op();

  if (omode & O_CREATE) {
    ip = create(path, T_FILE, 0, 0);
    if (ip == 0) {
      end_op();
      return -1;
    }
  } else {
    if ((ip = namei(path)) == 0) {
      end_op();
      return -1;
    }
    ilock(ip);
    if (ip->type == T_DIR && omode != O_RDONLY) {
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if (ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)) {
    iunlockput(ip);
    end_op();
    return -1;
  }

  if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
    if (f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if (ip->type == T_DEVICE) {
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if ((omode & O_TRUNC) && ip->type == T_FILE) {
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

int ffmkdir(const char *ppath) {
  char path[MAXPATH];
  struct inode *ip;

  strncpy(path, ppath, MAXPATH);

  begin_op();
  if ((ip = create(path, T_DIR, 0, 0)) == 0) {
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int ffmknod(const char *ppath, int major, int minor) {
  struct inode *ip;
  char path[MAXPATH];

  strncpy(path, ppath, MAXPATH);

  begin_op();
  if (major < 0 || minor < 0 ||
      (ip = create(path, T_DEVICE, major, minor)) == 0) {
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int ffseek(int fd, int offset, int base) {
  struct file *f;
  if (fd < 0 || fd >= NOFILE || (f = ofile[fd]) == 0)
    return -1;

  if (base < 0 || offset + base < 0 || offset + base > f->ip->size)
    return -1;

  f->off = offset + base;

  return offset + base;
}

int ffchdir(const char *ppath) {
  char path[MAXPATH];
  struct inode *ip;

  strncpy(path, ppath, MAXPATH);

  begin_op();
  if ((ip = namei(path)) == 0) {
    end_op();
    return -1;
  }
  ilock(ip);
  if (ip->type != T_DIR) {
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(cwd);
  end_op();
  cwd = ip;
  return 0;
}
