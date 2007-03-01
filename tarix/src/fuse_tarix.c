/*
 *  tarix - a GNU/POSIX tar indexer
 *  Copyright (C) 2006 Matthew "Cheetah" Gabeler-Lee
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define FUSE_USE_VERSION 26

#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <glib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "lineloop.h"
#include "index_parser.h"
#include "tstream.h"
#include "tar.h"


struct index_node {
  struct index_entry entry;
  /* stat struct for the file,
   * st_nlink is used to identify whether or not it has been initialized
   * 0 means not inited, other means inited
   * For directories, a value of 1 means the stat structure is inited,
   * but not the child nodes
   */
  struct stat stbuf;
  struct index_node *next;
  struct index_node *child;
};

struct tarixfs_t {
  int flags_norun;
  int flags;
  char *tarfilename;
  char *indexfilename;
  int use_zlib;
  /* tar(.gz) stream */
  t_streamp tsp;
  /* hash of all filenames to corresponding index_nodes */
  GHashTable *fnhash;
};

static struct tarixfs_t tarixfs;

static struct index_node *find_node(const char *path) {
  struct index_node *node = g_hash_table_lookup(tarixfs.fnhash, path);
//  if (node == NULL && path[0] == '/'l)
//    /* try to remove a leading slash */
//    node = find_node(path + 1);
  return node;
}

static int is_node_stat_filled(struct index_node *node) {
  if (node == NULL)
    return 0;
  if (node->stbuf.st_nlink == 0)
    return 0;
  if (node->stbuf.st_ino == 0)
    return 0;
  return 1;
}

static int is_node_children_filled(struct index_node *node) {
  if (node == NULL)
    return 0;
  if (node->stbuf.st_nlink == 0)
    return 0;
  if (node->stbuf.st_mode == S_IFDIR)
    return node->stbuf.st_nlink > 1;
  return 1;
}

static int fill_node_stat(struct index_node *node) {
  int res;
  union tar_block tarhdr;
  /* seek to header */
  res = ts_seek(tarixfs.tsp, tarixfs.use_zlib
    ? node->entry.zoffset : (off64_t)node->entry.ioffset * TARBLKSZ);
  if (res != 0)
    /*TODO: log underlying error */
    return -EIO;
  /* read header */
  res = ts_read(tarixfs.tsp, &tarhdr, TARBLKSZ);
  if (res < TARBLKSZ)
    return -EIO;
  /* handle long name */
  if (tarhdr.header.typeflag == GNUTYPE_LONGNAME) {
    /* skip the name */
    res = ts_read(tarixfs.tsp, &tarhdr, TARBLKSZ);
    if (res < TARBLKSZ)
      return -EIO;
    /* read the real file header */
    res = ts_read(tarixfs.tsp, &tarhdr, TARBLKSZ);
    if (res < TARBLKSZ)
      return -EIO;
  }
  /* process header */
  memset(&node->stbuf, 0, sizeof(node->stbuf));
  node->stbuf.st_ino = node->entry.num + 1;
  /* tar doesn't fill in higher bits */
  node->stbuf.st_mode = strtol(tarhdr.header.mode, NULL, 8) & 07777;
  switch (tarhdr.header.typeflag) {
    case REGTYPE:
    case AREGTYPE:
      node->stbuf.st_mode |= S_IFREG;
      break;
    case SYMTYPE:
      node->stbuf.st_mode |= S_IFLNK;
      break;
    case CHRTYPE:
      node->stbuf.st_mode |= S_IFCHR;
      break;
    case BLKTYPE:
      node->stbuf.st_mode |= S_IFBLK;
      break;
    case DIRTYPE:
      node->stbuf.st_mode |= S_IFDIR;
      break;
    case FIFOTYPE:
      node->stbuf.st_mode |= S_IFIFO;
      break;
    default:
      /*TODO: logging */
      return -EIO;
  }
  /* dirs will get further analysis later on */
  node->stbuf.st_nlink = 1;
  /*TODO: user/group handling */
  node->stbuf.st_uid = strtoul(tarhdr.header.uid, NULL, 8);
  node->stbuf.st_gid = strtoul(tarhdr.header.gid, NULL, 8);
  node->stbuf.st_size = strtoul(tarhdr.header.size, NULL, 8);
  node->stbuf.st_mtime = node->stbuf.st_atime = node->stbuf.st_ctime
    = strtoul(tarhdr.header.mtime, NULL, 8);
  
  return 0;
}

