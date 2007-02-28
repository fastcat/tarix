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

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <glib.h>

#include "lineloop.h"
#include "index_parser.h"
#include "tstream.h"
#include "tar.h"

/* node type in the index */
enum nodetype {
  /* haven't yet examined the node */
  FT_UNPROCESSED = 0,
  FT_FILE,
  FT_DIRECTORY,
  FT_OTHER,
};

struct index_node {
  struct index_entry entry;
  enum nodetype type;
  struct index_node *next;
  struct index_node *child;
};

struct tarixfs_t {
  char *tarfilename;
  char *indexfilename;
  int use_zlib;
  /* tar(.gz) stream */
  t_streamp tsp;
  /* hash of all filenames to corresponding index_nodes */
  GHashTable *fnhash;
};

static struct tarixfs_t tarixfs;

/*
static int tarix_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
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
    return res;
}

static int tarix_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, tarix_path + 1, NULL, 0);

    return 0;
}

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
/*
    .getattr	= tarix_getattr,
    .readdir	= tarix_readdir,
    .open	= tarix_open,
    .read	= tarix_read,
*/
};

enum tarix_opt_keys {
  TARIX_KEY_ZLIB = 1,
};

#define TARIX_OPT(t, p, v) { t, offsetof(struct tarixfs_t, p), v }
static struct fuse_opt tarix_opts[] = {
/*
  TARIX_OPT("tarix_str=%s", tarix_str, 0),
*/
  TARIX_OPT("tar=%s", tarfilename, 0),
  TARIX_OPT("tarix=%s", indexfilename, 0),
  FUSE_OPT_KEY("zlib", TARIX_KEY_ZLIB),
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
    struct index_node *node = (struct index_node*)malloc(sizeof(*node));
    /* make sure all the pointers in *node are NULL */
    memset(node, 0, sizeof(*node));
    if (parse_index_line(ipstate, line, &node->entry) != 0)
      return 1;
    g_hash_table_insert(tarixfs.fnhash, node->entry.filename, node);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  int tarfd, indexfd;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  
  if (fuse_opt_parse(&args, &tarixfs, tarix_opts, tarix_opt_proc) == -1)
    return 1;
  
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
  
  if (close(indexfd) != 0) {
    perror("close indexfile");
    return 1;
  }
  
  return fuse_main(args.argc, args.argv, &tarix_oper, NULL);
}
