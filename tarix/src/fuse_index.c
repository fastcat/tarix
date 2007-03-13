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

struct cmissing_state {
  GHashTable *newentries;
  struct index_parser_state *ipstate;
};


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
  /* handle (skip) long name and link records */
  while (tarhdr.header.typeflag == GNUTYPE_LONGNAME
      || tarhdr.header.typeflag == GNUTYPE_LONGLINK) {
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

static off64_t get_node_effective_offset(struct index_node *node) {
  switch (node->entry.version) {
    case 0:
      return node->entry.ioffset * TARBLKSZ;
      break;
    case 1:
      if (tarixfs.use_zlib)
        return node->entry.zoffset;
      else
        return node->entry.ioffset * TARBLKSZ;
      break;
    default:
fprintf(stderr, "Unknown version %d in record '%s', how did it get past?\n", node->entry.version, node->entry.filename);
      return -1;
  }
}