struct cmissing_state {
  GHashTable *newentries;
  struct index_parser_state *ipstate;
};

static void create_dentry_if_missing(const char *path, const char *stop,
    struct cmissing_state *cmstate) {
  if (stop == NULL)
    stop = path + strlen(path) - 1;

  char *dpath;  
  /* dpath MUST NOT end with a / */
  int fnlen = stop - path;
  /* handle the "/" case */
  if (fnlen == 0)
    fnlen = 1;
  /* make sure created path starts with / */
  if (path[0] != '/') {
fprintf(stderr, "WARN: cm path '%s' doesn't start with a /\n", path);
    ++fnlen;
  }
  dpath = malloc(fnlen + 1 /* for null terminator */);
  /* always should start with /, avoids a couple ifs */
  *dpath = '/';
  char *dest = dpath;
  if (path[0] != '/')
    ++dest;
  strncpy(dest, path, stop - path);
  /* put in that null terminator */
  dpath[fnlen] = 0;

  struct index_node *node = find_node(dpath);
  if (node != NULL) {
    /* don't need this any more */
    free(dpath);
    return;
  }
  node = g_hash_table_lookup(cmstate->newentries, dpath);
  if (node != NULL) {
    free(dpath);
    return;
  }
  
fprintf(stderr, "INFO: creating implicit directory '%s'\n", dpath);

  node = calloc(1, sizeof(*node));
  /* flag the node as being virtual */
  node->entry.version = -1;
  node->entry.num = ++cmstate->ipstate->last_num;
  /* put bogus offset values in */
  node->entry.ioffset = ~0UL;
  node->entry.zoffset = ~0ULL;
  node->entry.ilen = ~0UL;
  node->entry.filename = dpath;
  node->entry.filename_allocated = 1;
  
  node->stbuf.st_ino = node->entry.num + 1;
  /*TODO: user-specified mode/perms on implicit dirs */
  node->stbuf.st_mode = S_IFDIR | 0755;
  node->stbuf.st_nlink = 1;
  node->stbuf.st_uid = getuid();
  node->stbuf.st_gid = getgid();
  node->stbuf.st_mtime = node->stbuf.st_atime = node->stbuf.st_ctime = time(NULL);
  
  g_hash_table_insert(cmstate->newentries, node->entry.filename, node);
}

static void create_implicit_entries(void *vpath, void *vnode, void *vcmstate) {
  char *path = (char*)vpath;
  char *spos;
  struct cmissing_state *cmstate = (struct cmissing_state*)vcmstate;
  /* make sure there are directory entries the whole way up the tree */
  for (spos = strchr(path, '/'); spos != NULL; spos = strchr(spos + 1, '/'))
    create_dentry_if_missing(path, spos, cmstate);
}

static void copy_entries(void *key, void *value, void *vdest) {
  GHashTable *dest = (GHashTable*)vdest;
  g_hash_table_insert(dest, key, value);
}

