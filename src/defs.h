#ifndef DEFS_H
#define DEFS_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long long uint64;

#define ROOTDEV 1            // device no of file system root disk
#define BSIZE 1024           // block size
#define MAXOPBLKS 10         // max number of blocks by once write operation
#define NBUF (MAXOPBLKS * 3) // buf num in buffer cache
#define NLOG (MAXOPBLKS * 3) // log num in on-disk log

#define NOFILE 16     // open files per process
#define NFILE 100     // open files per system
#define NINODE 50     // maximum number of active i-nodes
#define NINODEBLK 200 // num of inode blocks on disk
#define FSSIZE 200000 // size of the file system in blocks(For big File)
#define MAXPATH 128   // maximum file path name

#define T_DIR 1    // Directory
#define T_FILE 2   // File
#define T_DEVICE 3 // Device
#define NDEV 10    // maximum major device number

struct stat;
struct inode;
struct superblock;

// bio.c
void binit(void);
struct buf *bread(uint, uint);
void brelse(struct buf *);
void bwrite(struct buf *);
void bpin(struct buf *);
void bunpin(struct buf *);

// fs.c
int readi(struct inode *ip, void *dst, uint off, uint n);
int writei(struct inode *ip, void *src, uint off, uint n);
void iinit();
void init_cwd();
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iput(struct inode *ip);
struct inode *idup(struct inode *);
struct inode *namei(char *path);
void stati(struct inode *, struct stat *);
int namecmp(const char *s, const char *t);
struct inode *nameiparent(char *path, char *name);
struct inode *dirlookup(struct inode *dp, char *name, uint *poff);
void iunlockput(struct inode *ip);
void iupdate(struct inode *ip);
void itrunc(struct inode *ip);
struct inode *ialloc(uint dev, short type);
int dirlink(struct inode *dp, char *name, uint inum);
void fsinit(int dev);

// log.c
void initlog(int, struct superblock *);
void log_write(struct buf *);
void begin_op(void);
void end_op(void);

void fileinit(void);
struct file *filedup(struct file *f);
int fileread(struct file *f, void *addr, int n);
int filewrite(struct file *f, void *addr, int n);
void fileclose(struct file *f);
int filestat(struct file *f, void *addr);
struct file *filealloc(void);

// disk
void virtio_disk_init(void);
void virtio_disk_rw(struct buf *b, int write);

// filecall.c
int ffdup(int fd);
int ffread(int fd, void *p, int n);
int ffwrite(int fd, void *p, int n);
int ffclose(int fd);
int ffstat(int fd, struct stat *st);
int fflink(const char *pold, const char *pnew);
int ffunlink(const char *ppath);
int ffopen(const char *ppath, int omode);
int ffmkdir(const char *ppath);
int ffmknod(const char *ppath, int major, int minor);
int ffchdir(const char *ppath);
int ffseek(int fd, int offset, int base);

// interface
int ls(char *path);
int mkdir(char *args[], int arg_cnt);
int rm(char *args[], int arg_cnt);
int touch(char *args[], int arg_cnt);
int cat(char *args[], int arg_cnt);
int fimport(char *args[], int arg_cnt);
int testseek(char *args[], int arg_cnt);

#endif
