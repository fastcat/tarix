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


// helper code related to in memory tar index is in a secondary file
#include "fuse_index.c"

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
  
  if (!(node->stbuf.st_mode & S_IFDIR))
    return -ENOTDIR;
  
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

static int tarix_open(const char *path, struct fuse_file_info *fi) {
  struct index_node *node = find_node(path);
  if (node == NULL)
    return -ENOENT;
  if ((fi->flags & 3) != O_RDONLY)
    return -EACCES;
  return 0;
}

static int tarix_read(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi) {
  union tar_block theader;
  int res;
  
  //TODO: cache state in the tar fd so we don't keep re-reading the beginning
  // of a file over and over again
  
  struct index_node *node = find_node(path);
  if (node == NULL)
    return -ENOENT;
  
  off64_t nodeoffset;
  switch (node->entry.version) {
    case 0:
      nodeoffset = node->entry.ioffset * TARBLKSZ;
      break;
    case 1:
      nodeoffset = node->entry.zoffset;
      break;
    default:
fprintf(stderr, "Unknown version %d in record '%s', how did it get past?\n", node->entry.version, node->entry.filename);
      return -EIO;
  }
  
  if (ts_seek(tarixfs.tsp, nodeoffset) != 0) {
fprintf(stderr, "seek error for initial tar header in record '%s'\n", node->entry.filename);
    return -EIO;
  }
  
  if ((res = ts_read(tarixfs.tsp, &theader, TARBLKSZ)) != TARBLKSZ) {
fprintf(stderr, "read error for initial tar header in record '%s'\n", node->entry.filename);
    return -EIO;
  }
  
  if (theader.header.typeflag == GNUTYPE_LONGNAME
      || theader.header.typeflag == GNUTYPE_LONGLINK) {
    // skip the text
    if ((res = ts_read(tarixfs.tsp, &theader, TARBLKSZ)) != TARBLKSZ) {
fprintf(stderr, "read error skipping long link/name in record '%s'\n", node->entry.filename);
      return -EIO;
    }
    // read the real header
    if ((res = ts_read(tarixfs.tsp, &theader, TARBLKSZ)) != TARBLKSZ) {
fprintf(stderr, "read error skipping long link/name (#2) in record '%s'\n", node->entry.filename);
      return -EIO;
    }
  }
  
  if (theader.header.typeflag != REGTYPE && theader.header.typeflag != AREGTYPE) {
    // can only read from regular files
    return -EIO;
  }
  
  // use the buffer in the tar header to skip data
  //TODO: seek on normal files
  
  // skip to the desired data
  while (offset > 0) {
    res = ts_read(tarixfs.tsp, theader.buffer, TARBLKSZ);
    if (res < 1) {
fprintf(stderr, "pseudo-seek read error in record '%s'\n", node->entry.filename);
      return -EIO;
    }
    offset -= res;
  }
  
  // read the desired data
  res = ts_read(tarixfs.tsp, buf, size);
  return res;
}


static struct fuse_operations tarix_oper = {
  .getattr = tarix_getattr,
  .readdir = tarix_readdir,
  .open = tarix_open,
  .read = tarix_read,
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