static void link_entries(void *vpath, void *vnode, void *vdata) {
  char *path = (char*)vpath;
  struct index_node *node = (struct index_node*)vnode;
  
  char *lspos = strrchr(path, '/');
  if (lspos == path) {
    if (path[1] == 0)
      /* path == "/" */
      return;
    else
      /* parent is "/", hack it */
      ++lspos;
  }
  char *ppath = malloc(lspos - path + 1);
  strncpy(ppath, path, lspos - path);
  ppath[lspos - path] = 0;
  
  struct index_node *pnode = find_node(ppath);
  if (pnode == NULL) {
    /*TODO: error report, should never get here */
    fprintf(stderr, "ERROR: cannot find parent '%s' for '%s'\n", ppath, path);
    free(ppath);
    return;
  }
  
  /* prepend it to the linked list */
  node->next = pnode->child;
  pnode->child = node;
  /* update the link count */
  if (pnode->stbuf.st_mode & S_IFDIR) {
    if (pnode->stbuf.st_nlink < 2)
      pnode->stbuf.st_nlink = 3;
    else
      ++pnode->stbuf.st_nlink;
//fprintf(stderr, "INFO: linked '%s' to parent '%s', nlink=%ld\n", path, ppath, pnode->stbuf.st_nlink);
  } else {
fprintf(stderr, "WARN: linked '%s' to non-dir parent '%s'\n", path, ppath);
  }
  
  free(ppath);
}

static int fill_link_nodes(struct index_parser_state *ipstate) {
  /* create missing (implicit) entries */
  struct cmissing_state cmstate;
  /* spool new entries in a hash */
  cmstate.newentries = g_hash_table_new(g_str_hash, g_str_equal);
  cmstate.ipstate = ipstate;
  g_hash_table_foreach(tarixfs.fnhash, create_implicit_entries, &cmstate);
  /* copy over new entries into master hash */
  g_hash_table_foreach(cmstate.newentries, copy_entries, tarixfs.fnhash);
  g_hash_table_destroy(cmstate.newentries);
  
  /* link up all the directory contents */
  g_hash_table_foreach(tarixfs.fnhash, link_entries, NULL);
  
  return 0;
}

static int tarix_getattr(const char *path, struct stat *stbuf)
{
  memset(stbuf, 0, sizeof(struct stat));
  struct index_node *node = find_node(path);
  if (node == NULL)
    return -ENOENT;
  int res;
  if (!is_node_stat_filled(node))
    if ((res = fill_node_stat(node)) != 0)
      return res;
  memcpy(stbuf, &node->stbuf, sizeof(*stbuf));
  return 0;
}

static int tarix_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi) {
  struct index_node *node = find_node(path);
  if (node == NULL)
    return -ENOENT;
  
  int res;
  if (!is_node_stat_filled(node))
    if ((res = fill_node_stat(node)) != 0)
      return res;
  
  if (!is_node_children_filled(node))
      return -EIO;
  
  filler(buf, ".", &node->stbuf, 0);
  /* this may be available, but not worth the effort to dig up here */
  filler(buf, "..", NULL, 0);
  
  struct index_node *child = node->child;
  while (child != NULL) {
    filler(buf, strrchr(child->entry.filename, '/') + 1, &child->stbuf, 0);
    child = child->next;
  }
  
  return 0;
}

/*
static int tarix_open(const char *path, struct fuse_file_info *fi)
{
    if(strcmp(path, tarix_path) != 0)
        return -ENOENT;

    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;
    return 0;
}

static int tarix_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
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
    return size;
}
*/

static struct fuse_operations tarix_oper = {
  .getattr = tarix_getattr,
  .readdir = tarix_readdir,
//  .open = tarix_open,
//  .read = tarix_read,
};

static struct fuse_operations null_oper = { };

enum tarix_opt_keys {
  TARIX_KEY_ZLIB = 1,
  TARIX_KEY_HELP = 2,
};

#define TARIX_OPT(t, p, v) { t, offsetof(struct tarixfs_t, p), v }
static struct fuse_opt tarix_opts[] = {
/*
  TARIX_OPT("tarix_str=%s", tarix_str, 0),
*/
  TARIX_OPT("tar=%s", tarfilename, 0),
  TARIX_OPT("tarix=%s", indexfilename, 0),
  FUSE_OPT_KEY("zlib", TARIX_KEY_ZLIB),
  FUSE_OPT_KEY("--help", TARIX_KEY_HELP),
  FUSE_OPT_KEY("-h", TARIX_KEY_HELP),
  FUSE_OPT_END
};

