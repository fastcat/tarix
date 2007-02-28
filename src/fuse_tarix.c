/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2006  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.

    gcc -Wall `pkg-config fuse --cflags --libs` hello.c -o hello
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>

struct tarixfs_t {
  char *tarfilename;
  char *tarixfilename;
};
static struct tarixfs_t tarixfs;

static int tarix_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
/*
    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if(strcmp(path, tarix_path) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(tarixfs.tarix_str);
    }
    else
        res = -ENOENT;
*/
    return res;
}

static int tarix_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

/*
    if(strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, tarix_path + 1, NULL, 0);
*/

    return 0;
}

static int tarix_open(const char *path, struct fuse_file_info *fi)
{
/*
    if(strcmp(path, tarix_path) != 0)
        return -ENOENT;

    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;
*/
    return 0;
}

static int tarix_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
/*
    size_t len;
    (void) fi;
    if(strcmp(path, tarix_path) != 0)
        return -ENOENT;

    len = strlen(tarixfs.tarix_str);
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, tarixfs.tarix_str + offset, size);
    } else
        size = 0;
*/
    return size;
}

static struct fuse_operations tarix_oper = {
/*
    .getattr	= tarix_getattr,
    .readdir	= tarix_readdir,
    .open	= tarix_open,
    .read	= tarix_read,
*/
};

#define TARIX_OPT(t, p, v) { t, offsetof(struct tarixfs_t, p), v }
static struct fuse_opt tarix_opts[] = {
/*
  TARIX_OPT("tarix_str=%s", tarix_str, 0),
*/
  TARIX_OPT("tar=%s", tarfilename, 0),
  TARIX_OPT("tarix=%s", tarixfilename, 0),
  FUSE_OPT_END
};

int tarix_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
  switch (key) {
    case FUSE_OPT_KEY_OPT:
      return 1;
    case FUSE_OPT_KEY_NONOPT:
      return 1;
    default:
      fprintf(stderr, "wtf?\n");
      return 1;
  }
}

int main(int argc, char *argv[])
{
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  if (fuse_opt_parse(&args, &tarixfs, tarix_opts, tarix_opt_proc) == -1)
    return 1;
  return fuse_main(args.argc, args.argv, &tarix_oper, NULL);
}