int tarix_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
  switch (key) {
    case FUSE_OPT_KEY_OPT:
      return 1;
    case FUSE_OPT_KEY_NONOPT:
      return 1;
    case TARIX_KEY_ZLIB:
      tarixfs.use_zlib = 1;
      return 0;
      break;
    case TARIX_KEY_HELP:
      tarixfs.flags_norun |= TARIX_KEY_HELP;
      fuse_opt_add_arg(outargs, "-ho");
      return 0;
    default:
      fprintf(stderr, "wtf? key=%d\n", key);
      return 1;
  }
}

int index_lineloop(char *line, void *data) {
  struct index_parser_state *ipstate = (struct index_parser_state*)data;
  
  if (ipstate->version < 0) {
    if (init_index_parser(ipstate, line) != 0)
      return 1;
  } else {
    struct index_node *node = calloc(1, sizeof(*node));
    /* make sure all the pointers in *node are NULL */
    if (parse_index_line(ipstate, line, &node->entry) != 0)
      return 1;
    /* ensure directory entries are correct: not /-terminated */
    int namelen = strlen(node->entry.filename);
    /* could merge the case of no leading but a trailing slash and get fewer allocs */
    if (node->entry.filename[namelen-1] == '/') {
      node->entry.filename[namelen-1] = 0;
      /* node that it's a directory */
      node->stbuf.st_mode |= S_IFDIR;
    }
    if (node->entry.filename[0] != '/') {
      char *sfilename = malloc(strlen(node->entry.filename) + 2);
      sfilename[0] = '/';
      strcpy(sfilename+1, node->entry.filename);
      free(node->entry.filename);
      node->entry.filename = sfilename;
    }
//if (node->stbuf.st_mode & S_IFDIR) fprintf(stderr, "INFO: inferred directoryness on '%s'\n", node->entry.filename);
    g_hash_table_insert(tarixfs.fnhash, node->entry.filename, node);
  }
  return 0;
}

static void usage() {
  /*TODO*/
}

int main(int argc, char *argv[])
{
  int tarfd, indexfd;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  
  if (fuse_opt_parse(&args, &tarixfs, tarix_opts, tarix_opt_proc) == -1)
    return 1;
  
  if (tarixfs.flags_norun) {
    if (tarixfs.flags_norun & TARIX_KEY_HELP)
      usage();
    return fuse_main(args.argc, args.argv, &null_oper, NULL);
  }
  
  /* open the index file */
  if ((indexfd = open(tarixfs.indexfilename, O_RDONLY, 0666)) < 0) {
    perror("open indexfile");
    return 1;
  }
  
  if ((tarfd = open(tarixfs.tarfilename, O_RDONLY|P_O_LARGEFILE)) < 0) {
    perror("open tarfile");
    return 1;
  }
  
  /* tstream handles base offset */
  tarixfs.tsp = init_trs(NULL, tarfd, 0, TARBLKSZ, tarixfs.use_zlib);
  if (tarixfs.tsp->zlib_err != Z_OK) {
    fprintf(stderr, "zlib init error: %d\n", tarixfs.tsp->zlib_err);
    return 1;
  }
  
  /* init the hash table */
  tarixfs.fnhash = g_hash_table_new(g_str_hash, g_str_equal);
  
  /* read the index, load all the entries into the hash table */
  struct index_parser_state ipstate;
  ipstate.version = -1;
  ipstate.allocate_filename = 1;
  if (lineloop(indexfd, index_lineloop, &ipstate) != 0)
    return 1;
  
  /* scan through all the entries, create any implicit directory entries */
  if (fill_link_nodes(&ipstate) != 0)
    return 1;
  
  if (close(indexfd) != 0) {
    perror("close indexfile");
    return 1;
  }
  
  return fuse_main(args.argc, args.argv, &tarix_oper, NULL);
}
